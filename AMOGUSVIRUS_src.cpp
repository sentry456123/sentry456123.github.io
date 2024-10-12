#include <windows.h>
#include <tchar.h>
#include <random>


#include <d3d11.h>
#pragma comment (lib, "D3D11")
#include <d2d1.h>
#pragma comment (lib, "D2d1")
#include <d2d1_1.h>
#include <d2d1_3.h>
#include <dxgi1_2.h>

#include <dcomp.h>
#pragma comment(lib, "dcomp")

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")


#pragma comment(lib, "Winmm.lib")

static HINSTANCE hInst;
static int nWidth = 800, nHeight = 400;
#define IDC_STATIC 10
#define IDC_BUTTON 11

static ID2D1Factory1* m_pD2DFactory1 = nullptr;
static ID3D11Device* m_pD3D11Device = nullptr;
static ID3D11DeviceContext* m_pD3D11DeviceContext = nullptr;
static IDXGIDevice1* m_pDXGIDevice = nullptr;
static ID2D1Device* m_pD2DDevice = nullptr;
static ID2D1DeviceContext3* m_pD2DDeviceContext3 = nullptr;
static IDXGISwapChain1* m_pDXGISwapChain1 = nullptr;
static ID2D1Bitmap1* m_pD2DTargetBitmap = nullptr;

static ID2D1SolidColorBrush* m_pD2DBrushBlack = nullptr;
static ID2D1SolidColorBrush* m_pD2DBrushWhite = nullptr;
static ID2D1SolidColorBrush* m_pD2DBrushBlue = nullptr;
static ID2D1SolidColorBrush* m_pD2DBrushGreen = nullptr;

static IDCompositionDevice* m_pDCompositionDevice = nullptr;
static IDCompositionTarget* m_pDCompositionTarget = nullptr;


static IWICImagingFactory* m_pWICFactory = nullptr;
static ID2D1Bitmap* m_pBitmap = nullptr;


template <class T> static void SafeRelease(T** ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}

static DWORD WINAPI SoundThread(LPVOID lpParam) {
    while (true) {
        PlaySound(TEXT("amogus.wav"), nullptr, SND_FILENAME | SND_NODEFAULT);
    }
}

static HRESULT LoadBitmapFromFile(const WCHAR* uri, ID2D1Bitmap** ppBitmap)
{
    HRESULT hr = S_OK;

    IWICBitmapDecoder* pDecoder = nullptr;
    IWICBitmapFrameDecode* pSource = nullptr;
    IWICStream* pStream = nullptr;
    IWICFormatConverter* pConverter = nullptr;

    hr = m_pWICFactory->CreateDecoderFromFilename(uri, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
    
    if (SUCCEEDED(hr))
    {
        hr = pDecoder->GetFrame(0, &pSource);
    }
    if (SUCCEEDED(hr))
    {
        hr = m_pWICFactory->CreateFormatConverter(&pConverter);
    }
    if (SUCCEEDED(hr))
    {
        hr = pConverter->Initialize(
            pSource,
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.f,
            WICBitmapPaletteTypeMedianCut
        );
    }
    if (SUCCEEDED(hr))
    {
        hr = m_pD2DDeviceContext3->CreateBitmapFromWicBitmap(pConverter, nullptr, ppBitmap);
    }

    SafeRelease(&pDecoder);
    SafeRelease(&pSource);
    SafeRelease(&pStream);
    SafeRelease(&pConverter);

    return hr;
}

static void RenderImage()
{
    if (m_pBitmap)
    {
        // Set blend mode for transparency
        m_pD2DDeviceContext3->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);

        D2D1_RECT_F destRect = D2D1::RectF(0.0f, 0.0f, m_pBitmap->GetPixelSize().width, m_pBitmap->GetPixelSize().height); // Example destination rectangle

        // Draw the bitmap onto the render target
        m_pD2DDeviceContext3->DrawBitmap(m_pBitmap, destRect);
    }
}


static HRESULT CreateD2D1Factory()
{
    HRESULT hr = S_OK;
    D2D1_FACTORY_OPTIONS options;
    ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));
    options.debugLevel = D2D1_DEBUG_LEVEL::D2D1_DEBUG_LEVEL_INFORMATION;
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options, (void**)&m_pD2DFactory1);
    return hr;
}


// https://docs.microsoft.com/en-us/windows/win32/direct2d/devices-and-device-contexts
static HRESULT CreateD3D11Device()
{
    HRESULT hr = S_OK;
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;

    // This array defines the set of DirectX hardware feature levels this app  supports.
    // The ordering is important and you should  preserve it.
    // Don't forget to declare your app's minimum required feature level in its
    // description.  All apps are assumed to support 9.1 unless otherwise stated.
    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };
    D3D_FEATURE_LEVEL featureLevel;
    hr = D3D11CreateDevice(
        nullptr,                    // specify null to use the default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        0,
        creationFlags,              // optionally set debug and Direct2D compatibility flags
        featureLevels,              // list of feature levels this app can support
        ARRAYSIZE(featureLevels),   // number of possible feature levels
        D3D11_SDK_VERSION,
        &m_pD3D11Device,                    // returns the Direct3D device created
        &featureLevel,            // returns feature level of device created
        &m_pD3D11DeviceContext                    // returns the device immediate context
    );
    if (SUCCEEDED(hr))
    {
        // Obtain the underlying DXGI device of the Direct3D11 device.
        hr = m_pD3D11Device->QueryInterface((IDXGIDevice1**)&m_pDXGIDevice);
        if (SUCCEEDED(hr))
        {
            // Obtain the Direct2D device for 2-D rendering.
            hr = m_pD2DFactory1->CreateDevice(m_pDXGIDevice, &m_pD2DDevice);
            if (SUCCEEDED(hr))
            {
                // Get Direct2D device's corresponding device context object.
                ID2D1DeviceContext* pD2DDeviceContext = nullptr;
                hr = m_pD2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &pD2DDeviceContext);
                if (SUCCEEDED(hr))
                    hr = pD2DDeviceContext->QueryInterface((ID2D1DeviceContext3**)&m_pD2DDeviceContext3);
                SafeRelease(&pD2DDeviceContext);
            }
        }
    }
    return hr;
}


static HRESULT CreateDeviceResources()
{
    HRESULT hr = S_OK;
    if (m_pD2DDeviceContext3)
    {
        hr = m_pD2DDeviceContext3->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 0.5f), &m_pD2DBrushBlack);
        hr = m_pD2DDeviceContext3->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 0.5f), &m_pD2DBrushWhite);
        hr = m_pD2DDeviceContext3->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue, 1.0f), &m_pD2DBrushBlue);
        hr = m_pD2DDeviceContext3->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Green, 1.0f), &m_pD2DBrushGreen);
    }

    CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&m_pWICFactory)
    );

    return hr;
}

static HRESULT CreateSwapChain(HWND hWnd)
{
    HRESULT hr = S_OK;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};
    swapChainDesc.Width = 1;
    swapChainDesc.Height = 1;
    swapChainDesc.Format = DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.Stereo = false;
    swapChainDesc.SampleDesc.Count = 1; // don't use multi-sampling
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2; // use double buffering to enable flip
    swapChainDesc.Scaling = (hWnd != nullptr) ? DXGI_SCALING::DXGI_SCALING_NONE : DXGI_SCALING::DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.Flags = 0;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    IDXGIAdapter* pDXGIAdapter = nullptr;
    hr = m_pDXGIDevice->GetAdapter(&pDXGIAdapter);
    if (SUCCEEDED(hr))
    {
        IDXGIFactory2* pDXGIFactory2 = nullptr;
        hr = pDXGIAdapter->GetParent(IID_PPV_ARGS(&pDXGIFactory2));
        if (SUCCEEDED(hr))
        {
            if (hWnd != nullptr)
            {
                hr = pDXGIFactory2->CreateSwapChainForHwnd(m_pD3D11Device, hWnd, &swapChainDesc, nullptr, nullptr, &m_pDXGISwapChain1);
            } else
            {
                hr = pDXGIFactory2->CreateSwapChainForComposition(m_pD3D11Device, &swapChainDesc, nullptr, &m_pDXGISwapChain1);
            }
            if (SUCCEEDED(hr))
                hr = m_pDXGIDevice->SetMaximumFrameLatency(1);
            SafeRelease(&pDXGIFactory2);
        }
        SafeRelease(&pDXGIAdapter);
    }
    return hr;
}

static HRESULT ConfigureSwapChain(HWND hWnd)
{
    HRESULT hr = S_OK;

    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        0,
        0,
        nullptr
    );

    unsigned int nDPI = GetDpiForWindow(hWnd);
    bitmapProperties.dpiX = nDPI;
    bitmapProperties.dpiY = nDPI;

    IDXGISurface* pDXGISurface;
    if (m_pDXGISwapChain1)
    {
        hr = m_pDXGISwapChain1->GetBuffer(0, IID_PPV_ARGS(&pDXGISurface));
        if (SUCCEEDED(hr))
        {
            hr = m_pD2DDeviceContext3->CreateBitmapFromDxgiSurface(pDXGISurface, bitmapProperties, &m_pD2DTargetBitmap);
            if (SUCCEEDED(hr))
            {
                m_pD2DDeviceContext3->SetTarget(m_pD2DTargetBitmap);
            }
            SafeRelease(&pDXGISurface);
        }
    }
    return hr;
}


static HRESULT CreateDirectComposition(HWND hWnd)
{
    HRESULT hr = S_OK;
    hr = DCompositionCreateDevice(m_pDXGIDevice, __uuidof(m_pDCompositionDevice), (void**)(&m_pDCompositionDevice));
    if (SUCCEEDED(hr))
    {
        hr = m_pDCompositionDevice->CreateTargetForHwnd(hWnd, true, &m_pDCompositionTarget);
        if (SUCCEEDED(hr))
        {
            IDCompositionVisual* pDCompositionVisual = nullptr;
            hr = m_pDCompositionDevice->CreateVisual(&pDCompositionVisual);
            if (SUCCEEDED(hr))
            {
                hr = pDCompositionVisual->SetContent(m_pDXGISwapChain1);
                hr = m_pDCompositionTarget->SetRoot(pDCompositionVisual);
                hr = m_pDCompositionDevice->Commit();
                SafeRelease(&pDCompositionVisual);
            }
        }
    }
    return hr;
}


static void OnResize(HWND hWnd, UINT nWidth, UINT nHeight)
{
    if (m_pDXGISwapChain1)
    {
        HRESULT hr = S_OK;
        if (nWidth != 0 && nHeight != 0)
        {
            m_pD2DDeviceContext3->SetTarget(nullptr);
            SafeRelease(&m_pD2DTargetBitmap);
            hr = m_pDXGISwapChain1->ResizeBuffers(
                2, // Double-buffered swap chain.
                nWidth,
                nHeight,
                DXGI_FORMAT_B8G8R8A8_UNORM,
                0
            );
            if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
            {
                CreateD3D11Device();
                CreateSwapChain(nullptr);
                return;
            } else
            {
                //DX::ThrowIfFailed(hr);
            }
            ConfigureSwapChain(hWnd);
        }
    }
}


static void CleanDeviceResources()
{
    SafeRelease(&m_pD2DBrushBlack);
    SafeRelease(&m_pD2DBrushWhite);
    SafeRelease(&m_pD2DBrushBlue);
    SafeRelease(&m_pD2DBrushGreen);
}

static void Clean()
{
    SafeRelease(&m_pD2DDevice);
    SafeRelease(&m_pD2DDeviceContext3);
    SafeRelease(&m_pD2DTargetBitmap);
    CleanDeviceResources();
    SafeRelease(&m_pDXGISwapChain1);
    SafeRelease(&m_pDXGIDevice);
    SafeRelease(&m_pD3D11Device);
    SafeRelease(&m_pD3D11DeviceContext);
    SafeRelease(&m_pD2DFactory1);

    SafeRelease(&m_pDCompositionDevice);
    SafeRelease(&m_pDCompositionTarget);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    switch (message)
    {
    case WM_CREATE:
    {
        HRESULT hr = CoInitialize(nullptr);
        if (SUCCEEDED(hr))
        {
            hr = CreateD2D1Factory();

            if (SUCCEEDED(hr))
            {
                hr = CreateD3D11Device();
                hr = CreateDeviceResources();
                hr = CreateSwapChain(nullptr);

                LoadBitmapFromFile(L"amogus.png", &m_pBitmap);
                nWidth = m_pBitmap->GetPixelSize().width;
                nHeight = m_pBitmap->GetPixelSize().height;
                OnResize(hWnd, nWidth, nHeight);

                if (SUCCEEDED(hr))
                {
                    hr = ConfigureSwapChain(hWnd);
                    hr = CreateDirectComposition(hWnd);
                }
            }
        }
        return 0;
    }
    case WM_PAINT:
    {
        HRESULT hr = S_OK;
        if (m_pD2DDeviceContext3 && m_pDXGISwapChain1)
        {
            m_pD2DDeviceContext3->BeginDraw();
            D2D1_SIZE_F size = m_pD2DDeviceContext3->GetSize();
            //m_pD2DDeviceContext3->Clear(D2D1::ColorF(D2D1::ColorF::Red, 0.5f));
            //m_pD2DDeviceContext3->FillRectangle(D2D1::RectF(0.f, 0.f, size.width / 2, size.height), m_pD2DBrushBlack);
            //m_pD2DDeviceContext3->FillRectangle(D2D1::RectF(size.width / 2, 0.f, size.width, size.height), m_pD2DBrushWhite);
            //m_pD2DDeviceContext3->FillRectangle(D2D1::RectF(0.f, 0.f, size.width / 6, size.height / 3), m_pD2DBrushBlue);
            //m_pD2DDeviceContext3->FillRectangle(D2D1::RectF(size.width - size.width / 6, size.height - size.height / 3, size.width, size.height), m_pD2DBrushGreen);

            RenderImage();

            hr = m_pD2DDeviceContext3->EndDraw();
            hr = m_pDXGISwapChain1->Present(1, 0);
        }
        return 0;
    }
    case WM_SIZE:
    {
        UINT nWidth = LOWORD(lParam);
        UINT nHeight = HIWORD(lParam);
        OnResize(hWnd, nWidth, nHeight);
        return 0;
    }
    case WM_DESTROY:
    {
        Clean();
        CoUninitialize();
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}



int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    hInst = hInstance;
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof wcex;
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInst;
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = TEXT("WindowClass");

    if (!RegisterClassEx(&wcex)) {
        return MessageBox(nullptr, TEXT("Cannot register class !"), TEXT("Error"), MB_ICONERROR | MB_OK);
    }
    int nX = (GetSystemMetrics(SM_CXSCREEN) - nWidth) / 2;
    int nY = (GetSystemMetrics(SM_CYSCREEN) - nHeight) / 2;
    HWND hWnd = CreateWindowEx(WS_EX_NOREDIRECTIONBITMAP, wcex.lpszClassName, TEXT("AMOGUS VIRUS"), 0, nX, nY, nWidth, nHeight, nullptr, nullptr, hInst, nullptr);
    if (!hWnd) {
        return MessageBox(nullptr, TEXT("Cannot create window !"), TEXT("Error"), MB_ICONERROR | MB_OK);
    }
    SetWindowLong(hWnd, GWL_STYLE, 0);
    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);
    MSG msg;

            HRESULT hr = CoInitialize(nullptr);
        if (SUCCEEDED(hr))
        {
            hr = CreateD2D1Factory();

            if (SUCCEEDED(hr))
            {
                hr = CreateD3D11Device();
                hr = CreateDeviceResources();
                hr = CreateSwapChain(nullptr);

                LoadBitmapFromFile(L"amogus.png", &m_pBitmap);
                nWidth = m_pBitmap->GetPixelSize().width;
                nHeight = m_pBitmap->GetPixelSize().height;
                OnResize(hWnd, nWidth, nHeight);

                if (SUCCEEDED(hr))
                {
                    hr = ConfigureSwapChain(hWnd);
                    hr = CreateDirectComposition(hWnd);
                }
            }
        }


        DWORD dump;
        CreateThread( 
            NULL,                   // default security attributes
            0,                      // use default stack size  
            SoundThread,       // thread function name
            nullptr,          // argument to thread function 
            0,                      // use default creation flags 
            &dump);   // returns the thread identifier 


        int tick =0;
        int x=0,y=0;

        std::random_device rd;
        std::mt19937 gen(rd());


        int left = 0;
        int top = 0;
        int right = GetSystemMetrics(SM_CXSCREEN)-300;
        int bottom = GetSystemMetrics(SM_CYSCREEN)-300;

        constexpr int SPEED = 10;


        x=rd()%(right);
        y=rd()%(bottom);


        bool going_right = rd() % 2;
        bool going_bottom = rd() % 2;


        while (true) {





            SetWindowPos(hWnd, nullptr, x, y, 300, 400, 0);


            if (going_right) {
                x += SPEED;
            }
            else {
                x -= SPEED;
            }

            if (going_bottom) {
                y += SPEED;
            }
            else{
                y -= SPEED;
            }


            if (x < left) {
                going_right = true;
            }
            else if (x > right) {
                going_right = false;
            }
            if (y < top) {
                going_bottom = true;
            }
            else if (y > bottom) {
                going_bottom = false;
            }


                HRESULT hr = S_OK;
        if (m_pD2DDeviceContext3 && m_pDXGISwapChain1)
        {
            m_pD2DDeviceContext3->BeginDraw();
            D2D1_SIZE_F size = m_pD2DDeviceContext3->GetSize();
            //m_pD2DDeviceContext3->Clear(D2D1::ColorF(D2D1::ColorF::Red, 0.5f));
            //m_pD2DDeviceContext3->FillRectangle(D2D1::RectF(0.f, 0.f, size.width / 2, size.height), m_pD2DBrushBlack);
            //m_pD2DDeviceContext3->FillRectangle(D2D1::RectF(size.width / 2, 0.f, size.width, size.height), m_pD2DBrushWhite);
            //m_pD2DDeviceContext3->FillRectangle(D2D1::RectF(0.f, 0.f, size.width / 6, size.height / 3), m_pD2DBrushBlue);
            //m_pD2DDeviceContext3->FillRectangle(D2D1::RectF(size.width - size.width / 6, size.height - size.height / 3, size.width, size.height), m_pD2DBrushGreen);

            RenderImage();

            hr = m_pD2DDeviceContext3->EndDraw();
            hr = m_pDXGISwapChain1->Present(1, 0);
        }

        Sleep(10);
        tick++;

        }

    return (int)msg.wParam;
}

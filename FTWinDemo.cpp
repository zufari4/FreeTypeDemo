//-----------------------------------------------------------------//
//                                                                 //
//    This is an exaple of program to output text with             //
//    FreeType library and pure WinAPI.                            //
//                                                                 //
//    Compile command:                                             //
//      g++ FTWinDemo.cpp -o FTWinDemo.exe -mwindows -lfreetype.   //
//                                                                 //
//    Written by Ruslan Yusupov.                                   //
//                                                                 //
//    Public domain.                                               //
//-----------------------------------------------------------------//

#include <stdio.h>
#include <time.h>
#include <math.h>
#include <windows.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_LCD_FILTER_H

#define APP_WIDTH   800
#define APP_HEIGHT  520
#define FONT_FILE   "c:/windows/fonts/verdana.ttf"
#define FONT_SIZE   10
#define LRESC       LRESULT CALLBACK

const COLORREF BACK_COLOR = RGB(240,240,240);
const COLORREF FORE_COLOR = RGB(0,0,0);

const wchar_t* DRAW_TEXT =
   L"Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Cras sit amet"
    " dui.  Nam sapien. Fusce vestibulum ornare metus. Maecenas ligula orci,"
    " consequat vitae, dictum nec, lacinia non, elit. Aliquam iaculis"
    " molestie neque. Maecenas suscipit felis ut pede convallis malesuada."
    " Aliquam erat volutpat. Nunc pulvinar condimentum nunc. Donec ac sem vel"
    " leo bibendum aliquam. Pellentesque habitant morbi tristique senectus et"
    " netus et malesuada fames ac turpis egestas.\n"
    "\n"
    "Sed commodo. Nulla ut libero sit amet justo varius blandit. Mauris vitae"
    " nulla eget lorem pretium ornare. Proin vulputate erat porta risus."
    " Vestibulum malesuada, odio at vehicula lobortis, nisi metus hendrerit"
    " est, vitae feugiat quam massa a ligula. Aenean in tellus. Praesent"
    " convallis. Nullam vel lacus.  Aliquam congue erat non urna mollis"
    " faucibus. Morbi vitae mauris faucibus quam condimentum ornare. Quisque"
    " sit amet augue. Morbi ullamcorper mattis enim. Aliquam erat volutpat."
    " Morbi nec felis non enim pulvinar lobortis.  Ut libero. Nullam id orci"
    " quis nisl dapibus rutrum. Suspendisse consequat vulputate leo. Aenean"
    " non orci non tellus iaculis vestibulum. Sed neque.";

typedef struct _Pixel
{
    union {
        struct {
            BYTE b, g, r, a;
        };
        long val;
    };
} Pixel;

typedef struct _Bitmap
{
    BITMAPINFO  bmi;
    UINT        width;
    UINT        height;
    HBITMAP     hbmp;
    HBITMAP     hprev;
    Pixel*      pixels;
    HDC         hdc;
} Bitmap;

typedef struct _AppInfo
{
    HWND        window;
    FT_Library  library;
    FT_Face     face;
    HDC         hdc;
    HFONT       font;
    UINT        width;
    UINT        height;
    bool        kerning;
    Bitmap      bitmap;
} AppInfo;

bool   LoadApplication(AppInfo*);
void   FreeApplication(AppInfo*);
LRESC  WndProc(HWND, UINT, WPARAM, LPARAM);
HWND   CreateWnd(const char*,int, int, COLORREF);
void   RunMessageLoop();
void   SetCenter(HWND);
bool   AssignWindow(AppInfo*);
int    GetDpi();
void   OnPaint(AppInfo*);
void   OnResize(AppInfo*, int, int);
void   RenderText(const wchar_t*, FT_Face, bool, COLORREF, Bitmap*);
void   BlitGlyph(FT_Bitmap*, FT_Int, FT_Int, Bitmap*, COLORREF);
void   DrawLine(HDC, int, int, int, int);
void   ShowError(const char*, ...);
FT_Pos dtofix(const double&);
double fixtod(const FT_Pos);
bool   CreateBitmap(Bitmap*, int, int, COLORREF);
void   FreeBitmap(Bitmap*);
void   DrawBitmap(HDC, Bitmap*);
void   ClearBitmap(Bitmap*, COLORREF);
void   ResizeBitmap(Bitmap*, UINT, UINT);

//--------------------------------
// The entry point to the program
//--------------------------------
int main()
{
    AppInfo  app;
    if ( LoadApplication(&app) )
        RunMessageLoop();
    FreeApplication(&app);

    return 0;
}

//--------------------
// Load application
//--------------------
bool LoadApplication(AppInfo* app)
{
    memset(app, 0, sizeof(AppInfo));

    app->window = CreateWnd("FreeType demo", APP_WIDTH, APP_HEIGHT, BACK_COLOR);
    if (app->window == NULL) {
        ShowError("Window not create\n");
        return false;
    }

    SetCenter(app->window);

    RECT rect;
    GetClientRect(app->window, &rect);
    app->width   = rect.right -rect.left;
    app->height  = rect.bottom-rect.top;

    bool res = CreateBitmap(&app->bitmap, app->width/2, app->height, BACK_COLOR);
    if (!res) {
        ShowError("Bitmap not created\n");
        return false;
    }

    FT_Error error = FT_Init_FreeType(&app->library);
    if (error) {
        ShowError("Freetype not initialized: %d\n", error);
        return false;
    }

    error = FT_Library_SetLcdFilter(app->library, FT_LCD_FILTER_DEFAULT);
    if (error) {
        ShowError("Not set LCD filter: %d\n", error);
    }

    error = FT_New_Face(app->library, FONT_FILE, 0, &app->face);
    if (error) {
        ShowError("Font not load: %d\n", error);
        return false;
    }

    error = FT_Set_Char_Size(app->face, FONT_SIZE * 64, 0, GetDpi(), 0);
    if (error) {
        ShowError("Cant set char size: %d\n", error);
        return false;
    }

    app->kerning    = FT_HAS_KERNING( app->face );

    app->font = CreateFont(-MulDiv(FONT_SIZE, GetDpi(), 72), 0, 0, 0, 0,
                           FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                           DEFAULT_PITCH, app->face->family_name);
    if (app->font)
        SendMessage(app->window, WM_SETFONT, (WPARAM)app->font, 1);
    else {
        ShowError("Cant create font\n");
        return false;
    }

    if ( !AssignWindow(app) ) {
        ShowError("Cant set window data\n");
        return false;
    }

    return true;
}

//-----------------------
// Free application data
//-----------------------
void FreeApplication(AppInfo* app)
{
    FreeBitmap(&app->bitmap);
    if (app->font) {
        DeleteObject(app->font);
        app->font = NULL;
    }
    if (app->face) {
        FT_Done_Face(app->face);
        app->face = NULL;
    }
    if (app->library) {
        FT_Done_FreeType(app->library);
        app->library = NULL;
    }
    if (app->window) {
        DestroyWindow(app->window);
        app->window = NULL;
    }
}

//---------------
// Create window
//---------------
HWND CreateWnd(const char* caption, int width, int height, COLORREF backColor)
{
    const char* className = "FTWinDemoClass";
    WNDCLASSEX wc;
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = sizeof(LPVOID);
    wc.hInstance     = (HINSTANCE) GetModuleHandle(NULL);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush( backColor );
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = className;
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

    if ( RegisterClassEx(&wc) == 0 )
        return NULL;

    return CreateWindow(className, caption, WS_OVERLAPPEDWINDOW|WS_VISIBLE, 0,0,
                        width, height, NULL, NULL, GetModuleHandle(NULL), NULL);
}

//--------------
// Message loop
//--------------
void RunMessageLoop()
{
    MSG msg;
    while ( GetMessage(&msg, NULL, 0, 0) > 0 ) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

//------------------------------------------
// Message processing for our Windows class
//------------------------------------------
LRESC WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_CLOSE:
            PostQuitMessage(0);
            break;

        case WM_PAINT: {
            AppInfo* app = (AppInfo*) GetWindowLongPtr( hwnd, GWLP_USERDATA );
            if (app) {
                PAINTSTRUCT ps;
                app->hdc = BeginPaint( hwnd, &ps );
                OnPaint( app );
                EndPaint( hwnd, &ps );
                return 0;
            }
        } break;

        case WM_SIZE: {
            AppInfo* app = (AppInfo*) GetWindowLongPtr( hwnd, GWLP_USERDATA );
            if (app) {
                OnResize(app, LOWORD(lParam), HIWORD(lParam));
                return 0;
            }
        } break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

//------------------------
// Event rendering window
//------------------------
void OnPaint(AppInfo* app)
{
    // Draw text using FreeType
    ClearBitmap( &app->bitmap, BACK_COLOR );
    RenderText( DRAW_TEXT, app->face, app->kerning, FORE_COLOR, &app->bitmap );
    DrawBitmap( app->hdc, &app->bitmap );

    // Draw devide line
    DrawLine( app->hdc, app->bitmap.width, 0, app->bitmap.width, app->height );

    // Draw text using WinAPI
    SelectObject( app->hdc, app->font );
    SetBkMode( app->hdc, TRANSPARENT );
    RECT rect;
    rect.left   = app->bitmap.width+1;
    rect.top    = 0;
    rect.bottom = app->height;
    rect.right  = app->width;
    DrawTextW( app->hdc, DRAW_TEXT, wcslen(DRAW_TEXT), &rect, DT_LEFT|DT_WORDBREAK );
}

//---------------------
// Event resize window
//---------------------
void OnResize(AppInfo* app, int width, int height)
{
    if (width > 0 && height > 0) {
        app->width  = width;
        app->height = height;
        ResizeBitmap( &app->bitmap, width/2, height );
    }
}

//--------------------------------
// Render FreeType text to bitmap
//--------------------------------
void RenderText(const wchar_t* text, FT_Face face, bool kerning, COLORREF color, Bitmap* bmp)
{
    FT_Error      error;
    int           numChars   = wcslen(text);
    FT_GlyphSlot  slot       = face->glyph;
    int           flags      = FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LCD  | FT_LOAD_RENDER;
    double        lineHeight = fixtod( face->size->metrics.height );
    double        bmpWidth   = bmp->width;
    double        bmpHeight  = bmp->height;
    FT_UInt       charIndex, prev = 0;
    FT_Matrix     matrix;
    FT_Vector     pen;
    FT_Vector     delta;
    matrix.xx = (FT_Fixed)( 0x10000L );
    matrix.xy = (FT_Fixed)(0);
    matrix.yx = (FT_Fixed)(0);
    matrix.yy = (FT_Fixed)( 0x10000L );
    pen.x = 0;
    pen.y = dtofix(bmpHeight - fixtod(face->size->metrics.ascender) );

    for (int n = 0; n < numChars; ++n)
    {
        if (text[n] == '\n') {
            pen.x  = 0;
            pen.y  = dtofix( fixtod(pen.y) - lineHeight );
            continue;
        }

        charIndex = FT_Get_Char_Index( face, text[n] );

        if ( kerning && charIndex && prev ) {
            FT_Get_Kerning( face, prev, charIndex, FT_KERNING_DEFAULT, &delta );
            pen.x  = dtofix( fixtod(pen.x) +  fixtod(delta.x) );
        }

        FT_Set_Transform( face, &matrix, &pen );

        error = FT_Load_Glyph( face, charIndex, flags );
        if (error)
            continue;

        if ( slot->bitmap_left + fixtod(slot->advance.x) > bmpWidth ) {
            pen.x  = 0;
            pen.y  = dtofix( fixtod(pen.y) - lineHeight );
            FT_Set_Transform( face, &matrix, &pen );
            FT_Load_Glyph( face, charIndex, flags );
            if (error)
                continue;
        }

        BlitGlyph(&slot->bitmap, slot->bitmap_left, bmpHeight-slot->bitmap_top, bmp, color);

        pen.x = dtofix( fixtod(pen.x) +  fixtod(slot->advance.x) );
        prev  = charIndex;
    }
}

//-------------------------------
// Blit FreeType glyph to bitmap
//-------------------------------
void BlitGlyph(FT_Bitmap* src, int sx, int sy, Bitmap* dst, COLORREF color)
{
    UINT    x, y, xo, yo, d;
    Pixel*  pixel;
    BYTE    sr, sg, sb, dr, dg, db;
    BYTE    cr = GetRValue(color);
    BYTE    cg = GetGValue(color);
    BYTE    cb = GetBValue(color);

    for (x = 0, xo = sx; x < src->width/3; ++x, ++xo)
        for (y = 0, yo = sy; y < src->rows; ++y, ++yo)
            if (xo >=0 && xo < dst->width && yo >=0 && yo < dst->height)
            {
                sr = src->buffer[y * src->pitch + x*3];
                sg = src->buffer[y * src->pitch + x*3 + 1];
                sb = src->buffer[y * src->pitch + x*3 + 2];

                if (RGB(sr, sg, sb) > 0) {
                    pixel = dst->pixels + yo * dst->width + xo;

                    dr = pixel->r;
                    dg = pixel->g;
                    db = pixel->b;

                    d = cr - dr;
                    dr += (unsigned char)((sr*d + 127)/255);

                    d = cg - dg;
                    dg += (unsigned char)((sg*d + 127)/255);

                    d = cb - db;
                    db += (unsigned char)((sb*d + 127)/255);

                    pixel->r = dr;
                    pixel->g = dg;
                    pixel->b = db;
                }
            }
}

//----------------------------
// Get DPI of primary monitor
//----------------------------
int GetDpi()
{
    int dpi;
    HDC hdc = GetDC(NULL);
    if (hdc) {
        dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(NULL, hdc);
    }
    else
        dpi = 96;
    return dpi;
}

//------------------
// Draw single line
//------------------
void DrawLine(HDC hdc,int x1, int y1, int x2, int y2)
{
    MoveToEx(hdc, x1, y1, (LPPOINT) NULL);
    LineTo(hdc, x2, y2);
}

//-------------------------------------------
// Move the window to center of your desktop
//-------------------------------------------
void SetCenter(HWND window)
{
    RECT rect;
    GetWindowRect(window, &rect);
    int width = rect.right - rect.left;
    int height= rect.bottom- rect.top;
    int scrnW = GetSystemMetrics(SM_CXSCREEN);
    int scrnH = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(window, NULL, scrnW/2 - width/2, scrnH/2 - height/2, 0, 0,
                 SWP_NOZORDER|SWP_NOSIZE);
}

//-----------------------------------
// Assign application data to window
//-----------------------------------
bool AssignWindow(AppInfo* app)
{
    SetLastError(0);
    SetWindowLongPtr( app->window, GWLP_USERDATA, (LONG)app );
    return GetLastError() == 0;
}

//---------------------------
// Show window with an error
//---------------------------
void ShowError(const char* fmt, ...)
{
    char     str[255];
    va_list  args;
    va_start (args, fmt);
    vsnprintf(str, sizeof(str), fmt, args);
    va_end   (args);
    MessageBoxA(NULL, str, "Error", MB_ICONERROR);
}

//---------------------------------
// Create device independet bitmap
//---------------------------------
bool CreateBitmap(Bitmap* bmp, int width, int height, COLORREF color)
{
    bmp->bmi.bmiHeader.biSize     = sizeof (BITMAPINFOHEADER);
    bmp->bmi.bmiHeader.biWidth    = width;
    bmp->bmi.bmiHeader.biHeight   = -height;
    bmp->bmi.bmiHeader.biPlanes   = 1;
    bmp->bmi.bmiHeader.biBitCount = 32;
    bmp->bmi.bmiHeader.biCompression   = BI_RGB;
    bmp->bmi.bmiHeader.biSizeImage     = 0;
    bmp->bmi.bmiHeader.biXPelsPerMeter = 0;
    bmp->bmi.bmiHeader.biYPelsPerMeter = 0;
    bmp->bmi.bmiHeader.biClrUsed       = 0;
    bmp->bmi.bmiHeader.biClrImportant  = 0;
    bmp->bmi.bmiColors[0].rgbBlue      = 0;
    bmp->bmi.bmiColors[0].rgbGreen     = 0;
    bmp->bmi.bmiColors[0].rgbRed       = 0;
    bmp->bmi.bmiColors[0].rgbReserved  = 0;

    HDC hdc   = GetDC(NULL);
    if (hdc == NULL) {
        ShowError( "Cant get device context" );
        return false;
    }

    HDC memdc = CreateCompatibleDC(hdc);
    ReleaseDC(NULL, hdc);

    if (memdc == NULL) {
        ShowError( "Cant create memory device context" );
        return false;
    }

    bmp->hbmp = CreateDIBSection(memdc, &bmp->bmi, DIB_RGB_COLORS,
                                (void**)&bmp->pixels, NULL, 0);

    if (bmp->hbmp == NULL) {
        ShowError( "Cant create DIB" );
        return false;
    }

    bmp->hdc        = memdc;
    bmp->width      = width;
    bmp->height     = height;
    bmp->hprev      = (HBITMAP)SelectObject(memdc, bmp->hbmp);

    return true;
}

void FreeBitmap(Bitmap* bmp)
{
    if (bmp->hprev && bmp->hdc) {
        SelectObject(bmp->hdc, bmp->hprev);
        bmp->hprev = NULL;
    }
    if (bmp->hbmp) {
        DeleteObject(bmp->hbmp);
        bmp->hbmp = NULL;
    }
    if (bmp->hdc) {
        DeleteDC(bmp->hdc);
        bmp->hdc = NULL;
    }
}

void ResizeBitmap(Bitmap* bmp, UINT width, UINT height)
{
    if ( width == bmp->width && height == bmp->height )
        return;

    if ( bmp->hprev && bmp->hdc ) {
        SelectObject(bmp->hdc, bmp->hprev);
        bmp->hprev = NULL;
    }
    if ( bmp->hbmp )
        DeleteObject( bmp->hbmp );

    bmp->bmi.bmiHeader.biWidth    = width;
    bmp->bmi.bmiHeader.biHeight   = -height;

    bmp->pixels = NULL;
    bmp->hbmp   = CreateDIBSection( bmp->hdc, &bmp->bmi, DIB_RGB_COLORS,
                                    (void**)&bmp->pixels, NULL, 0 );

    bmp->width   = width;
    bmp->height  = height;
    bmp->hprev   = (HBITMAP) SelectObject( bmp->hdc, bmp->hbmp );
}

void DrawBitmap(HDC hdc, Bitmap* bitmap)
{
    BitBlt(hdc, 0, 0, bitmap->width, bitmap->height, bitmap->hdc, 0, 0, SRCCOPY);
}

void ClearBitmap(Bitmap* bitmap, COLORREF backColor)
{
    int    size    = bitmap->width * bitmap->height;
    Pixel* pixels  = bitmap->pixels;

    while (size--) {
        pixels->val = backColor;
        pixels++;
    }
}

//-----------------------
// Double to format 26.6
//-----------------------
FT_Pos dtofix(const double& val)
{
    //long  sig = val;
    //char  man = round( (val - sig)*100.0 );
    //return sig << 6 | (man & 0x003F);
    long  sig = val;
    return sig << 6;
}

//-----------------------
// Format 26.6 to double
//-----------------------
double fixtod(const FT_Pos val)
{
    //double  man = char(val & 0x003F);
    //double  sig = long(val >> 6);
    //return sig + man/100.0;
    double  sig = long(val >> 6);
    return sig;
}


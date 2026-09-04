// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/ui_util.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/win/registry.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"

namespace updater::ui {

namespace {

struct FindProcessWindowsRecord {
  uint32_t process_id = 0;
  uint32_t window_flags = 0;
  raw_ptr<std::vector<HWND>> windows = nullptr;
};

BOOL CALLBACK FindProcessWindowsEnumProc(HWND hwnd, LPARAM lparam) {
  FindProcessWindowsRecord* enum_record =
      reinterpret_cast<FindProcessWindowsRecord*>(lparam);
  CHECK(enum_record);

  DWORD process_id = 0;
  ::GetWindowThreadProcessId(hwnd, &process_id);

  if (enum_record->process_id != process_id) {
    return true;
  }

  if ((enum_record->window_flags & kWindowMustBeTopLevel) &&
      ::GetParent(hwnd)) {
    return true;
  }

  if ((enum_record->window_flags & kWindowMustHaveSysMenu) &&
      !(GetWindowLong(hwnd, GWL_STYLE) & WS_SYSMENU)) {
    return true;
  }

  if ((enum_record->window_flags & kWindowMustBeVisible) &&
      !::IsWindowVisible(hwnd)) {
    return true;
  }

  enum_record->windows->push_back(hwnd);
  return true;
}

// RAII helper for selecting a GDI object into a DC that allows graceful failure
// handling via is_valid() without DCHECK-aborting on locked bitmaps.
class ScopedSelectObject {
 public:
  ScopedSelectObject(HDC hdc, HGDIOBJ object)
      : hdc_(hdc), old_object_(::SelectObject(hdc, object)) {}
  ScopedSelectObject(const ScopedSelectObject&) = delete;
  ScopedSelectObject& operator=(const ScopedSelectObject&) = delete;
  ~ScopedSelectObject() {
    if (is_valid()) {
      ::SelectObject(hdc_, old_object_);
    }
  }

  bool is_valid() const { return old_object_ && old_object_ != HGDI_ERROR; }

 private:
  const HDC hdc_;
  const HGDIOBJ old_object_;
};

}  // namespace

bool FindProcessWindows(uint32_t process_id,
                        uint32_t window_flags,
                        std::vector<HWND>* windows) {
  CHECK(windows);
  windows->clear();
  FindProcessWindowsRecord enum_record = {0};
  enum_record.process_id = process_id;
  enum_record.window_flags = window_flags;
  enum_record.windows = windows;
  ::EnumWindows(FindProcessWindowsEnumProc,
                reinterpret_cast<LPARAM>(&enum_record));
  const size_t num_windows = enum_record.windows->size();
  return num_windows > 0;
}

void MakeWindowForeground(HWND wnd) {
  if (!::IsWindowVisible(wnd)) {
    return;
  }
  ::SetWindowPos(wnd, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}

bool IsMainWindow(HWND wnd) {
  return nullptr == ::GetParent(wnd) && ::IsWindowVisible(wnd);
}

bool HasSystemMenu(HWND wnd) {
  return (::GetWindowLong(wnd, GWL_STYLE) & WS_SYSMENU) != 0;
}

IconSizes GetIconSizesForDpi(UINT dpi) {
  return {
      .cx_big = dpi ? ::GetSystemMetricsForDpi(SM_CXICON, dpi)
                    : ::GetSystemMetrics(SM_CXICON),
      .cy_big = dpi ? ::GetSystemMetricsForDpi(SM_CYICON, dpi)
                    : ::GetSystemMetrics(SM_CYICON),
      .cx_small = dpi ? ::GetSystemMetricsForDpi(SM_CXSMICON, dpi)
                      : ::GetSystemMetrics(SM_CXSMICON),
      .cy_small = dpi ? ::GetSystemMetricsForDpi(SM_CYSMICON, dpi)
                      : ::GetSystemMetrics(SM_CYSMICON),
  };
}

WindowIcons LoadResourceIcons(int icon_resource_id, UINT dpi) {
  const IconSizes sizes = GetIconSizesForDpi(dpi);
  HINSTANCE exe_instance = static_cast<HINSTANCE>(::GetModuleHandle(nullptr));
  WindowIcons icons;
  icons.icon_big.reset(reinterpret_cast<HICON>(
      ::LoadImage(exe_instance, MAKEINTRESOURCE(icon_resource_id), IMAGE_ICON,
                  sizes.cx_big, sizes.cy_big, LR_DEFAULTCOLOR)));
  icons.icon_small.reset(reinterpret_cast<HICON>(
      ::LoadImage(exe_instance, MAKEINTRESOURCE(icon_resource_id), IMAGE_ICON,
                  sizes.cx_small, sizes.cy_small, LR_DEFAULTCOLOR)));

  // If DPI-scaled icon loading fails for either size when scaled dimensions
  // differ from standard system metrics, reset both handles and symmetrically
  // retry with standard unscaled (96 DPI) system metrics.
  const bool sizes_differ = sizes.cx_big != ::GetSystemMetrics(SM_CXICON) ||
                            sizes.cy_big != ::GetSystemMetrics(SM_CYICON) ||
                            sizes.cx_small != ::GetSystemMetrics(SM_CXSMICON) ||
                            sizes.cy_small != ::GetSystemMetrics(SM_CYSMICON);
  if ((!icons.icon_big.is_valid() || !icons.icon_small.is_valid()) &&
      sizes_differ) {
    icons.icon_big.reset(reinterpret_cast<HICON>(
        ::LoadImage(exe_instance, MAKEINTRESOURCE(icon_resource_id), IMAGE_ICON,
                    ::GetSystemMetrics(SM_CXICON),
                    ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR)));
    icons.icon_small.reset(reinterpret_cast<HICON>(
        ::LoadImage(exe_instance, MAKEINTRESOURCE(icon_resource_id), IMAGE_ICON,
                    ::GetSystemMetrics(SM_CXSMICON),
                    ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR)));
  }
  if (!icons.icon_big.is_valid() || !icons.icon_small.is_valid()) {
    icons.icon_big.reset();
    icons.icon_small.reset();
  }
  return icons;
}

void SetWindowIcons(HWND hwnd,
                    WindowIcons new_icons,
                    WindowIcons& current_icons) {
  ::SendMessage(hwnd, WM_SETICON, ICON_BIG,
                reinterpret_cast<LPARAM>(new_icons.icon_big.get()));
  ::SendMessage(hwnd, WM_SETICON, ICON_SMALL,
                reinterpret_cast<LPARAM>(new_icons.icon_small.get()));
  current_icons = std::move(new_icons);
}

// Creates an icon from an opaque HBITMAP (such as an updater 24bpp 48x48 app
// logo BMP or a 32bpp compatible bitmap), scaled to the specified dimensions
// (or system icon dimensions if 0). This function scales the bitmap to the
// target dimensions using high-quality HALFTONE stretch mode and creates an
// HICON with a guaranteed 24bpp DIBSection and a monochrome 1bpp mask.
// Non-square source bitmaps are fitted and centered within the target
// dimensions while preserving their aspect ratio; any unused letterbox margin
// is made transparent in the 1bpp mask.
// Note: 32bpp source bitmaps are treated as opaque RGB; per-pixel alpha
// channels are ignored by StretchBlt into 24bpp and rendered fully opaque.
base::win::ScopedGDIObject<HICON> CreateIconFromHBitmap(HBITMAP bitmap,
                                                        int width,
                                                        int height,
                                                        UINT dpi) {
  if (!bitmap) {
    return {};
  }
  BITMAP bm = {};
  if (!::GetObject(bitmap, sizeof(bm), &bm) || bm.bmWidth <= 0 ||
      bm.bmHeight == 0 || (bm.bmBitsPixel != 24 && bm.bmBitsPixel != 32)) {
    VLOG(1) << __func__ << ": Invalid bitmap or unsupported bit depth ("
            << bm.bmBitsPixel << "bpp, " << bm.bmWidth << "x" << bm.bmHeight
            << ")";
    return {};
  }
  if (bm.bmBitsPixel == 32) {
    VLOG(1) << __func__
            << ": 32bpp bitmap provided; alpha channel blending is omitted "
               "and pixels are rendered fully opaque";
  }
  // Ensure the source height extent passed to StretchBlt is strictly positive,
  // protecting against potential negative height values (such as top-down DIBs
  // defined with negative heights in BITMAPINFOHEADER).
  const int bm_height = std::abs(bm.bmHeight);

  const IconSizes sizes = GetIconSizesForDpi(dpi);
  const int icon_w = width > 0 ? width : (height > 0 ? height : sizes.cx_big);
  const int icon_h = height > 0 ? height : (width > 0 ? width : sizes.cy_big);
  const int target_w = icon_w > 0 ? icon_w : 32;
  const int target_h = icon_h > 0 ? icon_h : 32;

  base::win::ScopedGetDC hdc(nullptr);
  base::win::ScopedCreateDC mem_dc(::CreateCompatibleDC(hdc));
  base::win::ScopedCreateDC src_dc(::CreateCompatibleDC(hdc));

  // Explicitly create a 24bpp DIB section rather than a compatible bitmap
  // via CreateCompatibleBitmap(). On modern 32bpp displays,
  // CreateCompatibleBitmap() returns a 32bpp DDB where StretchBlt() copies RGB
  // values while leaving the alpha channel at 0x00. Windows DWM and Alt+Tab
  // treat 32bpp icons with zero alpha as 100% transparent. An explicit 24bpp
  // bitmap ensures transparency is governed exclusively by the 1bpp mask.
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = target_w;
  bi.bmiHeader.biHeight = target_h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 24;
  bi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  base::win::ScopedGDIObject<HBITMAP> color_bmp(
      ::CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0));
  // In Win32 icons, the 1bpp monochrome mask for an opaque icon is all 0s
  // (0 in mask = opaque, 1 = transparent).
  base::win::ScopedGDIObject<HBITMAP> mask_bmp(
      ::CreateBitmap(target_w, target_h, 1, 1, nullptr));

  if (!hdc || !mem_dc.is_valid() || !src_dc.is_valid() ||
      !color_bmp.is_valid() || !mask_bmp.is_valid()) {
    VLOG(1) << __func__ << ": Failed to allocate GDI DCs or bitmaps";
    return {};
  }

  // Calculate scaled dimensions that fit within the target dimensions while
  // preserving the source bitmap's aspect ratio.
  int dst_w = target_w;
  int dst_h = target_h;
  if (static_cast<int64_t>(bm.bmWidth) * target_h >
      static_cast<int64_t>(target_w) * bm_height) {
    // Source is wider than target aspect ratio: fit to width.
    dst_w = target_w;
    dst_h = std::min(target_h,
                     std::max(1, ::MulDiv(bm_height, target_w, bm.bmWidth)));
  } else if (static_cast<int64_t>(bm.bmWidth) * target_h <
             static_cast<int64_t>(target_w) * bm_height) {
    // Source is taller than target aspect ratio: fit to height.
    dst_h = target_h;
    dst_w = std::min(target_w,
                     std::max(1, ::MulDiv(bm.bmWidth, target_h, bm_height)));
  }
  const int dst_x = (target_w - dst_w) / 2;
  const int dst_y = (target_h - dst_h) / 2;
  const bool is_letterboxed = dst_w < target_w || dst_h < target_h;

  // Initialize the 1bpp monochrome mask (0 = opaque, 1 = transparent).
  // If the source bitmap was letterboxed to preserve aspect ratio, any unused
  // margin is set to 1 (transparent), while the centered destination rectangle
  // is set to 0 (opaque). If the image fills the destination canvas completely,
  // the entire mask is set to 0 (opaque).
  {
    ScopedSelectObject select_mask(mem_dc.get(), mask_bmp.get());
    if (!select_mask.is_valid() ||
        !::PatBlt(mem_dc.get(), 0, 0, target_w, target_h,
                  is_letterboxed ? WHITENESS : BLACKNESS) ||
        (is_letterboxed &&
         !::PatBlt(mem_dc.get(), dst_x, dst_y, dst_w, dst_h, BLACKNESS))) {
      VLOG(1) << __func__ << ": Failed to initialize icon mask";
      return {};
    }
  }

  {
    ScopedSelectObject select_color(mem_dc.get(), color_bmp.get());
    ScopedSelectObject select_src(src_dc.get(), bitmap);
    if (!select_color.is_valid() || !select_src.is_valid()) {
      VLOG(1) << __func__ << ": Failed to select bitmaps into DCs";
      return {};
    }

    // In Win32 icons, transparent pixels (mask bit = 1) must have their color
    // bits set to RGB(0, 0, 0) so that (Destination AND 1) XOR 0 preserves the
    // background without XOR artifacts.
    if (is_letterboxed &&
        !::PatBlt(mem_dc.get(), 0, 0, target_w, target_h, BLACKNESS)) {
      VLOG(1) << __func__ << ": Failed to clear color bitmap background";
      return {};
    }

    ::SetStretchBltMode(mem_dc.get(), HALFTONE);
    ::SetBrushOrgEx(mem_dc.get(), 0, 0, nullptr);
    if (!::StretchBlt(mem_dc.get(), dst_x, dst_y, dst_w, dst_h, src_dc.get(), 0,
                      0, bm.bmWidth, bm_height, SRCCOPY)) {
      VLOG(1) << __func__ << ": StretchBlt failed";
      return {};
    }
  }

  ICONINFO icon_info = {};
  icon_info.fIcon = TRUE;
  icon_info.hbmMask = mask_bmp.get();
  icon_info.hbmColor = color_bmp.get();
  base::win::ScopedGDIObject<HICON> icon(::CreateIconIndirect(&icon_info));
  if (!icon.is_valid()) {
    VLOG(1) << __func__ << ": CreateIconIndirect failed";
  }
  return icon;
}

std::wstring GetInstallerDisplayName(const std::u16string& bundle_name,
                                     const std::wstring& lang) {
  std::wstring display_name = base::AsWString(bundle_name);
  if (display_name.empty()) {
    display_name = GetLocalizedString(IDS_FRIENDLY_COMPANY_NAME_BASE, lang);
  }
  return GetLocalizedStringF(IDS_INSTALLER_DISPLAY_NAME_BASE, display_name,
                             lang);
}

bool GetDlgItemText(HWND dlg, int item_id, std::wstring* text) {
  text->clear();
  auto* item = ::GetDlgItem(dlg, item_id);
  if (!item) {
    return false;
  }
  const auto num_chars = ::GetWindowTextLength(item);
  if (!num_chars) {
    return false;
  }
  std::vector<wchar_t> tmp(num_chars + 1);
  if (!::GetWindowText(item, &tmp.front(), tmp.size())) {
    return false;
  }
  text->assign(tmp.begin(), tmp.end());
  return true;
}

bool IsHighContrastOn() {
  HIGHCONTRAST hc = {.cbSize = sizeof(HIGHCONTRAST)};
  if (!::SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(HIGHCONTRAST), &hc,
                              0)) {
    return false;
  }
  return hc.dwFlags & HCF_HIGHCONTRASTON;
}

bool IsColorDark(COLORREF color) {
  // Coefficients for standard perceived luminance (Luma) calculation:
  // Y = 0.299R + 0.587G + 0.114B.
  // Weights and threshold are scaled by 1000 to keep operations in fast integer
  // math without floating-point conversions.
  constexpr int kRedLuminanceWeight = 299;
  constexpr int kGreenLuminanceWeight = 587;
  constexpr int kBlueLuminanceWeight = 114;
  // Midpoint luminance threshold (128 out of 255 scaled by 1000).
  constexpr int kDarkLuminanceThreshold = 128000;
  return (kRedLuminanceWeight * GetRValue(color) +
          kGreenLuminanceWeight * GetGValue(color) +
          kBlueLuminanceWeight * GetBValue(color)) < kDarkLuminanceThreshold;
}

bool IsDarkModeOn() {
  if (IsHighContrastOn()) {
    return IsColorDark(::GetSysColor(COLOR_WINDOW));
  }

  base::win::RegKey key(
      HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      KEY_READ);
  DWORD is_light_theme = 1;
  return key.ReadValueDW(L"AppsUseLightTheme", &is_light_theme) ==
             ERROR_SUCCESS &&
         !is_light_theme;
}

bool MaybeSetArrowCursor(HWND hwnd, WPARAM wparam, LPARAM lparam) {
  if (LOWORD(lparam) != HTCLIENT) {
    return false;
  }

  const HWND message_wnd = reinterpret_cast<HWND>(wparam);
  if (!message_wnd || !::IsWindow(message_wnd)) {
    return false;
  }

  if (message_wnd != hwnd && !::IsChild(hwnd, message_wnd)) {
    return false;
  }

  const HCURSOR arrow_cursor = ::LoadCursor(nullptr, IDC_ARROW);
  if (!arrow_cursor) {
    return false;
  }

  const HCURSOR class_cursor =
      reinterpret_cast<HCURSOR>(::GetClassLongPtr(message_wnd, GCLP_HCURSOR));

  // If the window class defines a custom cursor that is not the standard arrow
  // (e.g. an EDIT control with IDC_IBEAM or a window with a custom tool
  // cursor), do not override it. Windows with no class cursor (nullptr) or
  // with the standard arrow class cursor (such as #32770 dialogs) are
  // explicitly set to IDC_ARROW to dismiss the IDC_APPSTARTING feedback cursor.
  if (class_cursor != nullptr && class_cursor != arrow_cursor) {
    return false;
  }

  ::SetCursor(arrow_cursor);
  return true;
}

}  // namespace updater::ui

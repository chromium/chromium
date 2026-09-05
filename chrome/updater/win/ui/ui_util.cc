// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/ui_util.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
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

base::win::ScopedGDIObject<HICON> Create32bppAlphaIcon(HBITMAP bitmap,
                                                       int bm_width,
                                                       int bm_height,
                                                       int target_w,
                                                       int target_h,
                                                       int dst_x,
                                                       int dst_y,
                                                       int dst_w,
                                                       int dst_h) {
  base::win::ScopedGetDC hdc(nullptr);
  if (!hdc) {
    VLOG(1) << __func__ << ": Failed to acquire screen DC";
    return {};
  }

  BITMAPINFO bi32 = {};
  bi32.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi32.bmiHeader.biWidth = bm_width;
  bi32.bmiHeader.biHeight = -bm_height;  // top-down
  bi32.bmiHeader.biPlanes = 1;
  bi32.bmiHeader.biBitCount = 32;
  bi32.bmiHeader.biCompression = BI_RGB;

  std::vector<uint32_t> src_pixels(static_cast<size_t>(bm_width) * bm_height);
  if (::GetDIBits(hdc, bitmap, 0, bm_height, src_pixels.data(), &bi32,
                  DIB_RGB_COLORS) != bm_height) {
    return {};
  }

  bool has_partial_alpha = false;
  bool has_zero_alpha = false;
  bool has_opaque_alpha = false;
  bool has_unpremultiplied_colors = false;
  for (uint32_t pixel : src_pixels) {
    const uint8_t alpha = pixel >> 24;
    if (alpha > 0 && alpha < 255) {
      has_partial_alpha = true;
      const uint8_t r = (pixel >> 16) & 0xFF;
      const uint8_t g = (pixel >> 8) & 0xFF;
      const uint8_t b = pixel & 0xFF;
      if (r > alpha || g > alpha || b > alpha) {
        has_unpremultiplied_colors = true;
      }
    } else if (alpha == 0) {
      has_zero_alpha = true;
      if ((pixel & 0x00FFFFFF) != 0) {
        has_unpremultiplied_colors = true;
      }
    } else if (alpha == 255) {
      has_opaque_alpha = true;
    }
  }

  const bool has_per_pixel_alpha =
      has_partial_alpha || (has_zero_alpha && has_opaque_alpha);
  if (!has_per_pixel_alpha) {
    return {};
  }

  BITMAPV5HEADER v5 = {};
  v5.bV5Size = sizeof(BITMAPV5HEADER);
  v5.bV5Width = target_w;
  v5.bV5Height = -target_h;  // top-down
  v5.bV5Planes = 1;
  v5.bV5BitCount = 32;
  v5.bV5Compression = BI_RGB;
  v5.bV5RedMask = 0x00FF0000;
  v5.bV5GreenMask = 0x0000FF00;
  v5.bV5BlueMask = 0x000000FF;
  v5.bV5AlphaMask = 0xFF000000;
  v5.bV5CSType = LCS_WINDOWS_COLOR_SPACE;
  v5.bV5Intent = LCS_GM_IMAGES;

  void* v5_bits = nullptr;
  base::win::ScopedGDIObject<HBITMAP> color_bmp(
      ::CreateDIBSection(hdc, reinterpret_cast<BITMAPINFO*>(&v5),
                         DIB_RGB_COLORS, &v5_bits, nullptr, 0));
  if (!color_bmp.is_valid() || !v5_bits) {
    VLOG(1) << __func__ << ": Failed to allocate 32bpp DIB section";
    return {};
  }

  const size_t dst_pixels_count = static_cast<size_t>(target_w) * target_h;
  base::span<uint32_t> dst_pixels = UNSAFE_BUFFERS(
      base::span(static_cast<uint32_t*>(v5_bits), dst_pixels_count));
  std::ranges::fill(dst_pixels, 0);

  for (int dy = 0; dy < dst_h; ++dy) {
    const float sy = (dy + 0.5f) * bm_height / dst_h - 0.5f;
    const int unclamped_y = static_cast<int>(std::floor(sy));
    const int y0 = std::clamp(unclamped_y, 0, bm_height - 1);
    const int y1 = std::clamp(unclamped_y + 1, 0, bm_height - 1);
    const float wy = std::max(0.0f, sy - std::floor(sy));

    for (int dx = 0; dx < dst_w; ++dx) {
      const float sx = (dx + 0.5f) * bm_width / dst_w - 0.5f;
      const int unclamped_x = static_cast<int>(std::floor(sx));
      const int x0 = std::clamp(unclamped_x, 0, bm_width - 1);
      const int x1 = std::clamp(unclamped_x + 1, 0, bm_width - 1);
      const float wx = std::max(0.0f, sx - std::floor(sx));

      const uint32_t p00 = src_pixels[y0 * bm_width + x0];
      const uint32_t p10 = src_pixels[y0 * bm_width + x1];
      const uint32_t p01 = src_pixels[y1 * bm_width + x0];
      const uint32_t p11 = src_pixels[y1 * bm_width + x1];

      auto interp = [&](int shift) -> uint32_t {
        const float c00 = (p00 >> shift) & 0xFF;
        const float c10 = (p10 >> shift) & 0xFF;
        const float c01 = (p01 >> shift) & 0xFF;
        const float c11 = (p11 >> shift) & 0xFF;
        const float c0 = c00 * (1.0f - wx) + c10 * wx;
        const float c1 = c01 * (1.0f - wx) + c11 * wx;
        return static_cast<uint32_t>(
            std::clamp(c0 * (1.0f - wy) + c1 * wy, 0.0f, 255.0f));
      };

      const uint32_t b = interp(0);
      const uint32_t g = interp(8);
      const uint32_t r = interp(16);
      const uint32_t a = interp(24);

      // Windows Desktop Window Manager (DWM) expects 32bpp icons with
      // BITMAPV5HEADER and bV5AlphaMask to use premultiplied alpha. If the
      // input bitmap contains straight (un-premultiplied) alpha, premultiply
      // RGB channels by alpha to prevent bright halos on dark taskbars.
      const uint32_t r_out = has_unpremultiplied_colors ? (r * a) / 255 : r;
      const uint32_t g_out = has_unpremultiplied_colors ? (g * a) / 255 : g;
      const uint32_t b_out = has_unpremultiplied_colors ? (b * a) / 255 : b;

      dst_pixels[(dst_y + dy) * target_w + (dst_x + dx)] =
          (a << 24) | (r_out << 16) | (g_out << 8) | b_out;
    }
  }

  const size_t mask_bytes_per_line =
      (static_cast<size_t>(target_w) + 15) / 16 * 2;
  std::vector<uint8_t> mask_bits(mask_bytes_per_line * target_h, 0);
  base::win::ScopedGDIObject<HBITMAP> mask_bmp(
      ::CreateBitmap(target_w, target_h, 1, 1, mask_bits.data()));
  if (!mask_bmp.is_valid()) {
    VLOG(1) << __func__ << ": Failed to allocate mask bitmap";
    return {};
  }

  ICONINFO icon_info = {};
  icon_info.fIcon = TRUE;
  icon_info.hbmMask = mask_bmp.get();
  icon_info.hbmColor = color_bmp.get();
  return base::win::ScopedGDIObject<HICON>(::CreateIconIndirect(&icon_info));
}

base::win::ScopedGDIObject<HICON> CreateColorKeyedIcon(
    HBITMAP bitmap,
    int bm_width,
    int bm_height,
    int target_w,
    int target_h,
    int dst_x,
    int dst_y,
    int dst_w,
    int dst_h,
    std::optional<COLORREF> transparent_color) {
  base::win::ScopedGetDC hdc(nullptr);
  base::win::ScopedCreateDC mem_dc(::CreateCompatibleDC(hdc));
  base::win::ScopedCreateDC src_dc(::CreateCompatibleDC(hdc));
  if (!hdc || !mem_dc.is_valid() || !src_dc.is_valid()) {
    VLOG(1) << __func__ << ": Failed to allocate GDI DCs";
    return {};
  }

  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = target_w;
  bi.bmiHeader.biHeight = target_h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 24;
  bi.bmiHeader.biCompression = BI_RGB;

  void* color_bits_ptr = nullptr;
  base::win::ScopedGDIObject<HBITMAP> color_bmp(::CreateDIBSection(
      hdc, &bi, DIB_RGB_COLORS, &color_bits_ptr, nullptr, 0));
  base::win::ScopedGDIObject<HBITMAP> mask_bmp(
      ::CreateBitmap(target_w, target_h, 1, 1, nullptr));

  if (!color_bmp.is_valid() || !mask_bmp.is_valid() || !color_bits_ptr) {
    VLOG(1) << __func__ << ": Failed to allocate GDI bitmaps";
    return {};
  }

  std::optional<COLORREF> key_color;
  if (transparent_color.has_value()) {
    if (*transparent_color != CLR_INVALID) {
      key_color = *transparent_color;
    }
  }

  {
    ScopedSelectObject select_color(mem_dc.get(), color_bmp.get());
    ScopedSelectObject select_src(src_dc.get(), bitmap);
    if (!select_color.is_valid() || !select_src.is_valid()) {
      VLOG(1) << __func__ << ": Failed to select bitmaps into DCs";
      return {};
    }

    // If an explicit key color was not provided, sample the 4 corners of the
    // source bitmap.
    if (!key_color.has_value()) {
      constexpr COLORREF kLightDialogBg = RGB(255, 255, 255);
      constexpr COLORREF kDarkDialogBg = RGB(31, 31, 31);
      const COLORREF c00 = ::GetPixel(src_dc.get(), 0, 0);
      const COLORREF c10 = ::GetPixel(src_dc.get(), bm_width - 1, 0);
      const COLORREF c01 = ::GetPixel(src_dc.get(), 0, bm_height - 1);
      const COLORREF c11 =
          ::GetPixel(src_dc.get(), bm_width - 1, bm_height - 1);
      const bool corners_match =
          (c00 == c10 && c00 == c01 && c00 == c11 && c00 != CLR_INVALID);
      const bool is_known_dialog_bg =
          (c00 == kLightDialogBg || c00 == kDarkDialogBg);

      if (is_known_dialog_bg || corners_match) {
        key_color = c00;
      }
    }

    if (!::PatBlt(mem_dc.get(), 0, 0, target_w, target_h, BLACKNESS)) {
      VLOG(1) << __func__ << ": Failed to clear color bitmap background";
      return {};
    }

    ::SetStretchBltMode(mem_dc.get(), HALFTONE);
    ::SetBrushOrgEx(mem_dc.get(), 0, 0, nullptr);
    if (!::StretchBlt(mem_dc.get(), dst_x, dst_y, dst_w, dst_h, src_dc.get(), 0,
                      0, bm_width, bm_height, SRCCOPY)) {
      VLOG(1) << __func__ << ": StretchBlt failed";
      return {};
    }
  }
  ::GdiFlush();

  const size_t color_row_stride =
      ((static_cast<size_t>(target_w) * 3 + 3) / 4) * 4;
  const size_t color_buffer_size = color_row_stride * target_h;
  base::span<uint8_t> color_bytes = UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(color_bits_ptr), color_buffer_size));
  const size_t mask_row_stride =
      ((static_cast<size_t>(target_w) + 31) / 32) * 4;
  std::vector<uint8_t> mask_pixels(mask_row_stride * target_h, 0);
  std::vector<bool> is_transparent(static_cast<size_t>(target_w) * target_h,
                                   false);

  auto get_pixel_color = [&](int x, int y) -> COLORREF {
    const size_t dib_y = target_h - 1 - y;
    const size_t offset = dib_y * color_row_stride + static_cast<size_t>(x) * 3;
    return RGB(color_bytes[offset + 2], color_bytes[offset + 1],
               color_bytes[offset + 0]);
  };

  // Accounts for edge color interpolation introduced by StretchBlt's HALFTONE
  // mode, preventing anti-aliased perimeter pixels from failing exact color
  // matches or leaving fringing halos.
  constexpr int kColorTolerance = 10;
  auto is_color_match = [&](COLORREF c1, COLORREF c2) {
    return std::abs(static_cast<int>(GetRValue(c1)) -
                    static_cast<int>(GetRValue(c2))) <= kColorTolerance &&
           std::abs(static_cast<int>(GetGValue(c1)) -
                    static_cast<int>(GetGValue(c2))) <= kColorTolerance &&
           std::abs(static_cast<int>(GetBValue(c1)) -
                    static_cast<int>(GetBValue(c2))) <= kColorTolerance;
  };

  // For auto-detected key colors, verify directly on the in-memory color buffer
  // that the image contains non-background interior content (avoiding
  // expensive GDI GetPixel round-trips).
  if (key_color.has_value() && !transparent_color.has_value()) {
    bool has_interior_content = false;
    for (int y = dst_y; y < dst_y + dst_h; ++y) {
      for (int x = dst_x; x < dst_x + dst_w; ++x) {
        if (!is_color_match(get_pixel_color(x, y), *key_color)) {
          has_interior_content = true;
          break;
        }
      }
      if (has_interior_content) {
        break;
      }
    }
    if (!has_interior_content) {
      key_color = std::nullopt;
    }
  }

  // Mark letterbox margins as transparent.
  for (int y = 0; y < target_h; ++y) {
    for (int x = 0; x < target_w; ++x) {
      if (x < dst_x || x >= dst_x + dst_w || y < dst_y || y >= dst_y + dst_h) {
        is_transparent[static_cast<size_t>(y) * target_w + x] = true;
      }
    }
  }

  // If a background key color is identified, flood-fill from the perimeter of
  // the destination image rectangle across connected matching pixels.
  if (key_color.has_value()) {
    const COLORREF target_key = *key_color;
    std::vector<std::pair<int, int>> pixels_to_visit;
    pixels_to_visit.reserve(static_cast<size_t>(target_w) * target_h);

    auto check_and_push = [&](int x, int y) {
      if (x >= dst_x && x < dst_x + dst_w && y >= dst_y && y < dst_y + dst_h) {
        const size_t idx = static_cast<size_t>(y) * target_w + x;
        if (!is_transparent[idx] &&
            is_color_match(get_pixel_color(x, y), target_key)) {
          is_transparent[idx] = true;
          pixels_to_visit.push_back({x, y});
        }
      }
    };

    for (int x = dst_x; x < dst_x + dst_w; ++x) {
      check_and_push(x, dst_y);
      if (dst_h > 1) {
        check_and_push(x, dst_y + dst_h - 1);
      }
    }
    for (int y = dst_y; y < dst_y + dst_h; ++y) {
      check_and_push(dst_x, y);
      if (dst_w > 1) {
        check_and_push(dst_x + dst_w - 1, y);
      }
    }

    while (!pixels_to_visit.empty()) {
      const auto [cx, cy] = pixels_to_visit.back();
      pixels_to_visit.pop_back();

      static constexpr std::array<std::pair<int, int>, 4> kDirections = {{
          {0, 1},
          {0, -1},
          {1, 0},
          {-1, 0},
      }};
      for (const auto& [dx, dy] : kDirections) {
        check_and_push(cx + dx, cy + dy);
      }
    }
  }

  // Update mask bits (1 = transparent, 0 = opaque) and zero transparent color
  // pixels to prevent XOR artifacts.
  for (int y = 0; y < target_h; ++y) {
    const size_t dib_y = target_h - 1 - y;
    for (int x = 0; x < target_w; ++x) {
      if (is_transparent[static_cast<size_t>(y) * target_w + x]) {
        mask_pixels[dib_y * mask_row_stride + (x / 8)] |=
            static_cast<uint8_t>(1 << (7 - (x % 8)));
        const size_t offset =
            dib_y * color_row_stride + static_cast<size_t>(x) * 3;
        color_bytes[offset + 0] = 0;
        color_bytes[offset + 1] = 0;
        color_bytes[offset + 2] = 0;
      }
    }
  }

  struct {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[2];
  } mask_bi = {};
  mask_bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  mask_bi.bmiHeader.biWidth = target_w;
  mask_bi.bmiHeader.biHeight = target_h;
  mask_bi.bmiHeader.biPlanes = 1;
  mask_bi.bmiHeader.biBitCount = 1;
  mask_bi.bmiHeader.biCompression = BI_RGB;
  mask_bi.bmiColors[0] = {0, 0, 0, 0};
  mask_bi.bmiColors[1] = {255, 255, 255, 0};

  if (::SetDIBits(hdc, mask_bmp.get(), 0, target_h, mask_pixels.data(),
                  reinterpret_cast<BITMAPINFO*>(&mask_bi),
                  DIB_RGB_COLORS) != target_h) {
    VLOG(1) << __func__ << ": SetDIBits failed for icon mask";
    return {};
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

// Creates an icon from an HBITMAP (such as an updater 24bpp 48x48 app logo BMP,
// a 32bpp ARGB icon bitmap, or a 32bpp compatible bitmap), scaled to the
// specified dimensions (or system icon dimensions if 0).
// If the source bitmap is 32bpp with a true per-pixel alpha channel,
// synthesizes a 32bpp BITMAPV5HEADER icon with full alpha transparency. For
// opaque 24bpp or 32bpp bitmaps without an alpha channel, performs background
// color keying: samples the background color from (0, 0) (or tests for known
// light/dark dialog background colors RGB(255, 255, 255) and RGB(31, 31, 31),
// or uses `transparent_color` if provided) and keys out the connected
// background pixels in the 1bpp monochrome mask, setting transparent color
// pixels to RGB(0, 0, 0) to prevent Windows GDI XOR artifacts. Non-square
// source bitmaps are fitted and centered within the target dimensions while
// preserving their aspect ratio; any unused letterbox margin is made
// transparent in the 1bpp mask.
base::win::ScopedGDIObject<HICON> CreateIconFromHBitmap(
    HBITMAP bitmap,
    int width,
    int height,
    UINT dpi,
    std::optional<COLORREF> transparent_color) {
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

  // Ensure the source dimensions passed to StretchBlt are strictly positive,
  // protecting against potential negative height values (such as top-down DIBs
  // defined with negative heights in BITMAPINFOHEADER).
  const int bm_width = bm.bmWidth;
  const int bm_height = std::abs(bm.bmHeight);

  const IconSizes sizes = GetIconSizesForDpi(dpi);
  const int icon_w = width > 0 ? width : (height > 0 ? height : sizes.cx_big);
  const int icon_h = height > 0 ? height : (width > 0 ? width : sizes.cy_big);
  const int target_w = icon_w > 0 ? icon_w : 32;
  const int target_h = icon_h > 0 ? icon_h : 32;

  // Calculate scaled dimensions that fit within the target dimensions while
  // preserving the source bitmap's aspect ratio.
  int dst_w = target_w;
  int dst_h = target_h;
  if (static_cast<int64_t>(bm_width) * target_h >
      static_cast<int64_t>(target_w) * bm_height) {
    // Source is wider than target aspect ratio: fit to width.
    dst_w = target_w;
    dst_h = std::min(target_h,
                     std::max(1, ::MulDiv(bm_height, target_w, bm_width)));
  } else if (static_cast<int64_t>(bm_width) * target_h <
             static_cast<int64_t>(target_w) * bm_height) {
    // Source is taller than target aspect ratio: fit to height.
    dst_h = target_h;
    dst_w = std::min(target_w,
                     std::max(1, ::MulDiv(bm_width, target_h, bm_height)));
  }
  const int dst_x = (target_w - dst_w) / 2;
  const int dst_y = (target_h - dst_h) / 2;

  // 1. Check for true 32bpp per-pixel alpha:
  if (bm.bmBitsPixel == 32) {
    base::win::ScopedGDIObject<HICON> alpha_icon =
        Create32bppAlphaIcon(bitmap, bm_width, bm_height, target_w, target_h,
                             dst_x, dst_y, dst_w, dst_h);
    if (alpha_icon.is_valid()) {
      return alpha_icon;
    }
  }

  // 2. Color-keying fallback for opaque bitmaps (24bpp or opaque 32bpp).
  return CreateColorKeyedIcon(bitmap, bm_width, bm_height, target_w, target_h,
                              dst_x, dst_y, dst_w, dst_h, transparent_color);
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
  CHECK(text);
  text->clear();
  auto* item = ::GetDlgItem(dlg, item_id);
  if (!item) {
    return false;
  }
  ::SetLastError(ERROR_SUCCESS);
  const auto num_chars = ::GetWindowTextLength(item);
  if (!num_chars) {
    return ::GetLastError() == ERROR_SUCCESS;
  }
  text->resize(num_chars + 1);
  ::SetLastError(ERROR_SUCCESS);
  const auto chars_copied = ::GetWindowText(
      item, &text->front(), base::checked_cast<int>(text->size()));
  if (!chars_copied) {
    text->clear();
    return ::GetLastError() == ERROR_SUCCESS;
  }
  text->resize(chars_copied);
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

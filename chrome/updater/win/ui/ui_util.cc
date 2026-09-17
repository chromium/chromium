// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/ui_util.h"

#include <windows.h>

#include <stdint.h>
#include <algorithm>
#include <array>
#include <cmath>
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

// Extracts the alpha channel from `icon` scaled to `width x height`.
// If `icon` is a 32bpp icon with per-pixel alpha, extracts per-pixel alpha
// from its color bitmap. Otherwise, falls back to querying the 1bpp AND mask
// via `DrawIconEx` with `DI_MASK` (where 0 is opaque 255 and non-zero is
// transparent 0).
std::vector<uint8_t> GetIconAlphaChannel(HDC hdc,
                                         HICON icon,
                                         int width,
                                         int height) {
  if (!icon || width <= 0 || height <= 0) {
    return {};
  }

  std::optional<base::win::ScopedGetDC> default_dc;
  if (!hdc) {
    default_dc.emplace(nullptr);
  }
  const HDC dc = hdc ? hdc : static_cast<HDC>(default_dc.value());
  if (!dc) {
    VLOG(1) << __func__ << ": Failed to acquire screen DC";
    return {};
  }

  // 1. Inspect the icon's color bitmap via GetIconInfo. Standard Windows 32bpp
  // ARGB icons store transparency in the color bitmap's alpha channel while
  // leaving the 1bpp AND mask (hbmMask) all zeros (0x00, opaque).
  ICONINFO icon_info = {};
  if (::GetIconInfo(icon, &icon_info)) {
    base::win::ScopedGDIObject<HBITMAP> color_bmp(icon_info.hbmColor);
    base::win::ScopedGDIObject<HBITMAP> mask_bmp(icon_info.hbmMask);

    if (color_bmp.is_valid()) {
      BITMAP bm = {};
      if (::GetObject(color_bmp.get(), sizeof(bm), &bm) != 0 &&
          bm.bmBitsPixel == 32 && bm.bmWidth > 0 && bm.bmHeight > 0) {
        const int bm_width = static_cast<int>(bm.bmWidth);
        const int bm_height = static_cast<int>(bm.bmHeight);

        BITMAPINFO bi32 = {};
        bi32.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi32.bmiHeader.biWidth = bm_width;
        bi32.bmiHeader.biHeight = -bm_height;  // top-down
        bi32.bmiHeader.biPlanes = 1;
        bi32.bmiHeader.biBitCount = 32;
        bi32.bmiHeader.biCompression = BI_RGB;

        std::vector<uint32_t> pixels(static_cast<size_t>(bm_width) * bm_height);
        if (::GetDIBits(dc, color_bmp.get(), 0, bm_height, pixels.data(),
                        &bi32, DIB_RGB_COLORS) == bm_height) {
          bool has_per_pixel_alpha = false;
          for (uint32_t pixel : pixels) {
            if ((pixel >> 24) != 0) {
              has_per_pixel_alpha = true;
              break;
            }
          }

          // Tradeoff (All-zero alpha vs 1bpp AND mask fallback):
          // 32bpp icons with all-zero alpha (e.g. `google_update.ico`) are
          // treated as lacking per-pixel alpha, falling back to DI_MASK.
          // Non-zero alpha (even all 255) is treated as valid icon alpha.
          if (has_per_pixel_alpha) {
            std::vector<uint8_t> alpha(static_cast<size_t>(width) * height);
            for (int y = 0; y < height; ++y) {
              const int src_y =
                  std::clamp(::MulDiv(y, bm_height, height), 0, bm_height - 1);
              for (int x = 0; x < width; ++x) {
                const int src_x =
                    std::clamp(::MulDiv(x, bm_width, width), 0, bm_width - 1);
                const uint32_t pixel =
                    pixels[static_cast<size_t>(src_y) * bm_width + src_x];
                alpha[static_cast<size_t>(y) * width + x] =
                    static_cast<uint8_t>(pixel >> 24);
              }
            }
            return alpha;
          }
        }
      }
    }
  }

  // 2. Fallback for non-32bpp icons (or 32bpp icons without per-pixel alpha):
  // Query the 1bpp AND mask using DI_MASK. In Windows icon AND masks,
  // 0 (black) represents opaque pixels (alpha = 255) and non-zero
  // (white 0x00FFFFFF) represents transparent pixels (alpha = 0).
  BITMAPINFO mask_bi = {};
  mask_bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  mask_bi.bmiHeader.biWidth = width;
  mask_bi.bmiHeader.biHeight = -height;  // top-down
  mask_bi.bmiHeader.biPlanes = 1;
  mask_bi.bmiHeader.biBitCount = 32;
  mask_bi.bmiHeader.biCompression = BI_RGB;

  void* mask_bits = nullptr;
  base::win::ScopedGDIObject<HBITMAP> mask_dib(::CreateDIBSection(
      dc, &mask_bi, DIB_RGB_COLORS, &mask_bits, nullptr, 0));
  base::win::ScopedCreateDC mask_dc(::CreateCompatibleDC(dc));

  if (!mask_dib.is_valid() || !mask_dc.is_valid() || !mask_bits) {
    return {};
  }

  // SAFETY: `mask_dib` is a 32bpp top-down DIB section allocated immediately
  // above with dimensions `width x height`, containing exactly `width * height`
  // 32-bit DWORDs.
  base::span<uint32_t> mask_span = UNSAFE_BUFFERS(base::span(
      static_cast<uint32_t*>(mask_bits), static_cast<size_t>(width) * height));
  std::ranges::fill(mask_span, 0);

  {
    ScopedSelectObject select_mask(mask_dc.get(), mask_dib.get());
    if (!select_mask.is_valid()) {
      return {};
    }

    // Draw the icon's AND mask using DI_MASK. In Windows icon AND masks,
    // 0 (black) represents opaque pixels and non-zero (white 0x00FFFFFF)
    // represents transparent pixels.
    if (!::DrawIconEx(mask_dc.get(), 0, 0, icon, width, height, 0, nullptr,
                      DI_MASK)) {
      VLOG(1) << __func__ << ": DrawIconEx(DI_MASK) failed";
      return {};
    }
    ::GdiFlush();
  }

  // `mask_dib` is now unselected from `mask_dc`, ensuring safe memory access
  // across all display drivers and GDI acceleration modes.
  std::vector<uint8_t> alpha(static_cast<size_t>(width) * height);
  for (size_t i = 0; i < mask_span.size(); ++i) {
    alpha[i] = ((mask_span[i] & 0x00FFFFFF) == 0) ? 255 : 0;
  }
  return alpha;
}

// Synthesizes a 32bpp icon with per-pixel alpha (BITMAPV5HEADER) from
// `bitmap`. If `badge_icon` is provided, composites the badge over the base
// logo using GDI `::DrawIconEx` and restores the destination alpha channel via
// single-channel Porter-Duff "Over" (A_out = A_badge + A_base * (1 - A_badge)).
base::win::ScopedGDIObject<HICON> Create32bppAlphaIcon(
    HBITMAP bitmap,
    int bm_width,
    int bm_height,
    int target_w,
    int target_h,
    int dst_x,
    int dst_y,
    int dst_w,
    int dst_h,
    HICON badge_icon = nullptr,
    bool* is_alpha_bitmap = nullptr) {
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
  if (is_alpha_bitmap) {
    *is_alpha_bitmap = has_per_pixel_alpha;
  }

  // Opaque bitmaps (24bpp or 32bpp without true per-pixel alpha) must fall
  // back to `CreateColorKeyedIcon()`, which samples the border/corners and
  // keys out the solid dialog background (e.g. RGB(31, 31, 31) or
  // RGB(255, 255, 255)). Generating a 32bpp icon from an opaque base bitmap
  // without color keying would render the logo inside a solid square box.
  // Note: Unlike `GetIconAlphaChannel()`, which accepts fully opaque 32bpp
  // icons (all alpha = 255), base logo bitmaps with uniform alpha = 255 are
  // treated as opaque here so their solid rectangular background is keyed out.
  if (!has_per_pixel_alpha) {
    return {};
  }

  const RECT badge_rect = GetBadgeRect(target_w, target_h);
  const int badge_w = badge_rect.right - badge_rect.left;
  const int badge_h = badge_rect.bottom - badge_rect.top;
  const int badge_x = badge_rect.left;
  const int badge_y = badge_rect.top;

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
  // SAFETY: `color_bmp` is a 32bpp top-down DIB section allocated immediately
  // above with dimensions `target_w x target_h`, containing exactly
  // `target_w * target_h` 32-bit DWORDs.
  base::span<uint32_t> dst_span = UNSAFE_BUFFERS(
      base::span(static_cast<uint32_t*>(v5_bits), dst_pixels_count));
  std::ranges::fill(dst_span, 0);

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

      dst_span[(dst_y + dy) * target_w + (dst_x + dx)] =
          (a << 24) | (r_out << 16) | (g_out << 8) | b_out;
    }
  }

  if (badge_icon && badge_w > 0 && badge_h > 0) {
    const std::vector<uint8_t> badge_alpha =
        GetIconAlphaChannel(hdc, badge_icon, badge_w, badge_h);
    if (badge_alpha.empty()) {
      VLOG(1) << __func__ << ": Failed to extract badge alpha channel";
      return {};
    }
    base::win::ScopedCreateDC badge_dc(::CreateCompatibleDC(hdc));
    if (!badge_dc.is_valid()) {
      VLOG(1) << __func__ << ": Failed to allocate badge DC";
      return {};
    }

    // `color_bmp` is created as a top-down DIB (`v5.bV5Height = -target_h`),
    // so memory row 0 is the visual top. Visual coordinates
    // `(badge_x + x, badge_y + y)` directly index `dst_span` without
    // bottom-up inversion (`target_h - 1 - y`).
    std::vector<uint8_t> base_alpha(static_cast<size_t>(badge_w) * badge_h);
    for (int y = 0; y < badge_h; ++y) {
      for (int x = 0; x < badge_w; ++x) {
        const size_t idx = (badge_y + y) * target_w + (badge_x + x);
        base_alpha[static_cast<size_t>(y) * badge_w + x] =
            static_cast<uint8_t>(dst_span[idx] >> 24);
      }
    }

    {
      ScopedSelectObject select_color(badge_dc.get(), color_bmp.get());
      if (!select_color.is_valid()) {
        VLOG(1) << __func__ << ": Failed to select color bitmap into badge DC";
        return {};
      }
      if (!::DrawIconEx(badge_dc.get(), badge_x, badge_y, badge_icon, badge_w,
                        badge_h, 0, nullptr, DI_NORMAL)) {
        VLOG(1) << __func__ << ": DrawIconEx failed for badge";
        return {};
      }
      ::GdiFlush();
    }

    // `color_bmp` is now unselected from `badge_dc`, making direct DIB bit
    // access safe across all display drivers.
    // Standard Windows GDI DrawIconEx blends the source badge into the
    // destination DIB using standard alpha blending:
    // C_out = C_badge * A_badge + C_base * (1 - A_badge).
    // Since the base logo DIB is already premultiplied, the resulting RGB
    // channels in color_bmp are already correctly premultiplied for 32bpp
    // ARGB icon format. However, DrawIconEx does not preserve the
    // destination alpha channel, zeroing the alpha byte on rendered pixels.
    // Restore the blended per-pixel alpha using Porter-Duff "Over":
    // A_out = A_badge + A_base * (1 - A_badge).
    for (int y = 0; y < badge_h; ++y) {
      for (int x = 0; x < badge_w; ++x) {
        const size_t badge_idx = static_cast<size_t>(y) * badge_w + x;
        const size_t idx = (badge_y + y) * target_w + (badge_x + x);
        const uint8_t badge_a = badge_alpha[badge_idx];
        const uint8_t base_a = base_alpha[badge_idx];
        const uint8_t out_a = static_cast<uint8_t>(
            badge_a + (base_a * (255 - badge_a) + 127) / 255);
        dst_span[idx] =
            (dst_span[idx] & 0x00FFFFFF) | (static_cast<uint32_t>(out_a) << 24);
      }
    }
  }

  const size_t mask_bytes_per_line = CalculateDDBStride(target_w);
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
    std::optional<COLORREF> transparent_color,
    HICON badge_icon) {
  const RECT badge_rect = GetBadgeRect(target_w, target_h);
  const int badge_w = badge_rect.right - badge_rect.left;
  const int badge_h = badge_rect.bottom - badge_rect.top;
  const int badge_x = badge_rect.left;
  const int badge_y = badge_rect.top;
  const bool has_badge = (badge_icon != nullptr) && badge_w > 0 && badge_h > 0;

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
  if (!color_bmp.is_valid() || !color_bits_ptr) {
    VLOG(1) << __func__ << ": Failed to allocate color DIB section";
    return {};
  }

  std::optional<COLORREF> key_color;
  if (transparent_color.has_value()) {
    if (*transparent_color != CLR_INVALID) {
      key_color = *transparent_color;
    }
  }

  std::vector<uint8_t> badge_alpha;
  {
    ScopedSelectObject select_color(mem_dc.get(), color_bmp.get());
    ScopedSelectObject select_src(src_dc.get(), bitmap);
    if (!select_color.is_valid() || !select_src.is_valid()) {
      VLOG(1) << __func__ << ": Failed to select bitmaps into DCs";
      return {};
    }

    // If an explicit key color was not provided, sample the 4 corners directly
    // from the source bitmap (src_dc) before compositing any badge overlay onto
    // mem_dc. This ensures c10 is not contaminated by the badge overlay in the
    // top-right corner, preserving corners_match and color keying for logos
    // with custom background colors.
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

    if (has_badge) {
      badge_alpha = GetIconAlphaChannel(hdc, badge_icon, badge_w, badge_h);
      if (badge_alpha.empty()) {
        VLOG(1) << __func__ << ": Failed to extract badge alpha channel";
        return {};
      }
      if (!::DrawIconEx(mem_dc.get(), badge_x, badge_y, badge_icon, badge_w,
                        badge_h, 0, nullptr, DI_NORMAL)) {
        VLOG(1) << __func__ << ": DrawIconEx failed for badge";
        return {};
      }
    }
  }
  ::GdiFlush();

  constexpr uint8_t kBadgeOpaqueThreshold = 128;
  auto is_badge_opaque = [&](int x, int y) -> bool {
    if (!has_badge || badge_alpha.empty() || x < badge_x ||
        x >= badge_x + badge_w || y < badge_y || y >= badge_y + badge_h) {
      return false;
    }
    return badge_alpha[static_cast<size_t>(y - badge_y) * badge_w +
                       (x - badge_x)] >= kBadgeOpaqueThreshold;
  };

  const size_t color_row_stride = CalculateDIBStride(target_w, 24);
  const size_t color_buffer_size = color_row_stride * target_h;
  // SAFETY: `color_bmp` is a 24bpp DIB section allocated immediately above
  // with width `target_w` and height `target_h`, whose byte buffer size is
  // exactly `CalculateDIBStride(target_w, 24) * target_h`.
  base::span<uint8_t> color_bytes = UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(color_bits_ptr), color_buffer_size));
  const size_t mask_row_stride = CalculateDDBStride(target_w);
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
  // that the base logo contains non-background interior content (avoiding
  // expensive GDI GetPixel round-trips). Pixels covered by an opaque badge
  // overlay are skipped so we inspect only the underlying base logo.
  if (key_color.has_value() && !transparent_color.has_value()) {
    bool has_interior_content = false;
    for (int y = dst_y; y < dst_y + dst_h && !has_interior_content; ++y) {
      for (int x = dst_x; x < dst_x + dst_w; ++x) {
        if (is_badge_opaque(x, y)) {
          continue;
        }
        if (!is_color_match(get_pixel_color(x, y), *key_color)) {
          has_interior_content = true;
          break;
        }
      }
    }
    if (!has_interior_content) {
      key_color = std::nullopt;
    }
  }

  // Mark letterbox margins as transparent, excluding pixels where the badge
  // overlay is opaque.
  for (int y = 0; y < target_h; ++y) {
    for (int x = 0; x < target_w; ++x) {
      if (is_badge_opaque(x, y)) {
        continue;
      }
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
        if (is_badge_opaque(x, y)) {
          return;
        }
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
  // pixels to prevent XOR artifacts. Note that CreateBitmap expects top-down
  // scanlines (row 0 is visual top), whereas color_bmp is a bottom-up DIB.
  for (int y = 0; y < target_h; ++y) {
    const size_t dib_y = target_h - 1 - y;
    for (int x = 0; x < target_w; ++x) {
      if (is_transparent[static_cast<size_t>(y) * target_w + x]) {
        mask_pixels[static_cast<size_t>(y) * mask_row_stride + (x / 8)] |=
            static_cast<uint8_t>(1 << (7 - (x % 8)));
        const size_t offset =
            dib_y * color_row_stride + static_cast<size_t>(x) * 3;
        color_bytes[offset + 0] = 0;
        color_bytes[offset + 1] = 0;
        color_bytes[offset + 2] = 0;
      }
    }
  }

  base::win::ScopedGDIObject<HBITMAP> mask_bmp(
      ::CreateBitmap(target_w, target_h, 1, 1, mask_pixels.data()));
  if (!mask_bmp.is_valid()) {
    VLOG(1) << __func__ << ": Failed to allocate mask bitmap";
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

std::optional<DWORD> ReadPersonalizeRegistryFlag(const wchar_t* value_name) {
  base::win::RegKey key(
      HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      KEY_READ);
  DWORD value = 0;
  if (key.ReadValueDW(value_name, &value) == ERROR_SUCCESS) {
    return value;
  }
  return std::nullopt;
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

RECT GetBadgeRect(int target_w, int target_h) {
  if (target_w <= 0 || target_h <= 0) {
    return {};
  }
  const int badge_w = std::max((target_w + 1) / 2, 1);
  const int badge_h = std::max((target_h + 1) / 2, 1);
  const int badge_x = target_w - badge_w;
  const int badge_y = 0;
  return {
      .left = badge_x,
      .top = badge_y,
      .right = badge_x + badge_w,
      .bottom = badge_y + badge_h,
  };
}

SIZE GetBaseLogoDimensions(int target_w, int target_h) {
  if (target_w <= 0 || target_h <= 0) {
    return {};
  }
  return {
      .cx = std::max((target_w * 3 + 2) / 4, 1),
      .cy = std::max((target_h * 3 + 2) / 4, 1),
  };
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
    std::optional<COLORREF> transparent_color,
    HICON badge_icon,
    bool* badge_applied) {
  if (badge_applied) {
    *badge_applied = false;
  }
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

  const bool has_badge = badge_icon != nullptr;

  // When a badge overlay is requested, the base logo is scaled to 3/4 of the
  // target canvas and positioned in the lower-left, allowing the badge overlay
  // in the top-right to sit higher and to the right of the logo (matching the
  // prominent native installer overlay appearance without obscuring the
  // center of the base logo). When unbadged, the base logo occupies the full
  // canvas and is centered.
  const SIZE base_size = has_badge ? GetBaseLogoDimensions(target_w, target_h)
                                   : SIZE{.cx = target_w, .cy = target_h};
  const int base_w = base_size.cx;
  const int base_h = base_size.cy;

  // Calculate scaled dimensions that fit within the target dimensions while
  // preserving the source bitmap's aspect ratio.
  int dst_w = base_w;
  int dst_h = base_h;
  if (static_cast<int64_t>(bm_width) * base_h >
      static_cast<int64_t>(base_w) * bm_height) {
    // Source is wider than target aspect ratio: fit to width.
    dst_w = base_w;
    dst_h =
        std::min(base_h, std::max(1, ::MulDiv(bm_height, base_w, bm_width)));
  } else if (static_cast<int64_t>(bm_width) * base_h <
             static_cast<int64_t>(base_w) * bm_height) {
    // Source is taller than target aspect ratio: fit to height.
    dst_h = base_h;
    dst_w =
        std::min(base_w, std::max(1, ::MulDiv(bm_width, base_h, bm_height)));
  }
  const int dst_x = has_badge ? (base_w - dst_w) / 2 : (target_w - dst_w) / 2;
  const int dst_y = has_badge ? target_h - dst_h : (target_h - dst_h) / 2;

  // 1. Check for true 32bpp per-pixel alpha:
  // If the source bitmap is 32bpp and possesses true per-pixel alpha,
  // synthesize a 32bpp BITMAPV5HEADER icon with per-pixel alpha blending.
  if (bm.bmBitsPixel == 32) {
    bool is_alpha_bitmap = false;
    base::win::ScopedGDIObject<HICON> alpha_icon =
        Create32bppAlphaIcon(bitmap, bm_width, bm_height, target_w, target_h,
                             dst_x, dst_y, dst_w, dst_h, badge_icon,
                             &is_alpha_bitmap);
    if (alpha_icon.is_valid()) {
      if (badge_applied) {
        *badge_applied = has_badge;
      }
      return alpha_icon;
    }
    if (is_alpha_bitmap) {
      if (has_badge) {
        VLOG(1) << __func__
                << ": Badge failed on alpha icon; retrying unbadged";
        return CreateIconFromHBitmap(bitmap, width, height, dpi,
                                     transparent_color, /*badge_icon=*/nullptr,
                                     badge_applied);
      }
      return {};
    }
  }

  // 2. Color-keying fallback for opaque bitmaps (24bpp or opaque 32bpp).
  // Generates a 24bpp icon with a 1bpp monochrome mask keyed to the dialog
  // background, avoiding multi-pass software keying while guaranteeing clean
  // transparent silhouettes without solid background boxes.
  base::win::ScopedGDIObject<HICON> keyed_icon = CreateColorKeyedIcon(
      bitmap, bm_width, bm_height, target_w, target_h, dst_x, dst_y, dst_w,
      dst_h, transparent_color, badge_icon);
  if (keyed_icon.is_valid()) {
    if (badge_applied) {
      *badge_applied = has_badge;
    }
    return keyed_icon;
  }

  // If badged creation failed, retry without the badge so the user gets a
  // full-canvas centered unbadged icon rather than failing or showing an
  // off-center logo.
  if (has_badge) {
    VLOG(1) << __func__ << ": Badged icon creation failed; retrying unbadged";
    return CreateIconFromHBitmap(bitmap, width, height, dpi, transparent_color,
                                 /*badge_icon=*/nullptr, badge_applied);
  }

  return {};
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
      item, text->data(), base::checked_cast<int>(text->size()));
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

  const std::optional<DWORD> is_light_theme =
      ReadPersonalizeRegistryFlag(L"AppsUseLightTheme");
  return is_light_theme.has_value() && !*is_light_theme;
}

bool CouldBeThemeSettingChange(WPARAM wparam) {
  // Zero covers the shell broadcasts, "ImmersiveColorSet" among them, and any
  // unattributed change.
  return wparam == 0 || wparam == SPI_SETHIGHCONTRAST;
}

void ApplySuggestedWindowRect(HWND hwnd, LPARAM lparam) {
  if (const RECT* new_window_rect = reinterpret_cast<const RECT*>(lparam)) {
    ::SetWindowPos(hwnd, nullptr, new_window_rect->left, new_window_rect->top,
                   new_window_rect->right - new_window_rect->left,
                   new_window_rect->bottom - new_window_rect->top,
                   SWP_NOZORDER | SWP_NOACTIVATE);
  }
}

bool IsSystemDarkModeOn() {
  if (IsHighContrastOn()) {
    return IsColorDark(::GetSysColor(COLOR_WINDOW));
  }

  const std::optional<DWORD> is_light_theme =
      ReadPersonalizeRegistryFlag(L"SystemUsesLightTheme");
  if (is_light_theme.has_value()) {
    return !*is_light_theme;
  }
  return IsDarkModeOn();
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

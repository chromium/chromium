// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/ui_util.h"

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/win_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace updater::ui {

TEST(UiUtilTest, IsColorDark) {
  // Pure black and white.
  EXPECT_TRUE(IsColorDark(RGB(0, 0, 0)));
  EXPECT_FALSE(IsColorDark(RGB(255, 255, 255)));

  // Midpoint gray boundaries.
  EXPECT_TRUE(IsColorDark(RGB(127, 127, 127)));
  EXPECT_FALSE(IsColorDark(RGB(128, 128, 128)));

  // Windows High Contrast Themes window background colors.
  // High Contrast Black / Night Sky (Dark)
  EXPECT_TRUE(IsColorDark(RGB(0, 0, 0)));
  // High Contrast White (Light)
  EXPECT_FALSE(IsColorDark(RGB(255, 255, 255)));
  // Aquatic theme (Dark blue/teal background)
  EXPECT_TRUE(IsColorDark(RGB(32, 32, 32)));
  EXPECT_TRUE(IsColorDark(RGB(0, 32, 48)));
  // Desert theme (Light cream/beige background)
  EXPECT_FALSE(IsColorDark(RGB(255, 250, 239)));
}

TEST(UiUtilTest, MaybeSetArrowCursor) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  HWND parent_hwnd =
      ::CreateWindowEx(0, L"STATIC", L"Parent", WS_POPUP, 0, 0, 100, 100,
                       nullptr, nullptr, nullptr, nullptr);
  ASSERT_TRUE(parent_hwnd);
  const absl::Cleanup destroy_parent = [&] { ::DestroyWindow(parent_hwnd); };

  HWND static_child_hwnd =
      ::CreateWindowEx(0, L"STATIC", L"Child", WS_CHILD | WS_VISIBLE, 0, 0, 50,
                       50, parent_hwnd, nullptr, nullptr, nullptr);
  ASSERT_TRUE(static_child_hwnd);

  HWND edit_child_hwnd =
      ::CreateWindowEx(0, L"EDIT", L"Edit", WS_CHILD | WS_VISIBLE, 0, 50, 50,
                       50, parent_hwnd, nullptr, nullptr, nullptr);
  ASSERT_TRUE(edit_child_hwnd);

  HWND unrelated_hwnd =
      ::CreateWindowEx(0, L"STATIC", L"Unrelated", WS_POPUP, 0, 0, 100, 100,
                       nullptr, nullptr, nullptr, nullptr);
  ASSERT_TRUE(unrelated_hwnd);
  const absl::Cleanup destroy_unrelated = [&] {
    ::DestroyWindow(unrelated_hwnd);
  };

  // HTCLIENT on the window itself -> returns true.
  EXPECT_TRUE(MaybeSetArrowCursor(parent_hwnd,
                                  reinterpret_cast<WPARAM>(parent_hwnd),
                                  MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)));

  // HTCLIENT on a child window without class cursor (STATIC) -> returns true.
  EXPECT_TRUE(MaybeSetArrowCursor(parent_hwnd,
                                  reinterpret_cast<WPARAM>(static_child_hwnd),
                                  MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)));

  // HTCLIENT on a child window with its own class cursor (EDIT) -> returns
  // false.
  EXPECT_FALSE(MaybeSetArrowCursor(parent_hwnd,
                                   reinterpret_cast<WPARAM>(edit_child_hwnd),
                                   MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)));

  // HTCLIENT on a window itself with its own non-arrow class cursor (EDIT) ->
  // returns false.
  EXPECT_FALSE(MaybeSetArrowCursor(edit_child_hwnd,
                                   reinterpret_cast<WPARAM>(edit_child_hwnd),
                                   MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)));

  // HTCLIENT on an unrelated window -> returns false.
  EXPECT_FALSE(MaybeSetArrowCursor(parent_hwnd,
                                   reinterpret_cast<WPARAM>(unrelated_hwnd),
                                   MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)));

  // Non-client hit-test (e.g. HTCAPTION) -> returns false.
  EXPECT_FALSE(MaybeSetArrowCursor(parent_hwnd,
                                   reinterpret_cast<WPARAM>(parent_hwnd),
                                   MAKELPARAM(HTCAPTION, WM_MOUSEMOVE)));

  // Null or invalid message_wnd -> returns false.
  EXPECT_FALSE(
      MaybeSetArrowCursor(parent_hwnd, 0, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)));
  EXPECT_FALSE(MaybeSetArrowCursor(parent_hwnd,
                                   reinterpret_cast<WPARAM>(parent_hwnd) + 1,
                                   MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)));
}

TEST(UiUtilTest, CreateIconFromBitmap) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  // Passing nullptr returns an invalid handle.
  EXPECT_FALSE(CreateIconFromHBitmap(nullptr).is_valid());
  EXPECT_FALSE(CreateIconFromHBitmap(nullptr, 16, 16).is_valid());

  base::win::ScopedGetDC dc(nullptr);

  // App logos are 24bpp 48x48 uncompressed RGB BMPs. Create a 24bpp 48x48 DIB
  // representing an application logo.
  BITMAPINFO bi24 = {};
  bi24.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi24.bmiHeader.biWidth = 48;
  bi24.bmiHeader.biHeight = 48;
  bi24.bmiHeader.biPlanes = 1;
  bi24.bmiHeader.biBitCount = 24;
  bi24.bmiHeader.biCompression = BI_RGB;
  void* bits24 = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp24(
      ::CreateDIBSection(dc, &bi24, DIB_RGB_COLORS, &bits24, nullptr, 0));
  ASSERT_TRUE(bmp24.is_valid());
  ASSERT_NE(bits24, nullptr);
  BITMAP dib_bm24 = {};
  ASSERT_NE(::GetObject(bmp24.get(), sizeof(dib_bm24), &dib_bm24), 0);
  const size_t bytes24 =
      static_cast<size_t>(dib_bm24.bmWidthBytes) * dib_bm24.bmHeight;
  base::span<uint8_t> span24 =
      UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(bits24), bytes24));
  std::ranges::fill(span24, 0x80);

  // Create standard big (32x32) and small (16x16) icons from the 48x48 logo.
  base::win::ScopedGDIObject<HICON> icon_big =
      CreateIconFromHBitmap(bmp24.get(), 32, 32);
  ASSERT_TRUE(icon_big.is_valid());
  ICONINFO info_big = {};
  ASSERT_TRUE(::GetIconInfo(icon_big.get(), &info_big));
  base::win::ScopedGDIObject<HBITMAP> color_big(info_big.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> mask_big(info_big.hbmMask);
  EXPECT_TRUE(color_big.is_valid());
  EXPECT_TRUE(mask_big.is_valid());
  BITMAP bm_big = {};
  EXPECT_NE(::GetObject(color_big.get(), sizeof(bm_big), &bm_big), 0);
  EXPECT_EQ(bm_big.bmWidth, 32);
  EXPECT_EQ(bm_big.bmHeight, 32);
  EXPECT_GE(bm_big.bmBitsPixel, 24);

  // Verifies that the 1bpp monochrome icon mask is fully opaque (all 0s).
  auto verify_mask_opaque = [&dc](HBITMAP mask, int width, int height) {
    BITMAP mask_bm = {};
    ASSERT_NE(::GetObject(mask, sizeof(mask_bm), &mask_bm), 0);
    EXPECT_EQ(mask_bm.bmWidth, width);
    EXPECT_EQ(mask_bm.bmHeight, height);
    EXPECT_EQ(mask_bm.bmBitsPixel, 1);

    // In a 1bpp DIB, scanlines are DWORD-aligned (4-byte boundary) and the
    // color table contains 2 RGBQUAD entries.
    struct {
      BITMAPINFOHEADER bmiHeader;
      RGBQUAD bmiColors[2];
    } mask_bi = {};
    mask_bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    mask_bi.bmiHeader.biWidth = width;
    mask_bi.bmiHeader.biHeight = height;
    mask_bi.bmiHeader.biPlanes = 1;
    mask_bi.bmiHeader.biBitCount = 1;
    mask_bi.bmiHeader.biCompression = BI_RGB;

    const size_t row_bytes = ((static_cast<size_t>(width) + 31) / 32) * 4;
    std::vector<uint8_t> mask_pixels(row_bytes * height, 0xFF);
    ASSERT_EQ(
        ::GetDIBits(dc, mask, 0, height, mask_pixels.data(),
                    reinterpret_cast<BITMAPINFO*>(&mask_bi), DIB_RGB_COLORS),
        height);

    const size_t active_bytes_per_row = (static_cast<size_t>(width) + 7) / 8;
    base::span<const uint8_t> pixels_span(mask_pixels);
    for (int y = 0; y < height; ++y) {
      base::span<const uint8_t> row =
          pixels_span.subspan(y * row_bytes, active_bytes_per_row);
      for (uint8_t byte : row) {
        EXPECT_EQ(byte, 0u);
      }
    }
  };
  ASSERT_NO_FATAL_FAILURE(verify_mask_opaque(mask_big.get(), 32, 32));

  base::win::ScopedGDIObject<HICON> icon_small =
      CreateIconFromHBitmap(bmp24.get(), 16, 16);
  ASSERT_TRUE(icon_small.is_valid());
  ICONINFO info_small = {};
  ASSERT_TRUE(::GetIconInfo(icon_small.get(), &info_small));
  base::win::ScopedGDIObject<HBITMAP> color_small(info_small.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> mask_small(info_small.hbmMask);
  EXPECT_TRUE(color_small.is_valid());
  EXPECT_TRUE(mask_small.is_valid());
  BITMAP bm_small = {};
  EXPECT_NE(::GetObject(color_small.get(), sizeof(bm_small), &bm_small), 0);
  EXPECT_EQ(bm_small.bmWidth, 16);
  EXPECT_EQ(bm_small.bmHeight, 16);
  EXPECT_GE(bm_small.bmBitsPixel, 24);
  ASSERT_NO_FATAL_FAILURE(verify_mask_opaque(mask_small.get(), 16, 16));

  // Verify symmetric single-dimension fallback (omitted dimension matches the
  // specified dimension).
  base::win::ScopedGDIObject<HICON> icon_width_only =
      CreateIconFromHBitmap(bmp24.get(), 16);
  ASSERT_TRUE(icon_width_only.is_valid());
  ICONINFO width_only_info = {};
  ASSERT_TRUE(::GetIconInfo(icon_width_only.get(), &width_only_info));
  base::win::ScopedGDIObject<HBITMAP> width_only_color(
      width_only_info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> width_only_mask(width_only_info.hbmMask);
  BITMAP width_only_bm = {};
  EXPECT_NE(::GetObject(width_only_color.get(), sizeof(width_only_bm),
                        &width_only_bm),
            0);
  EXPECT_EQ(width_only_bm.bmWidth, 16);
  EXPECT_EQ(width_only_bm.bmHeight, 16);

  base::win::ScopedGDIObject<HICON> icon_height_only =
      CreateIconFromHBitmap(bmp24.get(), 0, 16);
  ASSERT_TRUE(icon_height_only.is_valid());
  ICONINFO height_only_info = {};
  ASSERT_TRUE(::GetIconInfo(icon_height_only.get(), &height_only_info));
  base::win::ScopedGDIObject<HBITMAP> height_only_color(
      height_only_info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> height_only_mask(
      height_only_info.hbmMask);
  BITMAP height_only_bm = {};
  EXPECT_NE(::GetObject(height_only_color.get(), sizeof(height_only_bm),
                        &height_only_bm),
            0);
  EXPECT_EQ(height_only_bm.bmWidth, 16);
  EXPECT_EQ(height_only_bm.bmHeight, 16);

  // Verify top-down DIB handling (negative biHeight).
  BITMAPINFO bi24_top_down = {};
  bi24_top_down.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi24_top_down.bmiHeader.biWidth = 48;
  bi24_top_down.bmiHeader.biHeight = -48;
  bi24_top_down.bmiHeader.biPlanes = 1;
  bi24_top_down.bmiHeader.biBitCount = 24;
  bi24_top_down.bmiHeader.biCompression = BI_RGB;
  void* bits24_top_down = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp24_top_down(::CreateDIBSection(
      dc, &bi24_top_down, DIB_RGB_COLORS, &bits24_top_down, nullptr, 0));
  ASSERT_TRUE(bmp24_top_down.is_valid());
  ASSERT_NE(bits24_top_down, nullptr);
  BITMAP dib_bm24_top_down = {};
  ASSERT_NE(::GetObject(bmp24_top_down.get(), sizeof(dib_bm24_top_down),
                        &dib_bm24_top_down),
            0);
  const size_t bytes24_top_down =
      static_cast<size_t>(dib_bm24_top_down.bmWidthBytes) *
      std::abs(dib_bm24_top_down.bmHeight);
  const size_t row_stride = dib_bm24_top_down.bmWidthBytes;
  base::span<uint8_t> span24_top_down = UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(bits24_top_down), bytes24_top_down));
  // In a top-down DIB (negative biHeight), row 0 is the visual top. Fill top
  // half (rows 0-23) with Red (BGR: 0, 0, 255) and bottom half (rows 24-47)
  // with Blue (BGR: 255, 0, 0).
  for (size_t y = 0; y < 24; ++y) {
    base::span<uint8_t> row =
        span24_top_down.subspan(y * row_stride, row_stride);
    for (size_t x = 0; x < 48; ++x) {
      row[x * 3 + 0] = 0x00;
      row[x * 3 + 1] = 0x00;
      row[x * 3 + 2] = 0xFF;
    }
  }
  for (size_t y = 24; y < 48; ++y) {
    base::span<uint8_t> row =
        span24_top_down.subspan(y * row_stride, row_stride);
    for (size_t x = 0; x < 48; ++x) {
      row[x * 3 + 0] = 0xFF;
      row[x * 3 + 1] = 0x00;
      row[x * 3 + 2] = 0x00;
    }
  }
  base::win::ScopedGDIObject<HICON> icon_top_down =
      CreateIconFromHBitmap(bmp24_top_down.get(), 32, 32);
  ASSERT_TRUE(icon_top_down.is_valid());

  ICONINFO info_top_down = {};
  ASSERT_TRUE(::GetIconInfo(icon_top_down.get(), &info_top_down));
  base::win::ScopedGDIObject<HBITMAP> color_top_down(info_top_down.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> mask_top_down(info_top_down.hbmMask);
  ASSERT_TRUE(color_top_down.is_valid());

  BITMAPINFO inspect_bi = {};
  inspect_bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  inspect_bi.bmiHeader.biWidth = 32;
  inspect_bi.bmiHeader.biHeight = 32;
  inspect_bi.bmiHeader.biPlanes = 1;
  inspect_bi.bmiHeader.biBitCount = 24;
  inspect_bi.bmiHeader.biCompression = BI_RGB;
  const size_t inspect_row_bytes = ((32 * 24 + 31) / 32) * 4;
  std::vector<uint8_t> inspect_pixels(inspect_row_bytes * 32, 0);
  ASSERT_EQ(::GetDIBits(dc, color_top_down.get(), 0, 32, inspect_pixels.data(),
                        &inspect_bi, DIB_RGB_COLORS),
            32);
  base::span<const uint8_t> inspect_span(inspect_pixels);
  // In a standard bottom-up DIB (positive biHeight in inspect_bi), row 0 is the
  // visual bottom of the icon, which must be Blue (BGR: 255, 0, 0).
  base::span<const uint8_t> bottom_row =
      inspect_span.subspan(0 * inspect_row_bytes, inspect_row_bytes);
  EXPECT_EQ(bottom_row[0], 0xFF);
  EXPECT_EQ(bottom_row[1], 0x00);
  EXPECT_EQ(bottom_row[2], 0x00);

  // Row 31 is the visual top of the icon, which must be Red (BGR: 0, 0, 255).
  base::span<const uint8_t> top_row =
      inspect_span.subspan(31 * inspect_row_bytes, inspect_row_bytes);
  EXPECT_EQ(top_row[0], 0x00);
  EXPECT_EQ(top_row[1], 0x00);
  EXPECT_EQ(top_row[2], 0xFF);

  // Verify that if a bitmap is already selected into another DC,
  // CreateIconFromHBitmap fails gracefully.
  base::win::ScopedCreateDC other_dc(::CreateCompatibleDC(dc));
  ASSERT_TRUE(other_dc.is_valid());
  HGDIOBJ old_selected = ::SelectObject(other_dc.get(), bmp24.get());
  if (old_selected && old_selected != HGDI_ERROR) {
    EXPECT_FALSE(CreateIconFromHBitmap(bmp24.get(), 32, 32).is_valid());
    ::SelectObject(other_dc.get(), old_selected);
  }

  // Verify that a 32bpp source bitmap is supported and converted.
  BITMAPINFO bi32 = {};
  bi32.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi32.bmiHeader.biWidth = 32;
  bi32.bmiHeader.biHeight = 32;
  bi32.bmiHeader.biPlanes = 1;
  bi32.bmiHeader.biBitCount = 32;
  bi32.bmiHeader.biCompression = BI_RGB;
  void* bits32 = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp32(
      ::CreateDIBSection(dc, &bi32, DIB_RGB_COLORS, &bits32, nullptr, 0));
  ASSERT_TRUE(bmp32.is_valid());
  EXPECT_TRUE(CreateIconFromHBitmap(bmp32.get(), 32, 32).is_valid());

  // Verify that an unsupported bit depth (e.g. 1bpp monochrome) is rejected.
  base::win::ScopedGDIObject<HBITMAP> bmp1(
      ::CreateBitmap(32, 32, 1, 1, nullptr));
  ASSERT_TRUE(bmp1.is_valid());
  EXPECT_FALSE(CreateIconFromHBitmap(bmp1.get(), 32, 32).is_valid());

  // Verify that passing width = 0 and height = 0 with an explicit DPI scales
  // to DPI-aware system metrics.
  constexpr UINT kCustomDpi = 192;
  base::win::ScopedGDIObject<HICON> icon_dpi =
      CreateIconFromHBitmap(bmp24.get(), 0, 0, kCustomDpi);
  ASSERT_TRUE(icon_dpi.is_valid());
  ICONINFO dpi_info = {};
  ASSERT_TRUE(::GetIconInfo(icon_dpi.get(), &dpi_info));
  base::win::ScopedGDIObject<HBITMAP> dpi_color(dpi_info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> dpi_mask(dpi_info.hbmMask);
  BITMAP dpi_bm = {};
  EXPECT_NE(::GetObject(dpi_color.get(), sizeof(dpi_bm), &dpi_bm), 0);
  EXPECT_EQ(dpi_bm.bmWidth, ::GetSystemMetricsForDpi(SM_CXICON, kCustomDpi));
  EXPECT_EQ(std::abs(dpi_bm.bmHeight),
            ::GetSystemMetricsForDpi(SM_CYICON, kCustomDpi));

  // Verify that a rectangular (non-square) source bitmap is fitted and centered
  // while preserving its aspect ratio, with transparent letterbox margins.
  // Test 1: Wide rectangular logo (92x24, matching app logos in updater).
  BITMAPINFO bi_wide = {};
  bi_wide.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi_wide.bmiHeader.biWidth = 92;
  bi_wide.bmiHeader.biHeight = 24;
  bi_wide.bmiHeader.biPlanes = 1;
  bi_wide.bmiHeader.biBitCount = 24;
  bi_wide.bmiHeader.biCompression = BI_RGB;
  void* bits_wide = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp_wide(
      ::CreateDIBSection(dc, &bi_wide, DIB_RGB_COLORS, &bits_wide, nullptr, 0));
  ASSERT_TRUE(bmp_wide.is_valid());
  ASSERT_NE(bits_wide, nullptr);
  BITMAP dib_bm_wide = {};
  ASSERT_NE(::GetObject(bmp_wide.get(), sizeof(dib_bm_wide), &dib_bm_wide), 0);
  const size_t bytes_wide =
      static_cast<size_t>(dib_bm_wide.bmWidthBytes) * dib_bm_wide.bmHeight;
  base::span<uint8_t> span_wide =
      UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(bits_wide), bytes_wide));
  std::ranges::fill(span_wide, 0x80);

  base::win::ScopedGDIObject<HICON> icon_wide =
      CreateIconFromHBitmap(bmp_wide.get(), 32, 32);
  ASSERT_TRUE(icon_wide.is_valid());
  ICONINFO wide_info = {};
  ASSERT_TRUE(::GetIconInfo(icon_wide.get(), &wide_info));
  base::win::ScopedGDIObject<HBITMAP> wide_color(wide_info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> wide_mask(wide_info.hbmMask);
  ASSERT_TRUE(wide_color.is_valid());
  ASSERT_TRUE(wide_mask.is_valid());

  // In 32x32 destination, 92x24 scales to 32x8:
  // dst_h = MulDiv(24, 32, 92) = 8.
  // Centered vertically: dst_y = (32 - 8) / 2 = 12.
  // Letterbox margins: visual rows 0..11 and 20..31.
  // Active image: visual rows 12..19.
  // In a bottom-up DIB (positive biHeight in GetDIBits):
  // DIB rows 0..11 are visual bottom (margin).
  // DIB rows 12..19 are visual image.
  // DIB rows 20..31 are visual top (margin).
  {
    struct {
      BITMAPINFOHEADER bmiHeader;
      RGBQUAD bmiColors[2];
    } mask_bi = {};
    mask_bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    mask_bi.bmiHeader.biWidth = 32;
    mask_bi.bmiHeader.biHeight = 32;
    mask_bi.bmiHeader.biPlanes = 1;
    mask_bi.bmiHeader.biBitCount = 1;
    mask_bi.bmiHeader.biCompression = BI_RGB;

    const size_t row_bytes = ((32 + 31) / 32) * 4;
    std::vector<uint8_t> mask_pixels(row_bytes * 32, 0);
    ASSERT_EQ(
        ::GetDIBits(dc, wide_mask.get(), 0, 32, mask_pixels.data(),
                    reinterpret_cast<BITMAPINFO*>(&mask_bi), DIB_RGB_COLORS),
        32);
    base::span<const uint8_t> mask_span(mask_pixels);
    for (size_t y = 0; y < 32; ++y) {
      base::span<const uint8_t> row = mask_span.subspan(y * row_bytes, 4u);
      const uint8_t expected_byte = (y >= 12 && y < 20) ? 0x00 : 0xFF;
      for (uint8_t byte : row) {
        EXPECT_EQ(byte, expected_byte) << "Mask mismatch at row " << y;
      }
    }

    BITMAPINFO color_bi = {};
    color_bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    color_bi.bmiHeader.biWidth = 32;
    color_bi.bmiHeader.biHeight = 32;
    color_bi.bmiHeader.biPlanes = 1;
    color_bi.bmiHeader.biBitCount = 24;
    color_bi.bmiHeader.biCompression = BI_RGB;
    const size_t color_row_bytes = ((32 * 24 + 31) / 32) * 4;
    std::vector<uint8_t> color_pixels(color_row_bytes * 32, 0);
    ASSERT_EQ(::GetDIBits(dc, wide_color.get(), 0, 32, color_pixels.data(),
                          &color_bi, DIB_RGB_COLORS),
              32);
    base::span<const uint8_t> color_span(color_pixels);
    for (size_t y = 0; y < 32; ++y) {
      base::span<const uint8_t> row =
          color_span.subspan(y * color_row_bytes, 32u * 3u);
      if (y >= 12 && y < 20) {
        EXPECT_NE(row[0], 0u) << "Expected non-zero color in image row " << y;
      } else {
        for (size_t x = 0; x < 32 * 3; ++x) {
          EXPECT_EQ(row[x], 0u) << "Margin pixel non-zero at row " << y;
        }
      }
    }
  }

  // Test 2: Tall rectangular logo (24x92).
  BITMAPINFO bi_tall = {};
  bi_tall.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi_tall.bmiHeader.biWidth = 24;
  bi_tall.bmiHeader.biHeight = 92;
  bi_tall.bmiHeader.biPlanes = 1;
  bi_tall.bmiHeader.biBitCount = 24;
  bi_tall.bmiHeader.biCompression = BI_RGB;
  void* bits_tall = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp_tall(
      ::CreateDIBSection(dc, &bi_tall, DIB_RGB_COLORS, &bits_tall, nullptr, 0));
  ASSERT_TRUE(bmp_tall.is_valid());
  ASSERT_NE(bits_tall, nullptr);
  BITMAP dib_bm_tall = {};
  ASSERT_NE(::GetObject(bmp_tall.get(), sizeof(dib_bm_tall), &dib_bm_tall), 0);
  const size_t bytes_tall =
      static_cast<size_t>(dib_bm_tall.bmWidthBytes) * dib_bm_tall.bmHeight;
  base::span<uint8_t> span_tall =
      UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(bits_tall), bytes_tall));
  std::ranges::fill(span_tall, 0x80);

  base::win::ScopedGDIObject<HICON> icon_tall =
      CreateIconFromHBitmap(bmp_tall.get(), 32, 32);
  ASSERT_TRUE(icon_tall.is_valid());
  ICONINFO tall_info = {};
  ASSERT_TRUE(::GetIconInfo(icon_tall.get(), &tall_info));
  base::win::ScopedGDIObject<HBITMAP> tall_color(tall_info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> tall_mask(tall_info.hbmMask);
  ASSERT_TRUE(tall_color.is_valid());
  ASSERT_TRUE(tall_mask.is_valid());

  // In 32x32 destination, 24x92 scales to 8x32:
  // dst_w = MulDiv(24, 32, 92) = 8.
  // Centered horizontally: dst_x = (32 - 8) / 2 = 12.
  // Left margin: cols 0..11, Right margin: cols 20..31.
  // Active image: cols 12..19.
  // In 1bpp mask (32 bits per row = 4 bytes):
  // Byte 0 (cols 0..7): all 1s (0xFF).
  // Byte 1 (cols 8..15): cols 8..11 are 1, cols 12..15 are 0 -> 0xF0.
  // Byte 2 (cols 16..23): cols 16..19 are 0, cols 20..23 are 1 -> 0x0F.
  // Byte 3 (cols 24..31): all 1s (0xFF).
  {
    struct {
      BITMAPINFOHEADER bmiHeader;
      RGBQUAD bmiColors[2];
    } mask_bi = {};
    mask_bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    mask_bi.bmiHeader.biWidth = 32;
    mask_bi.bmiHeader.biHeight = 32;
    mask_bi.bmiHeader.biPlanes = 1;
    mask_bi.bmiHeader.biBitCount = 1;
    mask_bi.bmiHeader.biCompression = BI_RGB;

    const size_t row_bytes = ((32 + 31) / 32) * 4;
    std::vector<uint8_t> mask_pixels(row_bytes * 32, 0);
    ASSERT_EQ(
        ::GetDIBits(dc, tall_mask.get(), 0, 32, mask_pixels.data(),
                    reinterpret_cast<BITMAPINFO*>(&mask_bi), DIB_RGB_COLORS),
        32);
    base::span<const uint8_t> mask_span(mask_pixels);
    for (size_t y = 0; y < 32; ++y) {
      base::span<const uint8_t> row = mask_span.subspan(y * row_bytes, 4u);
      EXPECT_EQ(row[0], 0xFF) << "Row " << y << " byte 0";
      EXPECT_EQ(row[1], 0xF0) << "Row " << y << " byte 1";
      EXPECT_EQ(row[2], 0x0F) << "Row " << y << " byte 2";
      EXPECT_EQ(row[3], 0xFF) << "Row " << y << " byte 3";
    }
  }
}

TEST(UiUtilTest, IconHelpers) {
  // Test GetIconSizesForDpi.
  const IconSizes sizes_default = GetIconSizesForDpi(0);
  EXPECT_EQ(sizes_default.cx_big, ::GetSystemMetrics(SM_CXICON));
  EXPECT_EQ(sizes_default.cy_big, ::GetSystemMetrics(SM_CYICON));
  EXPECT_EQ(sizes_default.cx_small, ::GetSystemMetrics(SM_CXSMICON));
  EXPECT_EQ(sizes_default.cy_small, ::GetSystemMetrics(SM_CYSMICON));

  constexpr UINT kDpi = 192;
  const IconSizes sizes_192 = GetIconSizesForDpi(kDpi);
  EXPECT_EQ(sizes_192.cx_big, ::GetSystemMetricsForDpi(SM_CXICON, kDpi));
  EXPECT_EQ(sizes_192.cy_big, ::GetSystemMetricsForDpi(SM_CYICON, kDpi));
  EXPECT_EQ(sizes_192.cx_small, ::GetSystemMetricsForDpi(SM_CXSMICON, kDpi));
  EXPECT_EQ(sizes_192.cy_small, ::GetSystemMetricsForDpi(SM_CYSMICON, kDpi));

  // Invalid resource ID returns empty handles.
  WindowIcons invalid_icons = LoadResourceIcons(-1);
  EXPECT_FALSE(invalid_icons.icon_big.is_valid());
  EXPECT_FALSE(invalid_icons.icon_small.is_valid());
}

TEST(UiUtilTest, SetWindowIcons) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  HWND hwnd = ::CreateWindowEx(0, L"STATIC", L"IconTest", WS_POPUP, 0, 0, 100,
                               100, nullptr, nullptr, nullptr, nullptr);
  ASSERT_TRUE(hwnd);
  const absl::Cleanup destroy_wnd = [&] { ::DestroyWindow(hwnd); };

  base::win::ScopedGetDC dc(nullptr);
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = 16;
  bi.bmiHeader.biHeight = 16;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 24;
  bi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp(
      ::CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0));
  ASSERT_TRUE(bmp.is_valid());

  WindowIcons icons1;
  icons1.icon_big = CreateIconFromHBitmap(bmp.get(), 32, 32);
  icons1.icon_small = CreateIconFromHBitmap(bmp.get(), 16, 16);
  ASSERT_TRUE(icons1.icon_big.is_valid());
  ASSERT_TRUE(icons1.icon_small.is_valid());

  const HICON raw_big1 = icons1.icon_big.get();
  const HICON raw_small1 = icons1.icon_small.get();

  WindowIcons current_icons;
  SetWindowIcons(hwnd, std::move(icons1), current_icons);

  EXPECT_EQ(current_icons.icon_big.get(), raw_big1);
  EXPECT_EQ(current_icons.icon_small.get(), raw_small1);
  EXPECT_EQ(
      reinterpret_cast<HICON>(::SendMessage(hwnd, WM_GETICON, ICON_BIG, 0)),
      raw_big1);
  EXPECT_EQ(
      reinterpret_cast<HICON>(::SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0)),
      raw_small1);

  WindowIcons icons2;
  icons2.icon_big = CreateIconFromHBitmap(bmp.get(), 32, 32);
  icons2.icon_small = CreateIconFromHBitmap(bmp.get(), 16, 16);
  ASSERT_TRUE(icons2.icon_big.is_valid());
  ASSERT_TRUE(icons2.icon_small.is_valid());

  const HICON raw_big2 = icons2.icon_big.get();
  const HICON raw_small2 = icons2.icon_small.get();

  SetWindowIcons(hwnd, std::move(icons2), current_icons);

  EXPECT_EQ(current_icons.icon_big.get(), raw_big2);
  EXPECT_EQ(current_icons.icon_small.get(), raw_small2);
  EXPECT_EQ(
      reinterpret_cast<HICON>(::SendMessage(hwnd, WM_GETICON, ICON_BIG, 0)),
      raw_big2);
  EXPECT_EQ(
      reinterpret_cast<HICON>(::SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0)),
      raw_small2);
}

}  // namespace updater::ui

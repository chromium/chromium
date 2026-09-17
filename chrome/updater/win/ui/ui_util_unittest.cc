// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/ui_util.h"

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
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

TEST(UiUtilTest, GetBadgeRectAndBaseLogoDimensions) {
  // Target 32x32:
  const RECT badge_32 = GetBadgeRect(32, 32);
  EXPECT_EQ(badge_32.left, 16);
  EXPECT_EQ(badge_32.top, 0);
  EXPECT_EQ(badge_32.right, 32);
  EXPECT_EQ(badge_32.bottom, 16);

  const SIZE base_32 = GetBaseLogoDimensions(32, 32);
  EXPECT_EQ(base_32.cx, 24);
  EXPECT_EQ(base_32.cy, 24);

  // Target 16x16:
  const RECT badge_16 = GetBadgeRect(16, 16);
  EXPECT_EQ(badge_16.left, 8);
  EXPECT_EQ(badge_16.top, 0);
  EXPECT_EQ(badge_16.right, 16);
  EXPECT_EQ(badge_16.bottom, 8);

  const SIZE base_16 = GetBaseLogoDimensions(16, 16);
  EXPECT_EQ(base_16.cx, 12);
  EXPECT_EQ(base_16.cy, 12);

  // Edge case 1x1:
  const RECT badge_1 = GetBadgeRect(1, 1);
  EXPECT_EQ(badge_1.left, 0);
  EXPECT_EQ(badge_1.top, 0);
  EXPECT_EQ(badge_1.right, 1);
  EXPECT_EQ(badge_1.bottom, 1);

  const SIZE base_1 = GetBaseLogoDimensions(1, 1);
  EXPECT_EQ(base_1.cx, 1);
  EXPECT_EQ(base_1.cy, 1);

  // Non-positive dimensions yield empty results rather than negative geometry.
  for (const auto& [w, h] : {std::pair(0, 32), std::pair(32, 0),
                             std::pair(-32, 32), std::pair(32, -32)}) {
    const RECT badge = GetBadgeRect(w, h);
    EXPECT_EQ(badge.left, 0);
    EXPECT_EQ(badge.top, 0);
    EXPECT_EQ(badge.right, 0);
    EXPECT_EQ(badge.bottom, 0);

    const SIZE base = GetBaseLogoDimensions(w, h);
    EXPECT_EQ(base.cx, 0);
    EXPECT_EQ(base.cy, 0);
  }
}

TEST(UiUtilTest, IsSystemDarkModeOn) {
  if (IsHighContrastOn()) {
    GTEST_SKIP();
  }
  registry_util::RegistryOverrideManager registry_override;
  ASSERT_NO_FATAL_FAILURE(
      registry_override.OverrideRegistry(HKEY_CURRENT_USER));

  base::win::RegKey key;
  ASSERT_EQ(key.Create(HKEY_CURRENT_USER,
                       L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes"
                       L"\\Personalize",
                       KEY_SET_VALUE),
            ERROR_SUCCESS);

  // SystemUsesLightTheme = 0 -> Dark mode.
  ASSERT_EQ(key.WriteValue(L"SystemUsesLightTheme", static_cast<DWORD>(0)),
            ERROR_SUCCESS);
  EXPECT_TRUE(IsSystemDarkModeOn());

  // SystemUsesLightTheme = 1 -> Light mode.
  ASSERT_EQ(key.WriteValue(L"SystemUsesLightTheme", static_cast<DWORD>(1)),
            ERROR_SUCCESS);
  EXPECT_FALSE(IsSystemDarkModeOn());

  // If SystemUsesLightTheme is missing, falls back to AppsUseLightTheme /
  // IsDarkModeOn().
  ASSERT_EQ(key.DeleteValue(L"SystemUsesLightTheme"), ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"AppsUseLightTheme", static_cast<DWORD>(0)),
            ERROR_SUCCESS);
  EXPECT_TRUE(IsSystemDarkModeOn());

  ASSERT_EQ(key.WriteValue(L"AppsUseLightTheme", static_cast<DWORD>(1)),
            ERROR_SUCCESS);
  EXPECT_FALSE(IsSystemDarkModeOn());
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
  // SAFETY: `bmp24` is a 24bpp DIB section allocated immediately above with
  // byte size exactly equal to `bytes24`.
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
    EXPECT_EQ(mask_bm.bmWidthBytes,
              static_cast<int>(CalculateDDBStride(width)));

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

    const size_t row_bytes = CalculateDIBStride(width, 1);
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
  // SAFETY: `bmp24_top_down` is a 24bpp DIB section allocated immediately above
  // with byte size exactly equal to `bytes24_top_down`.
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
  const size_t inspect_row_bytes = CalculateDIBStride(32, 24);
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
  // SAFETY: `bmp_wide` is a 24bpp DIB section allocated immediately above with
  // byte size exactly equal to `bytes_wide`.
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

    const size_t row_bytes = CalculateDIBStride(32, 1);
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
    const size_t color_row_bytes = CalculateDIBStride(32, 24);
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
  // SAFETY: `bmp_tall` is a 24bpp DIB section allocated immediately above with
  // byte size exactly equal to `bytes_tall`.
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

    const size_t row_bytes = CalculateDIBStride(32, 1);
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

TEST(UiUtilTest, CreateIconFromBitmap_AlphaChannel) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  base::win::ScopedGetDC dc(nullptr);

  // Create a 32x32 32bpp DIB with per-pixel alpha variation.
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
  ASSERT_NE(bits32, nullptr);

  // Bottom-up DIB:
  // DIB rows 0..15 (visual bottom):
  //   x in [0..15]: opaque green (0xFF00FF00)
  //   x in [16..31]: opaque blue (0xFF0000FF)
  // DIB rows 16..31 (visual top):
  //   x in [0..15]: fully transparent (0x00000000)
  //   x in [16..31]: semi-transparent red (0x80FF0000)
  // SAFETY: `bmp32` is a 32bpp DIB section of 32x32 pixels, containing
  // exactly 1024 32-bit DWORDs.
  base::span<uint32_t> pixels32 =
      UNSAFE_BUFFERS(base::span(static_cast<uint32_t*>(bits32), 32u * 32u));
  for (size_t y = 0; y < 16; ++y) {
    for (size_t x = 0; x < 16; ++x) {
      pixels32[y * 32 + x] = 0xFF00FF00;
    }
    for (size_t x = 16; x < 32; ++x) {
      pixels32[y * 32 + x] = 0xFF0000FF;
    }
  }
  for (size_t y = 16; y < 32; ++y) {
    for (size_t x = 0; x < 16; ++x) {
      pixels32[y * 32 + x] = 0x00000000;
    }
    for (size_t x = 16; x < 32; ++x) {
      pixels32[y * 32 + x] = 0x80FF0000;
    }
  }

  base::win::ScopedGDIObject<HICON> icon =
      CreateIconFromHBitmap(bmp32.get(), 32, 32);
  ASSERT_TRUE(icon.is_valid());

  ICONINFO info = {};
  ASSERT_TRUE(::GetIconInfo(icon.get(), &info));
  base::win::ScopedGDIObject<HBITMAP> color_bmp(info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);
  ASSERT_TRUE(color_bmp.is_valid());
  ASSERT_TRUE(mask_bmp.is_valid());

  BITMAP bm_color = {};
  ASSERT_NE(::GetObject(color_bmp.get(), sizeof(bm_color), &bm_color), 0);
  EXPECT_EQ(bm_color.bmWidth, 32);
  EXPECT_EQ(bm_color.bmHeight, 32);
  EXPECT_EQ(bm_color.bmBitsPixel, 32);

  // Read back color pixels using a 32bpp header.
  std::vector<uint32_t> color_pixels(32u * 32u, 0);
  ASSERT_EQ(::GetDIBits(dc, color_bmp.get(), 0, 32, color_pixels.data(), &bi32,
                        DIB_RGB_COLORS),
            32);

  // In bottom-up DIB rows:
  // Visual top-left (x=8, visual_y=8 -> dib_y = 23): fully transparent.
  EXPECT_EQ(color_pixels[23 * 32 + 8] >> 24, 0u);

  const uint8_t red_alpha =
      static_cast<uint8_t>(color_pixels[23 * 32 + 24] >> 24);
  EXPECT_GE(red_alpha, 126u);
  EXPECT_LE(red_alpha, 130u);
  // Red is premultiplied by alpha for Windows DWM: (255 * 128) / 255 = 128.
  const uint8_t red_val =
      static_cast<uint8_t>((color_pixels[23 * 32 + 24] >> 16) & 0xFF);
  EXPECT_GE(red_val, 126u);
  EXPECT_LE(red_val, 130u);

  // Visual bottom-left (x=8, visual_y=24 -> dib_y = 7): opaque green.
  EXPECT_EQ(color_pixels[7 * 32 + 8] >> 24, 255u);
  EXPECT_GT((color_pixels[7 * 32 + 8] >> 8) & 0xFF, 200u);

  // Visual bottom-right (x=24, visual_y=24 -> dib_y = 7): opaque blue.
  EXPECT_EQ(color_pixels[7 * 32 + 24] >> 24, 255u);
  EXPECT_GT(color_pixels[7 * 32 + 24] & 0xFF, 200u);

  // Verify 1bpp mask is all 0s (transparency handled by 32bpp alpha channel).
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

  const size_t row_bytes = CalculateDIBStride(32, 1);
  std::vector<uint8_t> mask_pixels(row_bytes * 32, 0xFF);
  ASSERT_EQ(
      ::GetDIBits(dc, mask_bmp.get(), 0, 32, mask_pixels.data(),
                  reinterpret_cast<BITMAPINFO*>(&mask_bi), DIB_RGB_COLORS),
      32);
  for (uint8_t byte : mask_pixels) {
    EXPECT_EQ(byte, 0u);
  }
}

TEST(UiUtilTest, CreateIconFromBitmap_ColorKeying) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  base::win::ScopedGetDC dc(nullptr);

  // Test 1: 24bpp bitmap with white background and an interior enclosed white
  // region (simulating the Chrome logo with an outer background and an enclosed
  // white ring between the center and outer circle).
  {
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
    const size_t stride = dib_bm24.bmWidthBytes;
    const size_t bytes24 = stride * dib_bm24.bmHeight;
    // SAFETY: `bmp24` is a 24bpp DIB section allocated immediately above with
    // byte size exactly equal to `bytes24`.
    base::span<uint8_t> span24 =
        UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(bits24), bytes24));

    auto set_pixel = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
      const size_t dib_y = 47 - y;
      const size_t offset = dib_y * stride + static_cast<size_t>(x) * 3;
      span24[offset + 0] = b;
      span24[offset + 1] = g;
      span24[offset + 2] = r;
    };

    // 1. Fill entire image with white (RGB 255, 255, 255).
    for (int y = 0; y < 48; ++y) {
      for (int x = 0; x < 48; ++x) {
        set_pixel(x, y, 255, 255, 255);
      }
    }

    // 2. Draw a red ring enclosing the center: [10..37, 10..37].
    for (int y = 10; y <= 37; ++y) {
      for (int x = 10; x <= 37; ++x) {
        set_pixel(x, y, 255, 0, 0);
      }
    }

    // 3. Draw an enclosed white region inside the red ring: [18..29, 18..29].
    for (int y = 18; y <= 29; ++y) {
      for (int x = 18; x <= 29; ++x) {
        set_pixel(x, y, 255, 255, 255);
      }
    }

    // 4. Draw a blue center inside the enclosed white region: [22..25, 22..25].
    for (int y = 22; y <= 25; ++y) {
      for (int x = 22; x <= 25; ++x) {
        set_pixel(x, y, 0, 0, 255);
      }
    }

    base::win::ScopedGDIObject<HICON> icon =
        CreateIconFromHBitmap(bmp24.get(), 48, 48);
    ASSERT_TRUE(icon.is_valid());

    ICONINFO info = {};
    ASSERT_TRUE(::GetIconInfo(icon.get(), &info));
    base::win::ScopedGDIObject<HBITMAP> color_bmp(info.hbmColor);
    base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);
    ASSERT_TRUE(color_bmp.is_valid());
    ASSERT_TRUE(mask_bmp.is_valid());

    struct {
      BITMAPINFOHEADER bmiHeader;
      RGBQUAD bmiColors[2];
    } mask_bi = {};
    mask_bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    mask_bi.bmiHeader.biWidth = 48;
    mask_bi.bmiHeader.biHeight = 48;
    mask_bi.bmiHeader.biPlanes = 1;
    mask_bi.bmiHeader.biBitCount = 1;
    mask_bi.bmiHeader.biCompression = BI_RGB;

    const size_t mask_stride = CalculateDIBStride(48, 1);
    std::vector<uint8_t> mask_pixels(mask_stride * 48, 0);
    ASSERT_EQ(
        ::GetDIBits(dc, mask_bmp.get(), 0, 48, mask_pixels.data(),
                    reinterpret_cast<BITMAPINFO*>(&mask_bi), DIB_RGB_COLORS),
        48);

    BITMAPINFO color_bi = {};
    color_bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    color_bi.bmiHeader.biWidth = 48;
    color_bi.bmiHeader.biHeight = 48;
    color_bi.bmiHeader.biPlanes = 1;
    color_bi.bmiHeader.biBitCount = 24;
    color_bi.bmiHeader.biCompression = BI_RGB;

    const size_t color_stride = CalculateDIBStride(48, 24);
    std::vector<uint8_t> color_pixels(color_stride * 48, 0);
    ASSERT_EQ(::GetDIBits(dc, color_bmp.get(), 0, 48, color_pixels.data(),
                          &color_bi, DIB_RGB_COLORS),
              48);

    auto get_mask_bit = [&](int x, int y) -> bool {
      const size_t dib_y = 47 - y;
      const uint8_t byte = mask_pixels[dib_y * mask_stride + (x / 8)];
      return (byte & (1 << (7 - (x % 8)))) != 0;
    };

    auto get_color = [&](int x, int y) -> COLORREF {
      const size_t dib_y = 47 - y;
      const size_t offset = dib_y * color_stride + static_cast<size_t>(x) * 3;
      return RGB(color_pixels[offset + 2], color_pixels[offset + 1],
                 color_pixels[offset + 0]);
    };

    // A. Outer white background is transparent (mask bit = 1, color = 0).
    EXPECT_TRUE(get_mask_bit(0, 0));
    EXPECT_EQ(get_color(0, 0), RGB(0, 0, 0));
    EXPECT_TRUE(get_mask_bit(47, 0));
    EXPECT_EQ(get_color(47, 0), RGB(0, 0, 0));
    EXPECT_TRUE(get_mask_bit(0, 47));
    EXPECT_EQ(get_color(0, 47), RGB(0, 0, 0));
    EXPECT_TRUE(get_mask_bit(47, 47));
    EXPECT_EQ(get_color(47, 47), RGB(0, 0, 0));

    // B. Red ring is opaque (mask bit = 0, color = red).
    EXPECT_FALSE(get_mask_bit(12, 12));
    EXPECT_EQ(get_color(12, 12), RGB(255, 0, 0));

    // C. Enclosed white region is opaque (mask bit = 0, color = white).
    EXPECT_FALSE(get_mask_bit(20, 20));
    EXPECT_EQ(get_color(20, 20), RGB(255, 255, 255));

    // D. Blue center is opaque (mask bit = 0, color = blue).
    EXPECT_FALSE(get_mask_bit(23, 23));
    EXPECT_EQ(get_color(23, 23), RGB(0, 0, 255));
  }

  // Test 2: Dark dialog background auto-detection (RGB 31, 31, 31).
  {
    BITMAPINFO bi24 = {};
    bi24.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi24.bmiHeader.biWidth = 32;
    bi24.bmiHeader.biHeight = 32;
    bi24.bmiHeader.biPlanes = 1;
    bi24.bmiHeader.biBitCount = 24;
    bi24.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    base::win::ScopedGDIObject<HBITMAP> bmp(
        ::CreateDIBSection(dc, &bi24, DIB_RGB_COLORS, &bits, nullptr, 0));
    ASSERT_TRUE(bmp.is_valid());
    const size_t stride = CalculateDIBStride(32, 24);
    // SAFETY: `bmp` is a 24bpp DIB section of 32x32 pixels, with byte size
    // exactly equal to `stride * 32`.
    base::span<uint8_t> span =
        UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(bits), stride * 32));
    // Fill with RGB(31, 31, 31).
    std::ranges::fill(span, 31);

    // Center green square [10..21, 10..21].
    for (int y = 10; y <= 21; ++y) {
      const size_t dib_y = 31 - y;
      for (int x = 10; x <= 21; ++x) {
        span[dib_y * stride + x * 3 + 0] = 0;
        span[dib_y * stride + x * 3 + 1] = 255;
        span[dib_y * stride + x * 3 + 2] = 0;
      }
    }

    base::win::ScopedGDIObject<HICON> icon =
        CreateIconFromHBitmap(bmp.get(), 32, 32);
    ASSERT_TRUE(icon.is_valid());

    ICONINFO info = {};
    ASSERT_TRUE(::GetIconInfo(icon.get(), &info));
    base::win::ScopedGDIObject<HBITMAP> color_bmp(info.hbmColor);
    base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);

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

    const size_t mask_stride = CalculateDIBStride(32, 1);
    std::vector<uint8_t> mask_pixels(mask_stride * 32, 0);
    ASSERT_EQ(
        ::GetDIBits(dc, mask_bmp.get(), 0, 32, mask_pixels.data(),
                    reinterpret_cast<BITMAPINFO*>(&mask_bi), DIB_RGB_COLORS),
        32);

    auto get_mask_bit = [&](int x, int y) -> bool {
      const size_t dib_y = 31 - y;
      const uint8_t byte = mask_pixels[dib_y * mask_stride + (x / 8)];
      return (byte & (1 << (7 - (x % 8)))) != 0;
    };

    EXPECT_TRUE(get_mask_bit(0, 0));
    EXPECT_FALSE(get_mask_bit(16, 16));
  }

  // Test 3: Explicit transparent_color override (e.g. Magenta RGB 255, 0, 255).
  {
    BITMAPINFO bi24 = {};
    bi24.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi24.bmiHeader.biWidth = 32;
    bi24.bmiHeader.biHeight = 32;
    bi24.bmiHeader.biPlanes = 1;
    bi24.bmiHeader.biBitCount = 24;
    bi24.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    base::win::ScopedGDIObject<HBITMAP> bmp(
        ::CreateDIBSection(dc, &bi24, DIB_RGB_COLORS, &bits, nullptr, 0));
    ASSERT_TRUE(bmp.is_valid());
    const size_t stride = CalculateDIBStride(32, 24);
    // SAFETY: `bmp` is a 24bpp DIB section of 32x32 pixels, with byte size
    // exactly equal to `stride * 32`.
    base::span<uint8_t> span =
        UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(bits), stride * 32));

    // Fill with Magenta RGB(255, 0, 255) -> BGR: (255, 0, 255).
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        span[y * stride + x * 3 + 0] = 255;
        span[y * stride + x * 3 + 1] = 0;
        span[y * stride + x * 3 + 2] = 255;
      }
    }

    // Center Cyan square RGB(0, 255, 255) -> BGR: (255, 255, 0).
    for (int y = 10; y <= 21; ++y) {
      for (int x = 10; x <= 21; ++x) {
        span[y * stride + x * 3 + 0] = 255;
        span[y * stride + x * 3 + 1] = 255;
        span[y * stride + x * 3 + 2] = 0;
      }
    }

    base::win::ScopedGDIObject<HICON> icon =
        CreateIconFromHBitmap(bmp.get(), 32, 32, /*dpi=*/0, RGB(255, 0, 255));
    ASSERT_TRUE(icon.is_valid());

    ICONINFO info = {};
    ASSERT_TRUE(::GetIconInfo(icon.get(), &info));
    base::win::ScopedGDIObject<HBITMAP> color_bmp(info.hbmColor);
    base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);

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

    const size_t mask_stride = CalculateDIBStride(32, 1);
    std::vector<uint8_t> mask_pixels(mask_stride * 32, 0);
    ASSERT_EQ(
        ::GetDIBits(dc, mask_bmp.get(), 0, 32, mask_pixels.data(),
                    reinterpret_cast<BITMAPINFO*>(&mask_bi), DIB_RGB_COLORS),
        32);

    auto get_mask_bit = [&](int x, int y) -> bool {
      const size_t dib_y = 31 - y;
      const uint8_t byte = mask_pixels[dib_y * mask_stride + (x / 8)];
      return (byte & (1 << (7 - (x % 8)))) != 0;
    };

    EXPECT_TRUE(get_mask_bit(0, 0));
    EXPECT_FALSE(get_mask_bit(16, 16));
  }

  // Test 4: Opaque 32bpp bitmap (all alpha = 0) with background color keying.
  {
    BITMAPINFO bi32 = {};
    bi32.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi32.bmiHeader.biWidth = 32;
    bi32.bmiHeader.biHeight = 32;
    bi32.bmiHeader.biPlanes = 1;
    bi32.bmiHeader.biBitCount = 32;
    bi32.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    base::win::ScopedGDIObject<HBITMAP> bmp(
        ::CreateDIBSection(dc, &bi32, DIB_RGB_COLORS, &bits, nullptr, 0));
    ASSERT_TRUE(bmp.is_valid());
    // SAFETY: `bmp` is a 32bpp DIB section of 32x32 pixels, containing exactly
    // 1024 32-bit DWORDs.
    base::span<uint32_t> span =
        UNSAFE_BUFFERS(base::span(static_cast<uint32_t*>(bits), 32u * 32u));

    // Fill with white RGB(255, 255, 255) with alpha = 0 (opaque 32bpp).
    std::ranges::fill(span, 0x00FFFFFF);

    // Center Red square.
    for (int y = 10; y <= 21; ++y) {
      for (int x = 10; x <= 21; ++x) {
        span[y * 32 + x] = 0x00FF0000;
      }
    }

    base::win::ScopedGDIObject<HICON> icon =
        CreateIconFromHBitmap(bmp.get(), 32, 32);
    ASSERT_TRUE(icon.is_valid());

    ICONINFO info = {};
    ASSERT_TRUE(::GetIconInfo(icon.get(), &info));
    base::win::ScopedGDIObject<HBITMAP> color_bmp(info.hbmColor);
    base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);

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

    const size_t mask_stride = CalculateDIBStride(32, 1);
    std::vector<uint8_t> mask_pixels(mask_stride * 32, 0);
    ASSERT_EQ(
        ::GetDIBits(dc, mask_bmp.get(), 0, 32, mask_pixels.data(),
                    reinterpret_cast<BITMAPINFO*>(&mask_bi), DIB_RGB_COLORS),
        32);

    auto get_mask_bit = [&](int x, int y) -> bool {
      const size_t dib_y = 31 - y;
      const uint8_t byte = mask_pixels[dib_y * mask_stride + (x / 8)];
      return (byte & (1 << (7 - (x % 8)))) != 0;
    };

    EXPECT_TRUE(get_mask_bit(0, 0));
    EXPECT_FALSE(get_mask_bit(16, 16));
  }

  // Test 5: Custom solid background color where all 4 corners match but color
  // is not a known dialog background color (e.g. custom light gray RGB 240,
  // 240, 240).
  {
    BITMAPINFO bi24 = {};
    bi24.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi24.bmiHeader.biWidth = 32;
    bi24.bmiHeader.biHeight = 32;
    bi24.bmiHeader.biPlanes = 1;
    bi24.bmiHeader.biBitCount = 24;
    bi24.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    base::win::ScopedGDIObject<HBITMAP> bmp(
        ::CreateDIBSection(dc, &bi24, DIB_RGB_COLORS, &bits, nullptr, 0));
    ASSERT_TRUE(bmp.is_valid());
    const size_t stride = CalculateDIBStride(32, 24);
    // SAFETY: `bmp` is a 24bpp DIB section of 32x32 pixels, with byte size
    // exactly equal to `stride * 32`.
    base::span<uint8_t> span =
        UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(bits), stride * 32));

    // Fill with custom gray RGB(240, 240, 240) (not kLightDialogBg or
    // kDarkDialogBg).
    std::ranges::fill(span, 240);

    // Center Green square RGB(0, 255, 0).
    for (int y = 10; y <= 21; ++y) {
      const size_t dib_y = 31 - y;
      for (int x = 10; x <= 21; ++x) {
        span[dib_y * stride + x * 3 + 0] = 0;
        span[dib_y * stride + x * 3 + 1] = 255;
        span[dib_y * stride + x * 3 + 2] = 0;
      }
    }

    base::win::ScopedGDIObject<HICON> icon =
        CreateIconFromHBitmap(bmp.get(), 32, 32);
    ASSERT_TRUE(icon.is_valid());

    ICONINFO info = {};
    ASSERT_TRUE(::GetIconInfo(icon.get(), &info));
    base::win::ScopedGDIObject<HBITMAP> color_bmp(info.hbmColor);
    base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);

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

    const size_t mask_stride = CalculateDIBStride(32, 1);
    std::vector<uint8_t> mask_pixels(mask_stride * 32, 0);
    ASSERT_EQ(
        ::GetDIBits(dc, mask_bmp.get(), 0, 32, mask_pixels.data(),
                    reinterpret_cast<BITMAPINFO*>(&mask_bi), DIB_RGB_COLORS),
        32);

    auto get_mask_bit = [&](int x, int y) -> bool {
      const size_t dib_y = 31 - y;
      const uint8_t byte = mask_pixels[dib_y * mask_stride + (x / 8)];
      return (byte & (1 << (7 - (x % 8)))) != 0;
    };

    // The corner (0, 0) should be keyed out as transparent (bit = 1).
    EXPECT_TRUE(get_mask_bit(0, 0));
    // The center (16, 16) should remain opaque (bit = 0).
    EXPECT_FALSE(get_mask_bit(16, 16));
  }

  // Test 6: 16x16 icon mask verification. Ensures 16-bit WORD scanline
  // alignment in CreateBitmap (stride 2) avoids row-offset corruption against
  // 32-bit DWORD alignment (stride 4).
  {
    BITMAPINFO bi24 = {};
    bi24.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi24.bmiHeader.biWidth = 16;
    bi24.bmiHeader.biHeight = 16;
    bi24.bmiHeader.biPlanes = 1;
    bi24.bmiHeader.biBitCount = 24;
    bi24.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    base::win::ScopedGDIObject<HBITMAP> bmp(
        ::CreateDIBSection(dc, &bi24, DIB_RGB_COLORS, &bits, nullptr, 0));
    ASSERT_TRUE(bmp.is_valid());
    const size_t stride = CalculateDIBStride(16, 24);
    // SAFETY: `bmp` is a 24bpp DIB section of 16x16 pixels, with byte size
    // exactly equal to `stride * 16`.
    base::span<uint8_t> span =
        UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(bits), stride * 16));

    // Fill entire image with white RGB(255, 255, 255).
    std::ranges::fill(span, 255);

    // Center red square [4..11, 4..11].
    for (int y = 4; y <= 11; ++y) {
      const size_t dib_y = 15 - y;
      for (int x = 4; x <= 11; ++x) {
        span[dib_y * stride + x * 3 + 0] = 0;
        span[dib_y * stride + x * 3 + 1] = 0;
        span[dib_y * stride + x * 3 + 2] = 255;
      }
    }

    base::win::ScopedGDIObject<HICON> icon =
        CreateIconFromHBitmap(bmp.get(), 16, 16);
    ASSERT_TRUE(icon.is_valid());

    ICONINFO info = {};
    ASSERT_TRUE(::GetIconInfo(icon.get(), &info));
    base::win::ScopedGDIObject<HBITMAP> color_bmp(info.hbmColor);
    base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);

    struct {
      BITMAPINFOHEADER bmiHeader;
      RGBQUAD bmiColors[2];
    } mask_bi = {};
    mask_bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    mask_bi.bmiHeader.biWidth = 16;
    mask_bi.bmiHeader.biHeight = 16;
    mask_bi.bmiHeader.biPlanes = 1;
    mask_bi.bmiHeader.biBitCount = 1;
    mask_bi.bmiHeader.biCompression = BI_RGB;

    const size_t mask_stride = CalculateDIBStride(16, 1);
    std::vector<uint8_t> mask_pixels(mask_stride * 16, 0);
    ASSERT_EQ(
        ::GetDIBits(dc, mask_bmp.get(), 0, 16, mask_pixels.data(),
                    reinterpret_cast<BITMAPINFO*>(&mask_bi), DIB_RGB_COLORS),
        16);

    auto get_mask_bit = [&](int x, int y) -> bool {
      const size_t dib_y = 15 - y;
      const uint8_t byte = mask_pixels[dib_y * mask_stride + (x / 8)];
      return (byte & (1 << (7 - (x % 8)))) != 0;
    };

    // Corners should be keyed out as transparent (bit = 1).
    EXPECT_TRUE(get_mask_bit(0, 0));
    EXPECT_TRUE(get_mask_bit(15, 0));
    EXPECT_TRUE(get_mask_bit(0, 15));
    EXPECT_TRUE(get_mask_bit(15, 15));

    // Non-corner perimeter rows (e.g. row 1 and 2) should also be transparent,
    // verifying that row 0's 16-bit WORD stride does not skew row 1.
    EXPECT_TRUE(get_mask_bit(0, 1));
    EXPECT_TRUE(get_mask_bit(15, 1));
    EXPECT_TRUE(get_mask_bit(0, 2));
    EXPECT_TRUE(get_mask_bit(15, 2));

    // Center interior pixels should remain opaque (bit = 0).
    EXPECT_FALSE(get_mask_bit(7, 7));
    EXPECT_FALSE(get_mask_bit(8, 8));
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

TEST(UiUtilTest, CouldBeThemeSettingChange) {
  const struct {
    const char* description;
    WPARAM wparam;
    bool expected;
  } test_cases[] = {
      // What the shell broadcasts carry, "ImmersiveColorSet" among them.
      {"unattributed change", 0, true},
      // A genuine theme change, even though it names an SPI_* action. The
      // previous predicate rejected it: Windows sends it with lParam pointing
      // at L"HighContrast", not L"ImmersiveColorSet".
      {"high contrast", SPI_SETHIGHCONTRAST, true},

      // The behavior change this CL makes. Deleting these rows is the
      // deliberate act required to widen the filter again.
      {"non-client metrics", SPI_SETNONCLIENTMETRICS, false},
      {"work area", SPI_SETWORKAREA, false},
      {"client area animation", SPI_SETCLIENTAREAANIMATION, false},
      // Pins that the accepted set is an allowlist, not a denylist.
      {"arbitrary non-zero wparam", 0xDEADBEEF, false},
      // These would survive an IS_INTRESOURCE() screen: that macro tests the
      // value against 0x10000 as unsigned, so a negative one has its high
      // bits set and passes for a pointer. Rejecting from `wparam` does not
      // care.
      {"small negative value", static_cast<WPARAM>(-4096), false},
      {"all bits set", static_cast<WPARAM>(-1), false},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.description);
    EXPECT_EQ(test_case.expected, CouldBeThemeSettingChange(test_case.wparam));
  }
}

TEST(UiUtilTest, ApplySuggestedWindowRect) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  HWND hwnd = ::CreateWindowEx(0, L"STATIC", L"Test", WS_POPUP, 10, 10, 100,
                               100, nullptr, nullptr, nullptr, nullptr);
  ASSERT_TRUE(hwnd);
  const absl::Cleanup destroy = [&] { ::DestroyWindow(hwnd); };

  RECT before = {};
  ASSERT_TRUE(::GetWindowRect(hwnd, &before));

  // Null lParam leaves window bounds unchanged.
  ApplySuggestedWindowRect(hwnd, 0);
  RECT after_null = {};
  ASSERT_TRUE(::GetWindowRect(hwnd, &after_null));
  EXPECT_TRUE(::EqualRect(&before, &after_null));

  // Non-null lParam resizes and repositions the window to the suggested rect.
  const RECT suggested = {50, 60, 200, 250};
  ApplySuggestedWindowRect(hwnd, reinterpret_cast<LPARAM>(&suggested));
  RECT after_suggested = {};
  ASSERT_TRUE(::GetWindowRect(hwnd, &after_suggested));
  EXPECT_TRUE(::EqualRect(&suggested, &after_suggested));
}

TEST(UiUtilTest, CreateIconFromBitmap_BadgeOverlay_24bpp) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  base::win::ScopedGetDC dc(nullptr);

  // Create a 48x48 24bpp white logo bitmap.
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
  const size_t stride24 = CalculateDIBStride(48, 24);
  // SAFETY: `bmp24` is a 24bpp DIB section of 48x48 pixels, with byte size
  // exactly equal to `stride24 * 48`.
  base::span<uint8_t> span24 =
      UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(bits24), stride24 * 48));
  // Fill base with pure white (RGB 255, 255, 255).
  std::ranges::fill(span24, 255);

  // Create a 16x16 24bpp badge bitmap and icon with transparent corner keyed
  // by magenta RGB(255, 0, 255).
  BITMAPINFO bi_badge = {};
  bi_badge.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi_badge.bmiHeader.biWidth = 16;
  bi_badge.bmiHeader.biHeight = 16;
  bi_badge.bmiHeader.biPlanes = 1;
  bi_badge.bmiHeader.biBitCount = 24;
  bi_badge.bmiHeader.biCompression = BI_RGB;
  void* bits_badge = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp_badge(::CreateDIBSection(
      dc, &bi_badge, DIB_RGB_COLORS, &bits_badge, nullptr, 0));
  ASSERT_TRUE(bmp_badge.is_valid());
  const size_t stride_badge = CalculateDIBStride(16, 24);
  // SAFETY: `bmp_badge` is a 24bpp DIB section of 16x16 pixels, with byte size
  // exactly equal to `stride_badge * 16`.
  base::span<uint8_t> span_badge = UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(bits_badge), stride_badge * 16));
  for (size_t y = 0; y < 16; ++y) {
    const size_t dib_y = 15 - y;
    for (size_t x = 0; x < 16; ++x) {
      if (x == 0 && y == 0) {
        // Transparent corner pixel (keyed by magenta RGB 255, 0, 255).
        span_badge[dib_y * stride_badge + x * 3 + 0] = 255;  // Blue
        span_badge[dib_y * stride_badge + x * 3 + 1] = 0;    // Green
        span_badge[dib_y * stride_badge + x * 3 + 2] = 255;  // Red
      } else {
        span_badge[dib_y * stride_badge + x * 3 + 0] = 0;    // Blue
        span_badge[dib_y * stride_badge + x * 3 + 1] = 0;    // Green
        span_badge[dib_y * stride_badge + x * 3 + 2] = 255;  // Red
      }
    }
  }
  base::win::ScopedGDIObject<HICON> badge_icon = CreateIconFromHBitmap(
      bmp_badge.get(), 16, 16, /*dpi=*/0, RGB(255, 0, 255));
  ASSERT_TRUE(badge_icon.is_valid());

  // Create badged icon (24x24 target, base logo scaled to 18x18 in lower-left,
  // badge overlay 12x12 in top-right).
  base::win::ScopedGDIObject<HICON> badged_icon = CreateIconFromHBitmap(
      bmp24.get(), 24, 24, /*dpi=*/0, std::nullopt, badge_icon.get());
  ASSERT_TRUE(badged_icon.is_valid());

  ICONINFO info = {};
  ASSERT_TRUE(::GetIconInfo(badged_icon.get(), &info));
  base::win::ScopedGDIObject<HBITMAP> color_bmp(info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);
  ASSERT_TRUE(color_bmp.is_valid());
  ASSERT_TRUE(mask_bmp.is_valid());

  // Read back 24x24 color DIB to inspect composite pixels.
  BITMAPINFO bi_read = {};
  bi_read.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi_read.bmiHeader.biWidth = 24;
  bi_read.bmiHeader.biHeight = 24;
  bi_read.bmiHeader.biPlanes = 1;
  bi_read.bmiHeader.biBitCount = 24;
  bi_read.bmiHeader.biCompression = BI_RGB;
  const size_t stride_read = CalculateDIBStride(24, 24);
  std::vector<uint8_t> read_pixels(stride_read * 24, 0);
  ASSERT_EQ(::GetDIBits(dc, color_bmp.get(), 0, 24, read_pixels.data(),
                        &bi_read, DIB_RGB_COLORS),
            24);

  auto get_read_color = [&](int x, int y) -> COLORREF {
    const size_t dib_y = 23 - y;
    const size_t offset = dib_y * stride_read + static_cast<size_t>(x) * 3;
    return RGB(read_pixels[offset + 2], read_pixels[offset + 1],
               read_pixels[offset + 0]);
  };

  struct {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[2];
  } mask_bi = {};
  mask_bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  mask_bi.bmiHeader.biWidth = 24;
  mask_bi.bmiHeader.biHeight = 24;
  mask_bi.bmiHeader.biPlanes = 1;
  mask_bi.bmiHeader.biBitCount = 1;
  mask_bi.bmiHeader.biCompression = BI_RGB;
  const size_t mask_stride = CalculateDIBStride(24, 1);
  std::vector<uint8_t> mask_pixels(mask_stride * 24, 0);
  ASSERT_EQ(
      ::GetDIBits(dc, mask_bmp.get(), 0, 24, mask_pixels.data(),
                  reinterpret_cast<BITMAPINFO*>(&mask_bi), DIB_RGB_COLORS),
      24);

  auto get_mask_bit = [&](int x, int y) -> bool {
    const size_t dib_y = 23 - y;
    const uint8_t byte = mask_pixels[dib_y * mask_stride + (x / 8)];
    return (byte & (1 << (7 - (x % 8)))) != 0;
  };

  // Badge is in top-right: target_w = 24, badge_w = 12, badge_x = 12, badge_y =
  // 0. Visual coordinates (12..23, 0..11) contain badge pixels.
  // The badge's transparent corner at (12, 0) outside the base logo must have
  // mask bit 1 (transparent), preventing black boxes on light backgrounds.
  EXPECT_TRUE(get_mask_bit(12, 0));
  EXPECT_EQ(get_read_color(12, 0), RGB(0, 0, 0));

  // Opaque badge pixel at (18, 4) must have mask bit 0 (opaque) and red color.
  const COLORREF badge_sample = get_read_color(18, 4);
  EXPECT_GT(GetRValue(badge_sample), 200);
  EXPECT_LT(GetGValue(badge_sample), 50);
  EXPECT_LT(GetBValue(badge_sample), 50);
  EXPECT_FALSE(get_mask_bit(18, 4));

  // Bottom-left: base logo is at [0..18, 6..24].
  // Visual coordinates (4, 20) should still be base color (white) and opaque.
  const COLORREF bottom_left = get_read_color(4, 20);
  EXPECT_GT(GetGValue(bottom_left), 200);
  EXPECT_FALSE(get_mask_bit(4, 20));

  // Top-left (outside both base logo and badge) is keyed out as transparent.
  const COLORREF top_left = get_read_color(2, 2);
  EXPECT_EQ(top_left, RGB(0, 0, 0));
  EXPECT_TRUE(get_mask_bit(2, 2));
}

TEST(UiUtilTest, CreateIconFromBitmap_BadgeOverlay_CustomBackgroundColorKey) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  base::win::ScopedGetDC dc(nullptr);

  // Create a 48x48 24bpp logo bitmap with a custom non-dialog background
  // RGB(64, 128, 192) that is neither pure white nor dark dialog background.
  constexpr COLORREF kCustomBg = RGB(64, 128, 192);
  constexpr COLORREF kLogoFg = RGB(255, 255, 0);  // Yellow logo interior
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
  const size_t stride24 = CalculateDIBStride(48, 24);
  // SAFETY: `bmp24` is a 24bpp DIB section of 48x48 pixels, with byte size
  // exactly equal to `stride24 * 48`.
  base::span<uint8_t> span24 =
      UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(bits24), stride24 * 48));

  // Initialize with custom background RGB(64, 128, 192).
  for (size_t y = 0; y < 48; ++y) {
    for (size_t x = 0; x < 48; ++x) {
      const size_t offset = y * stride24 + x * 3;
      span24[offset + 0] = GetBValue(kCustomBg);
      span24[offset + 1] = GetGValue(kCustomBg);
      span24[offset + 2] = GetRValue(kCustomBg);
    }
  }

  // Draw a solid foreground rectangle in the center interior [16..32, 16..32].
  for (size_t y = 16; y < 32; ++y) {
    for (size_t x = 16; x < 32; ++x) {
      const size_t offset = y * stride24 + x * 3;
      span24[offset + 0] = GetBValue(kLogoFg);
      span24[offset + 1] = GetGValue(kLogoFg);
      span24[offset + 2] = GetRValue(kLogoFg);
    }
  }

  // Create a 16x16 24bpp badge bitmap and icon with transparent corner keyed
  // by magenta RGB(255, 0, 255).
  BITMAPINFO bi_badge = {};
  bi_badge.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi_badge.bmiHeader.biWidth = 16;
  bi_badge.bmiHeader.biHeight = 16;
  bi_badge.bmiHeader.biPlanes = 1;
  bi_badge.bmiHeader.biBitCount = 24;
  bi_badge.bmiHeader.biCompression = BI_RGB;
  void* bits_badge = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp_badge(::CreateDIBSection(
      dc, &bi_badge, DIB_RGB_COLORS, &bits_badge, nullptr, 0));
  ASSERT_TRUE(bmp_badge.is_valid());
  const size_t stride_badge = CalculateDIBStride(16, 24);
  // SAFETY: `bmp_badge` is a 24bpp DIB section of 16x16 pixels, with byte size
  // exactly equal to `stride_badge * 16`.
  base::span<uint8_t> span_badge = UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(bits_badge), stride_badge * 16));
  for (size_t y = 0; y < 16; ++y) {
    const size_t dib_y = 15 - y;
    for (size_t x = 0; x < 16; ++x) {
      if (x == 0 && y == 0) {
        // Transparent corner pixel (keyed by magenta RGB 255, 0, 255).
        span_badge[dib_y * stride_badge + x * 3 + 0] = 255;  // Blue
        span_badge[dib_y * stride_badge + x * 3 + 1] = 0;    // Green
        span_badge[dib_y * stride_badge + x * 3 + 2] = 255;  // Red
      } else {
        span_badge[dib_y * stride_badge + x * 3 + 0] = 0;    // Blue
        span_badge[dib_y * stride_badge + x * 3 + 1] = 0;    // Green
        span_badge[dib_y * stride_badge + x * 3 + 2] = 255;  // Red
      }
    }
  }
  base::win::ScopedGDIObject<HICON> badge_icon = CreateIconFromHBitmap(
      bmp_badge.get(), 16, 16, /*dpi=*/0, RGB(255, 0, 255));
  ASSERT_TRUE(badge_icon.is_valid());

  // Create badged icon (24x24 target, base logo scaled to 18x18 in lower-left,
  // badge overlay 12x12 in top-right). transparent_color is nullopt so key
  // color auto-detection runs.
  base::win::ScopedGDIObject<HICON> badged_icon = CreateIconFromHBitmap(
      bmp24.get(), 24, 24, /*dpi=*/0, std::nullopt, badge_icon.get());
  ASSERT_TRUE(badged_icon.is_valid());

  ICONINFO info = {};
  ASSERT_TRUE(::GetIconInfo(badged_icon.get(), &info));
  base::win::ScopedGDIObject<HBITMAP> color_bmp(info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);
  ASSERT_TRUE(color_bmp.is_valid());
  ASSERT_TRUE(mask_bmp.is_valid());

  BITMAPINFO bi_read = {};
  bi_read.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi_read.bmiHeader.biWidth = 24;
  bi_read.bmiHeader.biHeight = 24;
  bi_read.bmiHeader.biPlanes = 1;
  bi_read.bmiHeader.biBitCount = 24;
  bi_read.bmiHeader.biCompression = BI_RGB;
  const size_t stride_read = CalculateDIBStride(24, 24);
  std::vector<uint8_t> read_pixels(stride_read * 24, 0);
  ASSERT_EQ(::GetDIBits(dc, color_bmp.get(), 0, 24, read_pixels.data(),
                        &bi_read, DIB_RGB_COLORS),
            24);

  auto get_read_color = [&](int x, int y) -> COLORREF {
    const size_t dib_y = 23 - y;
    const size_t offset = dib_y * stride_read + static_cast<size_t>(x) * 3;
    return RGB(read_pixels[offset + 2], read_pixels[offset + 1],
               read_pixels[offset + 0]);
  };

  struct {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[2];
  } mask_bi = {};
  mask_bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  mask_bi.bmiHeader.biWidth = 24;
  mask_bi.bmiHeader.biHeight = 24;
  mask_bi.bmiHeader.biPlanes = 1;
  mask_bi.bmiHeader.biBitCount = 1;
  mask_bi.bmiHeader.biCompression = BI_RGB;
  const size_t mask_stride = CalculateDIBStride(24, 1);
  std::vector<uint8_t> mask_pixels(mask_stride * 24, 0);
  ASSERT_EQ(
      ::GetDIBits(dc, mask_bmp.get(), 0, 24, mask_pixels.data(),
                  reinterpret_cast<BITMAPINFO*>(&mask_bi), DIB_RGB_COLORS),
      24);

  auto get_mask_bit = [&](int x, int y) -> bool {
    const size_t dib_y = 23 - y;
    const uint8_t byte = mask_pixels[dib_y * mask_stride + (x / 8)];
    return (byte & (1 << (7 - (x % 8)))) != 0;
  };

  // 1. The custom background around the base logo (e.g. at (1, 23)) must be
  //    keyed out (mask bit 1, transparent; color zeroed to RGB(0, 0, 0)).
  //    This proves corners_match succeeded on the custom background color
  //    because c10 was sampled from the unbadged base logo in src_dc.
  EXPECT_TRUE(get_mask_bit(1, 23));
  EXPECT_EQ(get_read_color(1, 23), RGB(0, 0, 0));

  // 2. Base logo foreground content (yellow) in interior must remain opaque.
  EXPECT_FALSE(get_mask_bit(9, 15));
  const COLORREF fg_sample = get_read_color(9, 15);
  EXPECT_GT(GetRValue(fg_sample), 200);
  EXPECT_GT(GetGValue(fg_sample), 200);
  EXPECT_LT(GetBValue(fg_sample), 50);

  // 3. Opaque badge pixel at (18, 4) must be opaque and red.
  EXPECT_FALSE(get_mask_bit(18, 4));
  const COLORREF badge_sample = get_read_color(18, 4);
  EXPECT_GT(GetRValue(badge_sample), 200);
  EXPECT_LT(GetGValue(badge_sample), 50);
  EXPECT_LT(GetBValue(badge_sample), 50);

  // 4. Transparent badge corner at (12, 0) must be transparent (mask bit 1).
  EXPECT_TRUE(get_mask_bit(12, 0));
  EXPECT_EQ(get_read_color(12, 0), RGB(0, 0, 0));
}

TEST(UiUtilTest, CreateIconFromBitmap_BadgeOverlay_32bppAlpha) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  base::win::ScopedGetDC dc(nullptr);

  // Create a 24x24 32bpp DIB with per-pixel alpha (semi-transparent blue).
  BITMAPINFO bi32 = {};
  bi32.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi32.bmiHeader.biWidth = 24;
  bi32.bmiHeader.biHeight = 24;
  bi32.bmiHeader.biPlanes = 1;
  bi32.bmiHeader.biBitCount = 32;
  bi32.bmiHeader.biCompression = BI_RGB;
  void* bits32 = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp32(
      ::CreateDIBSection(dc, &bi32, DIB_RGB_COLORS, &bits32, nullptr, 0));
  ASSERT_TRUE(bmp32.is_valid());
  // SAFETY: `bmp32` is a 32bpp DIB section of 24x24 pixels, containing exactly
  // 576 32-bit DWORDs.
  base::span<uint32_t> pixels32 =
      UNSAFE_BUFFERS(base::span(static_cast<uint32_t*>(bits32), 24u * 24u));
  // Alpha = 128, Blue = 255. Premultiplied: (128 << 24) | 128.
  std::ranges::fill(pixels32, (128u << 24) | 128u);

  // Create a 16x16 32bpp ARGB badge icon with transparent corner (alpha = 0).
  BITMAPINFO bi_badge = {};
  bi_badge.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi_badge.bmiHeader.biWidth = 16;
  bi_badge.bmiHeader.biHeight = 16;
  bi_badge.bmiHeader.biPlanes = 1;
  bi_badge.bmiHeader.biBitCount = 32;
  bi_badge.bmiHeader.biCompression = BI_RGB;
  void* bits_badge = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp_badge(::CreateDIBSection(
      dc, &bi_badge, DIB_RGB_COLORS, &bits_badge, nullptr, 0));
  ASSERT_TRUE(bmp_badge.is_valid());
  // SAFETY: `bmp_badge` is a 32bpp DIB section of 16x16 pixels, containing
  // exactly 256 32-bit DWORDs.
  base::span<uint32_t> pixels_badge =
      UNSAFE_BUFFERS(base::span(static_cast<uint32_t*>(bits_badge), 16u * 16u));
  for (size_t y = 0; y < 16; ++y) {
    const size_t dib_y = 15 - y;
    for (size_t x = 0; x < 16; ++x) {
      if (x == 0 && y == 0) {
        // Transparent corner (alpha = 0).
        pixels_badge[dib_y * 16 + x] = 0x00000000;
      } else {
        // Opaque red (alpha = 255).
        pixels_badge[dib_y * 16 + x] = 0xFFFF0000;
      }
    }
  }
  // Construct the badge HICON directly via CreateIconIndirect with an explicit
  // all-zero 1bpp AND mask (hbmMask) and 32bpp ARGB color bitmap (hbmColor).
  // Standard Windows 32bpp icons leave hbmMask all zeros (0x00, opaque); this
  // verifies that GetIconAlphaChannel correctly extracts opacity from the
  // color bitmap's alpha channel rather than falling back to hbmMask.
  const size_t badge_mask_stride = CalculateDDBStride(16);
  std::vector<uint8_t> zero_mask(badge_mask_stride * 16, 0);
  base::win::ScopedGDIObject<HBITMAP> mask_badge(
      ::CreateBitmap(16, 16, 1, 1, zero_mask.data()));
  ASSERT_TRUE(mask_badge.is_valid());

  ICONINFO badge_info = {};
  badge_info.fIcon = TRUE;
  badge_info.hbmColor = bmp_badge.get();
  badge_info.hbmMask = mask_badge.get();
  base::win::ScopedGDIObject<HICON> badge_icon(
      ::CreateIconIndirect(&badge_info));
  ASSERT_TRUE(badge_icon.is_valid());

  base::win::ScopedGDIObject<HICON> badged_icon =
      CreateIconFromHBitmap(bmp32.get(), 24, 24, /*dpi=*/0,
                            /*transparent_color=*/std::nullopt,
                            badge_icon.get());
  ASSERT_TRUE(badged_icon.is_valid());

  ICONINFO info = {};
  ASSERT_TRUE(::GetIconInfo(badged_icon.get(), &info));
  base::win::ScopedGDIObject<HBITMAP> color_bmp(info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);
  ASSERT_TRUE(color_bmp.is_valid());
  ASSERT_TRUE(mask_bmp.is_valid());

  std::vector<uint32_t> read_pixels(24u * 24u, 0);
  ASSERT_EQ(::GetDIBits(dc, color_bmp.get(), 0, 24, read_pixels.data(), &bi32,
                        DIB_RGB_COLORS),
            24);

  // In bottom-up DIB rows:
  // Visual top-right badge corner (x=12, visual_y=0 -> dib_y = 23): transparent
  // badge corner outside base logo retains transparent alpha (0x00).
  const uint32_t badge_transparent_corner = read_pixels[23 * 24 + 12];
  EXPECT_EQ(badge_transparent_corner >> 24, 0u);

  // Visual top-right (x=18, visual_y=4 -> dib_y = 19): red badge with
  // opaque alpha (0xFF).
  const uint32_t badge_pixel = read_pixels[19 * 24 + 18];
  EXPECT_EQ(badge_pixel >> 24, 255u);
  EXPECT_GT((badge_pixel >> 16) & 0xFF, 200u);

  // Visual bottom-left (x=4, visual_y=20 -> dib_y = 3): original
  // semi-transparent blue.
  const uint32_t base_pixel = read_pixels[3 * 24 + 4];
  EXPECT_GE(base_pixel >> 24, 126u);
  EXPECT_LE(base_pixel >> 24, 130u);

  // Visual lower-right of base logo (x=16, visual_y=20 -> dib_y = 3): directly
  // below the top-right badge column (x=12..23). Verifies that the badge
  // compositing loop does not invert DIB row coordinates and overwrite the
  // lower-right base logo region; retains original base logo alpha (~128) and
  // blue color without red badge contamination.
  const uint32_t lower_right_base_pixel = read_pixels[3 * 24 + 16];
  EXPECT_GE(lower_right_base_pixel >> 24, 126u);
  EXPECT_LE(lower_right_base_pixel >> 24, 130u);
  EXPECT_GT(lower_right_base_pixel & 0xFF, 100u);
  EXPECT_EQ((lower_right_base_pixel >> 16) & 0xFF, 0u);

  // Visual lower-right margin outside base logo (x=22, visual_y=22 -> dib_y =
  // 1): outside both base logo and badge, remains fully transparent (0x00).
  const uint32_t lower_right_margin_pixel = read_pixels[1 * 24 + 22];
  EXPECT_EQ(lower_right_margin_pixel >> 24, 0u);

  // Visual top-left (x=2, visual_y=2 -> dib_y = 21): margin outside both base
  // logo and badge remains fully transparent.
  const uint32_t top_left_pixel = read_pixels[21 * 24 + 2];
  EXPECT_EQ(top_left_pixel >> 24, 0u);
}

TEST(UiUtilTest, CreateIconFromBitmap_BadgeOverlay_32bppAntiAliasedEdgeAlpha) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  base::win::ScopedGetDC dc(nullptr);

  // Create a 24x24 32bpp DIB base logo with semi-transparent blue (alpha =
  // 128). When scaled into the lower-left ~75% quadrant, it covers
  // [0..17]x[6..23].
  BITMAPINFO bi32 = {};
  bi32.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi32.bmiHeader.biWidth = 24;
  bi32.bmiHeader.biHeight = 24;
  bi32.bmiHeader.biPlanes = 1;
  bi32.bmiHeader.biBitCount = 32;
  bi32.bmiHeader.biCompression = BI_RGB;
  void* bits32 = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp32(
      ::CreateDIBSection(dc, &bi32, DIB_RGB_COLORS, &bits32, nullptr, 0));
  ASSERT_TRUE(bmp32.is_valid());
  // SAFETY: `bmp32` is a 32bpp DIB section of 24x24 pixels, containing exactly
  // 576 32-bit DWORDs.
  base::span<uint32_t> pixels32 =
      UNSAFE_BUFFERS(base::span(static_cast<uint32_t*>(bits32), 24u * 24u));
  std::ranges::fill(pixels32, (128u << 24) | 128u);

  // Create a 12x12 32bpp ARGB badge icon (matching target badge dimensions for
  // a 24x24 canvas) containing:
  // - Fully transparent corner at (0, 0): alpha = 0.
  // - Semi-transparent anti-aliased edge at (0, 1): alpha = 128, straight red.
  // - Semi-transparent anti-aliased edge at (0, 8): alpha = 128, straight red.
  // - Opaque interior at (4, 4): alpha = 255 (opaque red).
  BITMAPINFO bi_badge = {};
  bi_badge.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi_badge.bmiHeader.biWidth = 12;
  bi_badge.bmiHeader.biHeight = 12;
  bi_badge.bmiHeader.biPlanes = 1;
  bi_badge.bmiHeader.biBitCount = 32;
  bi_badge.bmiHeader.biCompression = BI_RGB;
  void* bits_badge = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp_badge(::CreateDIBSection(
      dc, &bi_badge, DIB_RGB_COLORS, &bits_badge, nullptr, 0));
  ASSERT_TRUE(bmp_badge.is_valid());
  // SAFETY: `bmp_badge` is a 32bpp DIB section of 12x12 pixels, containing
  // exactly 144 32-bit DWORDs.
  base::span<uint32_t> pixels_badge =
      UNSAFE_BUFFERS(base::span(static_cast<uint32_t*>(bits_badge), 12u * 12u));
  for (size_t y = 0; y < 12; ++y) {
    const size_t dib_y = 11 - y;
    for (size_t x = 0; x < 12; ++x) {
      if (x == 0 && y == 0) {
        pixels_badge[dib_y * 12 + x] = 0x00000000;
      } else if (x == 0 && (y == 1 || y == 8)) {
        // Semi-transparent anti-aliased edge: alpha = 128, straight red = 255.
        // Standard Windows icons store straight colors; DrawIconEx multiplies
        // RGB by alpha during rasterization.
        pixels_badge[dib_y * 12 + x] = (128u << 24) | (255u << 16);
      } else {
        pixels_badge[dib_y * 12 + x] = 0xFFFF0000;
      }
    }
  }
  // Construct the badge HICON directly via CreateIconIndirect with an explicit
  // all-zero 1bpp AND mask (hbmMask) and 32bpp ARGB color bitmap (hbmColor).
  // Standard Windows 32bpp icons leave hbmMask all zeros (0x00, opaque); this
  // verifies that GetIconAlphaChannel correctly extracts opacity from the
  // color bitmap's alpha channel rather than falling back to hbmMask.
  const size_t badge_mask_stride = CalculateDDBStride(12);
  std::vector<uint8_t> zero_mask(badge_mask_stride * 12, 0);
  base::win::ScopedGDIObject<HBITMAP> mask_badge(
      ::CreateBitmap(12, 12, 1, 1, zero_mask.data()));
  ASSERT_TRUE(mask_badge.is_valid());

  ICONINFO badge_info = {};
  badge_info.fIcon = TRUE;
  badge_info.hbmColor = bmp_badge.get();
  badge_info.hbmMask = mask_badge.get();
  base::win::ScopedGDIObject<HICON> badge_icon(
      ::CreateIconIndirect(&badge_info));
  ASSERT_TRUE(badge_icon.is_valid());

  base::win::ScopedGDIObject<HICON> badged_icon =
      CreateIconFromHBitmap(bmp32.get(), 24, 24, /*dpi=*/0,
                            /*transparent_color=*/std::nullopt,
                            badge_icon.get());
  ASSERT_TRUE(badged_icon.is_valid());

  ICONINFO info = {};
  ASSERT_TRUE(::GetIconInfo(badged_icon.get(), &info));
  base::win::ScopedGDIObject<HBITMAP> color_bmp(info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);
  ASSERT_TRUE(color_bmp.is_valid());
  ASSERT_TRUE(mask_bmp.is_valid());

  std::vector<uint32_t> read_pixels(24u * 24u, 0);
  ASSERT_EQ(::GetDIBits(dc, color_bmp.get(), 0, 24, read_pixels.data(), &bi32,
                        DIB_RGB_COLORS),
            24);

  // Badge is placed at badge_x = 12, badge_y = 0.
  // 1. Transparent badge corner at (x=12, visual_y=0 -> dib_y = 23):
  // Outside base logo (visual_y < 6) and badge_a == 0 -> out_a == 0.
  // Premultiplication ensures RGB is completely zeroed when alpha is 0.
  EXPECT_EQ(read_pixels[23 * 24 + 12] >> 24, 0u);
  EXPECT_EQ(read_pixels[23 * 24 + 12] & 0x00FFFFFF, 0u);

  // 2. Anti-aliased badge edge pixel at (x=12, visual_y=1 -> dib_y = 22):
  // Outside base logo (visual_y = 1 < 6, so base_a == 0), badge_a == 128.
  // Porter-Duff compositing: out_a = badge_a + base_a * (255 - badge_a) / 255 =
  // 128. Verifies the anti-aliased edge retains fractional alpha and that RGB
  // is properly premultiplied (R = 255 * 128 / 255 = 128).
  EXPECT_EQ(read_pixels[22 * 24 + 12] >> 24, 128u);
  EXPECT_EQ((read_pixels[22 * 24 + 12] >> 16) & 0xFF, 128u);

  // 3. Anti-aliased badge edge pixel at (x=12, visual_y=8 -> dib_y = 15):
  // Over base logo (visual_y = 8 >= 6, base_a ~ 128), badge_a == 128.
  // Porter-Duff compositing: 128 + (128 * 127 + 127) / 255 = 192.
  const uint32_t blended_alpha = read_pixels[15 * 24 + 12] >> 24;
  EXPECT_GE(blended_alpha, 190u);
  EXPECT_LE(blended_alpha, 194u);
  EXPECT_GT((read_pixels[15 * 24 + 12] >> 16) & 0xFF, 100u);
  EXPECT_LE((read_pixels[15 * 24 + 12] >> 16) & 0xFF, blended_alpha);
  EXPECT_GT(read_pixels[15 * 24 + 12] & 0xFF, 40u);
  EXPECT_LE(read_pixels[15 * 24 + 12] & 0xFF, blended_alpha);

  // 4. Fully opaque badge pixel at (x=16, visual_y=4 -> dib_y = 19):
  // badge_a == 255 -> out_a == 255.
  EXPECT_EQ(read_pixels[19 * 24 + 16] >> 24, 255u);
  EXPECT_GT((read_pixels[19 * 24 + 16] >> 16) & 0xFF, 200u);

  // 5. Lower-right base logo pixel below badge column (x=16, visual_y=20 ->
  // dib_y = 3): verifies unaffected lower-right region retains base logo alpha
  // (~128).
  const uint32_t lower_right_pixel = read_pixels[3 * 24 + 16];
  EXPECT_GE(lower_right_pixel >> 24, 126u);
  EXPECT_LE(lower_right_pixel >> 24, 130u);
}

TEST(UiUtilTest, CreateIconFromBitmap_BadgeOverlay_32bppArgbBadgeOn24bppLogo) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  base::win::ScopedGetDC dc(nullptr);

  // Create a 48x48 24bpp white logo bitmap (simulating downloaded application
  // logo bitmap).
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
  const size_t stride24 = CalculateDIBStride(48, 24);
  // SAFETY: `bmp24` is a 24bpp DIB section of 48x48 pixels, with byte size
  // exactly equal to `stride24 * 48`.
  base::span<uint8_t> span24 =
      UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(bits24), stride24 * 48));
  std::ranges::fill(span24, 255);

  // Create a 16x16 32bpp ARGB badge icon with transparent corner padding
  // (alpha = 0) and opaque interior (alpha = 255). Standard Windows 32bpp
  // icons have their 1bpp AND mask set to all zeros (opaque).
  BITMAPINFO bi_badge = {};
  bi_badge.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi_badge.bmiHeader.biWidth = 16;
  bi_badge.bmiHeader.biHeight = 16;
  bi_badge.bmiHeader.biPlanes = 1;
  bi_badge.bmiHeader.biBitCount = 32;
  bi_badge.bmiHeader.biCompression = BI_RGB;
  void* bits_badge = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp_badge(::CreateDIBSection(
      dc, &bi_badge, DIB_RGB_COLORS, &bits_badge, nullptr, 0));
  ASSERT_TRUE(bmp_badge.is_valid());
  // SAFETY: `bmp_badge` is a 32bpp DIB section of 16x16 pixels, containing
  // exactly 256 32-bit DWORDs.
  base::span<uint32_t> pixels_badge =
      UNSAFE_BUFFERS(base::span(static_cast<uint32_t*>(bits_badge), 16u * 16u));
  for (size_t y = 0; y < 16; ++y) {
    const size_t dib_y = 15 - y;
    for (size_t x = 0; x < 16; ++x) {
      if (x == 0 && y == 0) {
        // Transparent corner (alpha = 0).
        pixels_badge[dib_y * 16 + x] = 0x00000000;
      } else if (x == 1 && y == 0) {
        // Anti-aliased perimeter pixel below threshold (alpha = 64 < 128).
        pixels_badge[dib_y * 16 + x] = (64u << 24) | 0x00FF0000;
      } else {
        // Opaque red (alpha = 255).
        pixels_badge[dib_y * 16 + x] = 0xFFFF0000;
      }
    }
  }
  // Construct the badge HICON directly via CreateIconIndirect with an explicit
  // all-zero 1bpp AND mask (hbmMask) and 32bpp ARGB color bitmap (hbmColor).
  // Standard Windows 32bpp icons leave hbmMask all zeros (0x00, opaque); this
  // verifies that GetIconAlphaChannel correctly extracts opacity from the
  // color bitmap's alpha channel rather than falling back to hbmMask.
  const size_t badge_mask_stride = CalculateDDBStride(16);
  std::vector<uint8_t> zero_mask(badge_mask_stride * 16, 0);
  base::win::ScopedGDIObject<HBITMAP> mask_badge(
      ::CreateBitmap(16, 16, 1, 1, zero_mask.data()));
  ASSERT_TRUE(mask_badge.is_valid());

  ICONINFO badge_info = {};
  badge_info.fIcon = TRUE;
  badge_info.hbmColor = bmp_badge.get();
  badge_info.hbmMask = mask_badge.get();
  base::win::ScopedGDIObject<HICON> badge_icon(
      ::CreateIconIndirect(&badge_info));
  ASSERT_TRUE(badge_icon.is_valid());

  // Create badged icon (24x24 target, base logo scaled to 18x18 in lower-left,
  // badge overlay 12x12 in top-right).
  base::win::ScopedGDIObject<HICON> badged_icon = CreateIconFromHBitmap(
      bmp24.get(), 24, 24, /*dpi=*/0, std::nullopt, badge_icon.get());
  ASSERT_TRUE(badged_icon.is_valid());

  ICONINFO info = {};
  ASSERT_TRUE(::GetIconInfo(badged_icon.get(), &info));
  base::win::ScopedGDIObject<HBITMAP> color_bmp(info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);
  ASSERT_TRUE(color_bmp.is_valid());
  ASSERT_TRUE(mask_bmp.is_valid());

  BITMAPINFO bi_read = {};
  bi_read.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi_read.bmiHeader.biWidth = 24;
  bi_read.bmiHeader.biHeight = 24;
  bi_read.bmiHeader.biPlanes = 1;
  bi_read.bmiHeader.biBitCount = 24;
  bi_read.bmiHeader.biCompression = BI_RGB;
  const size_t stride_read = CalculateDIBStride(24, 24);
  std::vector<uint8_t> read_pixels(stride_read * 24, 0);
  ASSERT_EQ(::GetDIBits(dc, color_bmp.get(), 0, 24, read_pixels.data(),
                        &bi_read, DIB_RGB_COLORS),
            24);

  auto get_read_color = [&](int x, int y) -> COLORREF {
    const size_t dib_y = 23 - y;
    const size_t offset = dib_y * stride_read + static_cast<size_t>(x) * 3;
    return RGB(read_pixels[offset + 2], read_pixels[offset + 1],
               read_pixels[offset + 0]);
  };

  struct {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[2];
  } mask_bi = {};
  mask_bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  mask_bi.bmiHeader.biWidth = 24;
  mask_bi.bmiHeader.biHeight = 24;
  mask_bi.bmiHeader.biPlanes = 1;
  mask_bi.bmiHeader.biBitCount = 1;
  mask_bi.bmiHeader.biCompression = BI_RGB;
  const size_t mask_stride = CalculateDIBStride(24, 1);
  std::vector<uint8_t> mask_pixels(mask_stride * 24, 0);
  ASSERT_EQ(
      ::GetDIBits(dc, mask_bmp.get(), 0, 24, mask_pixels.data(),
                  reinterpret_cast<BITMAPINFO*>(&mask_bi), DIB_RGB_COLORS),
      24);

  auto get_mask_bit = [&](int x, int y) -> bool {
    const size_t dib_y = 23 - y;
    const uint8_t byte = mask_pixels[dib_y * mask_stride + (x / 8)];
    return (byte & (1 << (7 - (x % 8)))) != 0;
  };

  // The 32bpp ARGB badge's transparent corner at (12, 0) outside the base
  // logo must be detected as transparent via its alpha channel, receiving
  // mask bit 1 (transparent) and color RGB(0,0,0), preventing black boxes
  // on light taskbars.
  EXPECT_TRUE(get_mask_bit(12, 0));
  EXPECT_EQ(get_read_color(12, 0), RGB(0, 0, 0));

  // Anti-aliased perimeter pixel at (13, 0) with alpha = 64 is below
  // kBadgeOpaqueThreshold (128) and outside the base logo, so it must be
  // treated as transparent (mask bit 1, RGB(0,0,0)), eliminating dark
  // perimeter fringing on light taskbars.
  EXPECT_TRUE(get_mask_bit(13, 0));
  EXPECT_EQ(get_read_color(13, 0), RGB(0, 0, 0));

  // Opaque badge pixel at (18, 4) must have mask bit 0 (opaque) and red color.
  const COLORREF badge_sample = get_read_color(18, 4);
  EXPECT_GT(GetRValue(badge_sample), 200);
  EXPECT_LT(GetGValue(badge_sample), 50);
  EXPECT_LT(GetBValue(badge_sample), 50);
  EXPECT_FALSE(get_mask_bit(18, 4));

  // Base logo pixel at (4, 20) must have mask bit 0 (opaque).
  EXPECT_FALSE(get_mask_bit(4, 20));

  // Margin at (2, 2) must have mask bit 1 (transparent).
  EXPECT_TRUE(get_mask_bit(2, 2));
}

TEST(UiUtilTest, CreateIconFromBitmap_BadgeOverlay_24bppBadgeOn32bppLogo) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  base::win::ScopedGetDC dc(nullptr);

  // Create a 24x24 32bpp DIB base logo with semi-transparent blue (alpha =
  // 128).
  BITMAPINFO bi32 = {};
  bi32.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi32.bmiHeader.biWidth = 24;
  bi32.bmiHeader.biHeight = 24;
  bi32.bmiHeader.biPlanes = 1;
  bi32.bmiHeader.biBitCount = 32;
  bi32.bmiHeader.biCompression = BI_RGB;
  void* bits32 = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp32(
      ::CreateDIBSection(dc, &bi32, DIB_RGB_COLORS, &bits32, nullptr, 0));
  ASSERT_TRUE(bmp32.is_valid());
  // SAFETY: `bmp32` is a 32bpp DIB section of 24x24 pixels, containing exactly
  // 576 32-bit DWORDs.
  base::span<uint32_t> pixels32 =
      UNSAFE_BUFFERS(base::span(static_cast<uint32_t*>(bits32), 24u * 24u));
  // Alpha = 128, Blue = 128 (premultiplied).
  std::ranges::fill(pixels32, (128u << 24) | 128u);

  // Create a 16x16 24bpp badge bitmap and icon with transparent corner keyed
  // by magenta RGB(255, 0, 255). This yields a non-32bpp badge icon with an
  // explicit 1bpp monochrome mask, exercising the 1bpp AND mask DI_MASK
  // fallback inside GetIconAlphaChannel.
  BITMAPINFO bi_badge = {};
  bi_badge.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi_badge.bmiHeader.biWidth = 16;
  bi_badge.bmiHeader.biHeight = 16;
  bi_badge.bmiHeader.biPlanes = 1;
  bi_badge.bmiHeader.biBitCount = 24;
  bi_badge.bmiHeader.biCompression = BI_RGB;
  void* bits_badge = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp_badge(::CreateDIBSection(
      dc, &bi_badge, DIB_RGB_COLORS, &bits_badge, nullptr, 0));
  ASSERT_TRUE(bmp_badge.is_valid());
  const size_t stride_badge = CalculateDIBStride(16, 24);
  // SAFETY: `bmp_badge` is a 24bpp DIB section of 16x16 pixels, with byte size
  // exactly equal to `stride_badge * 16`.
  base::span<uint8_t> span_badge = UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(bits_badge), stride_badge * 16));
  for (size_t y = 0; y < 16; ++y) {
    const size_t dib_y = 15 - y;
    for (size_t x = 0; x < 16; ++x) {
      if (x == 0 && y == 0) {
        // Transparent corner (magenta RGB 255, 0, 255).
        span_badge[dib_y * stride_badge + x * 3 + 0] = 255;  // Blue
        span_badge[dib_y * stride_badge + x * 3 + 1] = 0;    // Green
        span_badge[dib_y * stride_badge + x * 3 + 2] = 255;  // Red
      } else {
        // Opaque red.
        span_badge[dib_y * stride_badge + x * 3 + 0] = 0;    // Blue
        span_badge[dib_y * stride_badge + x * 3 + 1] = 0;    // Green
        span_badge[dib_y * stride_badge + x * 3 + 2] = 255;  // Red
      }
    }
  }
  base::win::ScopedGDIObject<HICON> badge_icon = CreateIconFromHBitmap(
      bmp_badge.get(), 16, 16, /*dpi=*/0, RGB(255, 0, 255));
  ASSERT_TRUE(badge_icon.is_valid());

  base::win::ScopedGDIObject<HICON> badged_icon = CreateIconFromHBitmap(
      bmp32.get(), 24, 24, /*dpi=*/0, std::nullopt, badge_icon.get());
  ASSERT_TRUE(badged_icon.is_valid());

  ICONINFO info = {};
  ASSERT_TRUE(::GetIconInfo(badged_icon.get(), &info));
  base::win::ScopedGDIObject<HBITMAP> color_bmp(info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);
  ASSERT_TRUE(color_bmp.is_valid());
  ASSERT_TRUE(mask_bmp.is_valid());

  std::vector<uint32_t> read_pixels(24u * 24u, 0);
  ASSERT_EQ(::GetDIBits(dc, color_bmp.get(), 0, 24, read_pixels.data(), &bi32,
                        DIB_RGB_COLORS),
            24);

  // Badge is placed in top-right (target_w=24, badge_w=12, badge_x=12,
  // badge_y=0). Transparent badge corner at (x=12, visual_y=0 -> dib_y=23)
  // outside base logo (base logo covers [0..17]x[6..23]) should remain
  // transparent (alpha=0).
  EXPECT_EQ(read_pixels[23 * 24 + 12] >> 24, 0u);

  // Opaque badge pixel at (x=18, visual_y=4 -> dib_y=19) must be opaque red
  // (alpha=255).
  EXPECT_EQ(read_pixels[19 * 24 + 18] >> 24, 255u);
  EXPECT_GT((read_pixels[19 * 24 + 18] >> 16) & 0xFF, 200u);

  // Base logo pixel at (x=4, visual_y=20 -> dib_y=3) retains base logo alpha
  // (~128).
  const uint32_t base_alpha = read_pixels[3 * 24 + 4] >> 24;
  EXPECT_GE(base_alpha, 126u);
  EXPECT_LE(base_alpha, 130u);

  // Base logo pixel in lower-right below badge column (x=16, visual_y=20 ->
  // dib_y=3) retains base logo alpha (~128).
  const uint32_t lower_right_base_alpha = read_pixels[3 * 24 + 16] >> 24;
  EXPECT_GE(lower_right_base_alpha, 126u);
  EXPECT_LE(lower_right_base_alpha, 130u);
}

TEST(UiUtilTest, CreateIconFromBitmap_BadgeOverlay_TallAspectRatio) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  base::win::ScopedGetDC dc(nullptr);

  // Create a 24x92 24bpp tall rectangular logo bitmap.
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
  const size_t bytes_tall = CalculateDIBStride(24, 24) * 92;
  // SAFETY: `bmp_tall` is a 24bpp DIB section of 24x92 pixels, with byte size
  // exactly equal to `bytes_tall`.
  base::span<uint8_t> span_tall =
      UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(bits_tall), bytes_tall));
  std::ranges::fill(span_tall, 0x80);

  // Create a 16x16 badge icon.
  BITMAPINFO bi_badge = {};
  bi_badge.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi_badge.bmiHeader.biWidth = 16;
  bi_badge.bmiHeader.biHeight = 16;
  bi_badge.bmiHeader.biPlanes = 1;
  bi_badge.bmiHeader.biBitCount = 24;
  bi_badge.bmiHeader.biCompression = BI_RGB;
  void* bits_badge = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp_badge(::CreateDIBSection(
      dc, &bi_badge, DIB_RGB_COLORS, &bits_badge, nullptr, 0));
  ASSERT_TRUE(bmp_badge.is_valid());
  const size_t bytes_badge = CalculateDIBStride(16, 24) * 16;
  // SAFETY: `bmp_badge` is a 24bpp DIB section of 16x16 pixels, with byte size
  // exactly equal to `bytes_badge`.
  base::span<uint8_t> span_badge = UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(bits_badge), bytes_badge));
  std::ranges::fill(span_badge, 0xFF);
  base::win::ScopedGDIObject<HICON> badge_icon =
      CreateIconFromHBitmap(bmp_badge.get(), 16, 16);
  ASSERT_TRUE(badge_icon.is_valid());

  // In a 32x32 target with badge:
  // base_w = base_h = 24.
  // 24x92 fits to height: dst_h = base_h = 24.
  // dst_w = MulDiv(24, base_h, 92) = 24 * 24 / 92 = 6.
  // (If incorrectly using target_h = 32: dst_w = 24 * 32 / 92 = 8).
  // dst_x = (base_w - dst_w) / 2 = (24 - 6) / 2 = 9.
  // dst_y = target_h - dst_h = 32 - 24 = 8.
  // Active base logo spans x in [9..14], visual_y in [8..31].
  base::win::ScopedGDIObject<HICON> badged_icon = CreateIconFromHBitmap(
      bmp_tall.get(), 32, 32, /*dpi=*/0, std::nullopt, badge_icon.get());
  ASSERT_TRUE(badged_icon.is_valid());

  ICONINFO info = {};
  ASSERT_TRUE(::GetIconInfo(badged_icon.get(), &info));
  base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);
  ASSERT_TRUE(mask_bmp.is_valid());

  // Inspect the 1bpp mask: 32x32.
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

  const size_t row_bytes = CalculateDIBStride(32, 1);
  std::vector<uint8_t> mask_pixels(row_bytes * 32, 0);
  ASSERT_EQ(
      ::GetDIBits(dc, mask_bmp.get(), 0, 32, mask_pixels.data(),
                  reinterpret_cast<BITMAPINFO*>(&mask_bi), DIB_RGB_COLORS),
      32);

  // In bottom-up DIB rows: visual_y = 20 is dib_y = 32 - 1 - 20 = 11.
  // At visual_y = 20:
  // Active base logo columns are x in [9..14].
  // Columns 0..8 are transparent margin (mask bit 1).
  // Columns 9..14 are opaque logo (mask bit 0).
  // Columns 15..31 are transparent margin (mask bit 1).
  // If dst_w were incorrectly 8 (dst_x = 8, spanning 8..15), col 8 would be 0.
  // With correct dst_w = 6 (dst_x = 9, spanning 9..14), col 8 is 1.
  auto get_mask_bit = [&](int x, int dib_y) -> bool {
    const uint8_t byte = mask_pixels[dib_y * row_bytes + (x / 8)];
    return (byte & (0x80 >> (x % 8))) != 0;
  };

  // Col 8 must be transparent margin (bit 1).
  EXPECT_TRUE(get_mask_bit(8, 11));

  // Col 9, 11, 14 must be opaque base logo (bit 0).
  EXPECT_FALSE(get_mask_bit(9, 11));
  EXPECT_FALSE(get_mask_bit(11, 11));
  EXPECT_FALSE(get_mask_bit(14, 11));

  // Col 15 must be transparent margin (bit 1).
  EXPECT_TRUE(get_mask_bit(15, 11));
}

TEST(UiUtilTest, CreateIconFromBitmap_MaskBitmapProperties) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  base::win::ScopedGetDC dc(nullptr);

  // Validates mask bitmap properties (bmWidthBytes, bmBitsPixel, bmPlanes) via
  // ::GetObject for various icon dimensions. In Windows GDI, DDBs created via
  // ::CreateBitmap have 16-bit (WORD) aligned scanlines (CalculateDDBStride),
  // which differs from 32-bit (DWORD) aligned DIBs (CalculateDIBStride).
  constexpr int kTestSizes[] = {16, 24, 32, 48, 64};
  for (int size : kTestSizes) {
    const int expected_word_stride = CalculateDDBStride(size);

    // 1. Color-keyed 24bpp icon.
    {
      BITMAPINFO bi24 = {};
      bi24.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bi24.bmiHeader.biWidth = size;
      bi24.bmiHeader.biHeight = size;
      bi24.bmiHeader.biPlanes = 1;
      bi24.bmiHeader.biBitCount = 24;
      bi24.bmiHeader.biCompression = BI_RGB;

      void* bits24 = nullptr;
      base::win::ScopedGDIObject<HBITMAP> bmp24(
          ::CreateDIBSection(dc, &bi24, DIB_RGB_COLORS, &bits24, nullptr, 0));
      ASSERT_TRUE(bmp24.is_valid());

      base::win::ScopedGDIObject<HICON> icon =
          CreateIconFromHBitmap(bmp24.get(), size, size);
      ASSERT_TRUE(icon.is_valid());

      ICONINFO info = {};
      ASSERT_TRUE(::GetIconInfo(icon.get(), &info));
      base::win::ScopedGDIObject<HBITMAP> color_bmp(info.hbmColor);
      base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);
      ASSERT_TRUE(color_bmp.is_valid());
      ASSERT_TRUE(mask_bmp.is_valid());

      BITMAP bm = {};
      ASSERT_NE(::GetObject(mask_bmp.get(), sizeof(BITMAP), &bm), 0);
      EXPECT_EQ(bm.bmWidth, size);
      EXPECT_EQ(bm.bmHeight, size);
      EXPECT_EQ(bm.bmBitsPixel, 1);
      EXPECT_EQ(bm.bmPlanes, 1);
      EXPECT_EQ(bm.bmWidthBytes, expected_word_stride);

      // Verify that for sizes like 16 and 48, bmWidthBytes is strictly 16-bit
      // WORD-aligned (2 and 6 bytes) rather than 32-bit DWORD-aligned (4 and 8
      // bytes).
      if (size == 16 || size == 48) {
        EXPECT_NE(bm.bmWidthBytes,
                  static_cast<int>(CalculateDIBStride(size, 1)));
      }
    }

    // 2. 32bpp alpha icon.
    {
      BITMAPINFO bi32 = {};
      bi32.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bi32.bmiHeader.biWidth = size;
      bi32.bmiHeader.biHeight = size;
      bi32.bmiHeader.biPlanes = 1;
      bi32.bmiHeader.biBitCount = 32;
      bi32.bmiHeader.biCompression = BI_RGB;

      void* bits32 = nullptr;
      base::win::ScopedGDIObject<HBITMAP> bmp32(
          ::CreateDIBSection(dc, &bi32, DIB_RGB_COLORS, &bits32, nullptr, 0));
      ASSERT_TRUE(bmp32.is_valid());
      // SAFETY: `bmp32` is a 32bpp DIB section of `size x size` pixels,
      // containing exactly `size * size` 32-bit DWORDs.
      base::span<uint32_t> pixels32 = UNSAFE_BUFFERS(base::span(
          static_cast<uint32_t*>(bits32), static_cast<size_t>(size) * size));
      std::ranges::fill(pixels32, (128u << 24) | 0x00FF0000);

      base::win::ScopedGDIObject<HICON> icon =
          CreateIconFromHBitmap(bmp32.get(), size, size);
      ASSERT_TRUE(icon.is_valid());

      ICONINFO info = {};
      ASSERT_TRUE(::GetIconInfo(icon.get(), &info));
      base::win::ScopedGDIObject<HBITMAP> color_bmp(info.hbmColor);
      base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);
      ASSERT_TRUE(color_bmp.is_valid());
      ASSERT_TRUE(mask_bmp.is_valid());

      BITMAP bm = {};
      ASSERT_NE(::GetObject(mask_bmp.get(), sizeof(BITMAP), &bm), 0);
      EXPECT_EQ(bm.bmWidth, size);
      EXPECT_EQ(bm.bmHeight, size);
      EXPECT_EQ(bm.bmBitsPixel, 1);
      EXPECT_EQ(bm.bmPlanes, 1);
      EXPECT_EQ(bm.bmWidthBytes, expected_word_stride);
    }
  }
}

TEST(UiUtilTest, CalculateDIBStride) {
  // Invalid inputs (non-positive dimensions or bpp) return 0.
  EXPECT_EQ(CalculateDIBStride(0, 24), 0u);
  EXPECT_EQ(CalculateDIBStride(-10, 24), 0u);
  EXPECT_EQ(CalculateDIBStride(32, 0), 0u);
  EXPECT_EQ(CalculateDIBStride(32, -8), 0u);

  // 1bpp monochrome masks: aligned to 32-bit (4-byte) boundary.
  EXPECT_EQ(CalculateDIBStride(1, 1), 4u);
  EXPECT_EQ(CalculateDIBStride(31, 1), 4u);
  EXPECT_EQ(CalculateDIBStride(32, 1), 4u);
  EXPECT_EQ(CalculateDIBStride(33, 1), 8u);
  EXPECT_EQ(CalculateDIBStride(48, 1), 8u);
  EXPECT_EQ(CalculateDIBStride(64, 1), 8u);

  // 8bpp grayscale / palette DIBs: 1 byte per pixel.
  EXPECT_EQ(CalculateDIBStride(1, 8), 4u);
  EXPECT_EQ(CalculateDIBStride(4, 8), 4u);
  EXPECT_EQ(CalculateDIBStride(5, 8), 8u);

  // 24bpp RGB DIBs: 3 bytes per pixel, padded to 4-byte multiple.
  EXPECT_EQ(CalculateDIBStride(1, 24), 4u);
  EXPECT_EQ(CalculateDIBStride(2, 24), 8u);
  EXPECT_EQ(CalculateDIBStride(3, 24), 12u);
  EXPECT_EQ(CalculateDIBStride(4, 24), 12u);
  EXPECT_EQ(CalculateDIBStride(16, 24), 48u);
  EXPECT_EQ(CalculateDIBStride(24, 24), 72u);
  EXPECT_EQ(CalculateDIBStride(32, 24), 96u);
  EXPECT_EQ(CalculateDIBStride(48, 24), 144u);

  // 32bpp ARGB DIBs: 4 bytes per pixel, inherently 4-byte aligned.
  EXPECT_EQ(CalculateDIBStride(1, 32), 4u);
  EXPECT_EQ(CalculateDIBStride(16, 32), 64u);
  EXPECT_EQ(CalculateDIBStride(32, 32), 128u);
  EXPECT_EQ(CalculateDIBStride(48, 32), 192u);
}

TEST(UiUtilTest, CalculateDDBStride) {
  // Invalid inputs (non-positive dimensions or bpp) return 0.
  EXPECT_EQ(CalculateDDBStride(0, 1), 0u);
  EXPECT_EQ(CalculateDDBStride(-10, 1), 0u);
  EXPECT_EQ(CalculateDDBStride(32, 0), 0u);
  EXPECT_EQ(CalculateDDBStride(32, -8), 0u);

  // 1bpp monochrome masks: aligned to 16-bit (2-byte) boundary.
  EXPECT_EQ(CalculateDDBStride(1, 1), 2u);
  EXPECT_EQ(CalculateDDBStride(15, 1), 2u);
  EXPECT_EQ(CalculateDDBStride(16, 1), 2u);
  EXPECT_EQ(CalculateDDBStride(17, 1), 4u);
  EXPECT_EQ(CalculateDDBStride(32, 1), 4u);
  EXPECT_EQ(CalculateDDBStride(48, 1), 6u);
  EXPECT_EQ(CalculateDDBStride(64, 1), 8u);

  // 24bpp DDBs: 3 bytes per pixel, aligned to 16-bit boundary.
  EXPECT_EQ(CalculateDDBStride(1, 24), 4u);
  EXPECT_EQ(CalculateDDBStride(2, 24), 6u);
  EXPECT_EQ(CalculateDDBStride(3, 24), 10u);
  EXPECT_EQ(CalculateDDBStride(4, 24), 12u);
}

TEST(UiUtilTest, CalculateBitmapStride) {
  // Invalid inputs return 0.
  EXPECT_EQ(CalculateBitmapStride<1>(0, 8), 0u);
  EXPECT_EQ(CalculateBitmapStride<1>(-5, 8), 0u);
  EXPECT_EQ(CalculateBitmapStride<1>(10, 0), 0u);
  EXPECT_EQ(CalculateBitmapStride<1>(10, -8), 0u);

  // 1-byte alignment (e.g. unpadded packed bytes).
  EXPECT_EQ(CalculateBitmapStride<1>(1, 1), 1u);
  EXPECT_EQ(CalculateBitmapStride<1>(8, 1), 1u);
  EXPECT_EQ(CalculateBitmapStride<1>(9, 1), 2u);

  // 2-byte alignment matches CalculateDDBStride.
  EXPECT_EQ(CalculateBitmapStride<2>(17, 1), CalculateDDBStride(17, 1));
  EXPECT_EQ(CalculateBitmapStride<2>(3, 24), CalculateDDBStride(3, 24));

  // 4-byte alignment matches CalculateDIBStride.
  EXPECT_EQ(CalculateBitmapStride<4>(33, 1), CalculateDIBStride(33, 1));
  EXPECT_EQ(CalculateBitmapStride<4>(1, 24), CalculateDIBStride(1, 24));
  EXPECT_EQ(CalculateBitmapStride<4>(16, 32), CalculateDIBStride(16, 32));

  // 8-byte alignment.
  EXPECT_EQ(CalculateBitmapStride<8>(1, 8), 8u);
  EXPECT_EQ(CalculateBitmapStride<8>(8, 8), 8u);
  EXPECT_EQ(CalculateBitmapStride<8>(9, 8), 16u);
}

TEST(UiUtilTest, CreateIconFromHBitmap_BadgeFailureFallback) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  base::win::ScopedGetDC dc(nullptr);

  // Create a 32x32 32bpp base logo with per-pixel alpha.
  BITMAPINFO bi32 = {};
  bi32.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi32.bmiHeader.biWidth = 32;
  bi32.bmiHeader.biHeight = -32;  // top-down
  bi32.bmiHeader.biPlanes = 1;
  bi32.bmiHeader.biBitCount = 32;
  bi32.bmiHeader.biCompression = BI_RGB;

  void* bits32 = nullptr;
  base::win::ScopedGDIObject<HBITMAP> bmp32(
      ::CreateDIBSection(dc, &bi32, DIB_RGB_COLORS, &bits32, nullptr, 0));
  ASSERT_TRUE(bmp32.is_valid());
  // SAFETY: `bmp32` is a 32bpp top-down DIB section of 32x32 pixels, containing
  // exactly 1024 32-bit DWORDs.
  base::span<uint32_t> pixels32 = UNSAFE_BUFFERS(
      base::span(static_cast<uint32_t*>(bits32), 32u * 32u));
  std::ranges::fill(pixels32, (255u << 24) | 0x00FF0000);  // Opaque red

  // Pass an invalid / bogus HICON handle as the badge_icon.
  HICON bogus_badge =
      reinterpret_cast<HICON>(static_cast<uintptr_t>(0xDEADBEEF));

  // Should successfully fall back to unbadged icon creation.
  bool badge_applied = true;
  base::win::ScopedGDIObject<HICON> fallback_icon = CreateIconFromHBitmap(
      bmp32.get(), 32, 32, /*dpi=*/0, std::nullopt, bogus_badge,
      &badge_applied);
  ASSERT_TRUE(fallback_icon.is_valid());
  EXPECT_FALSE(badge_applied);

  ICONINFO info = {};
  ASSERT_TRUE(::GetIconInfo(fallback_icon.get(), &info));
  base::win::ScopedGDIObject<HBITMAP> color_bmp(info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> mask_bmp(info.hbmMask);
  ASSERT_TRUE(color_bmp.is_valid());
  ASSERT_TRUE(mask_bmp.is_valid());

  BITMAP bm = {};
  ASSERT_NE(::GetObject(color_bmp.get(), sizeof(BITMAP), &bm), 0);
  EXPECT_EQ(bm.bmWidth, 32);
  EXPECT_EQ(bm.bmHeight, 32);

  // Inspect the pixels: because it fell back to unbadged creation, the base
  // logo occupies the full 32x32 canvas (not shrunken to 75% at lower-left).
  std::vector<uint32_t> inspect_pixels(32 * 32);
  ASSERT_EQ(::GetDIBits(dc, color_bmp.get(), 0, 32, inspect_pixels.data(),
                        &bi32, DIB_RGB_COLORS),
            32);
  // Check that the top-right corner (which would have been empty space in a
  // badged layout) is populated by the base logo. `color_bmp` is 24bpp, so GDI
  // sets the reserved high byte to 0 in 32bpp GetDIBits output.
  EXPECT_EQ(inspect_pixels[0] & 0x00FFFFFF, 0x00FF0000u);
  EXPECT_EQ(inspect_pixels[31] & 0x00FFFFFF, 0x00FF0000u);
  EXPECT_EQ(inspect_pixels[16 * 32 + 16] & 0x00FFFFFF, 0x00FF0000u);

  // Verify that a true-alpha 32bpp bitmap with a badge failure retains 32bpp
  // alpha blending when falling back to unbadged creation rather than degrading
  // to a 24bpp color-keyed icon.
  std::ranges::fill(pixels32, 0);
  pixels32[0] = (128u << 24) | 0x00FF0000;  // Partial alpha
  bool alpha_badge_applied = true;
  base::win::ScopedGDIObject<HICON> alpha_fallback = CreateIconFromHBitmap(
      bmp32.get(), 32, 32, /*dpi=*/0, std::nullopt, bogus_badge,
      &alpha_badge_applied);
  ASSERT_TRUE(alpha_fallback.is_valid());
  EXPECT_FALSE(alpha_badge_applied);
  ICONINFO alpha_info = {};
  ASSERT_TRUE(::GetIconInfo(alpha_fallback.get(), &alpha_info));
  base::win::ScopedGDIObject<HBITMAP> alpha_color(alpha_info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> alpha_mask(alpha_info.hbmMask);
  BITMAP alpha_bm = {};
  ASSERT_NE(::GetObject(alpha_color.get(), sizeof(BITMAP), &alpha_bm), 0);
  EXPECT_EQ(alpha_bm.bmBitsPixel, 32);
}

TEST(UiUtilTest, CreateIconFromHBitmap_BadgeAppliedReportsSuccess) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  base::win::ScopedGetDC dc(nullptr);

  auto make_alpha_dib = [&dc](int size, uint32_t argb) {
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    base::win::ScopedGDIObject<HBITMAP> bmp(
        ::CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0));
    if (bmp.is_valid() && bits) {
      // SAFETY: `bmp` is a 32bpp top-down DIB section of `size x size` pixels,
      // containing exactly `size * size` 32-bit DWORDs.
      base::span<uint32_t> pixels = UNSAFE_BUFFERS(base::span(
          static_cast<uint32_t*>(bits), static_cast<size_t>(size) * size));
      std::ranges::fill(pixels, argb);
      // A fully transparent pixel plus opaque pixels makes this a true
      // per-pixel alpha bitmap for `Create32bppAlphaIcon()`.
      pixels[0] = 0;
    }
    return bmp;
  };

  // Badge source: opaque green 16x16 (matches `GetBadgeRect(32, 32)`).
  base::win::ScopedGDIObject<HBITMAP> badge_bmp =
      make_alpha_dib(16, (255u << 24) | 0x0000FF00);
  ASSERT_TRUE(badge_bmp.is_valid());
  base::win::ScopedGDIObject<HICON> badge_icon =
      CreateIconFromHBitmap(badge_bmp.get(), 16, 16);
  ASSERT_TRUE(badge_icon.is_valid());

  // Base logo: opaque red 32x32 with per-pixel alpha.
  base::win::ScopedGDIObject<HBITMAP> logo_bmp =
      make_alpha_dib(32, (255u << 24) | 0x00FF0000);
  ASSERT_TRUE(logo_bmp.is_valid());

  bool badge_applied = false;
  base::win::ScopedGDIObject<HICON> badged_icon = CreateIconFromHBitmap(
      logo_bmp.get(), 32, 32, /*dpi=*/0, std::nullopt, badge_icon.get(),
      &badge_applied);
  ASSERT_TRUE(badged_icon.is_valid());
  EXPECT_TRUE(badge_applied);

  // `badge_applied` is optional: omitting it must not change the result.
  base::win::ScopedGDIObject<HICON> badged_icon_no_out = CreateIconFromHBitmap(
      logo_bmp.get(), 32, 32, /*dpi=*/0, std::nullopt, badge_icon.get());
  EXPECT_TRUE(badged_icon_no_out.is_valid());

  // An unbadged request never reports a badge as applied.
  bool unbadged_applied = true;
  base::win::ScopedGDIObject<HICON> unbadged_icon = CreateIconFromHBitmap(
      logo_bmp.get(), 32, 32, /*dpi=*/0, std::nullopt, /*badge_icon=*/nullptr,
      &unbadged_applied);
  ASSERT_TRUE(unbadged_icon.is_valid());
  EXPECT_FALSE(unbadged_applied);
}

}  // namespace updater::ui

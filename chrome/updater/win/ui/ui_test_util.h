// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_UI_TEST_UTIL_H_
#define CHROME_UPDATER_WIN_UI_UI_TEST_UTIL_H_

#include <windows.h>

#include "base/win/scoped_gdi_object.h"

namespace updater::test {

// Creates a 24 bpp DIB section of `width` x `height`, compatible with `dc`.
// Returns an invalid object if creation failed, which the caller must check.
//
// Pixel bits are not returned; callers read back through ::GetPixel() to handle
// DIB row ordering and channel layout safely.
inline base::win::ScopedGDIObject<HBITMAP> CreateTestDIB24(HDC dc,
                                                           int width,
                                                           int height) {
  BITMAPINFO bi = {.bmiHeader = {.biSize = sizeof(BITMAPINFOHEADER),
                                 .biWidth = width,
                                 .biHeight = height,
                                 .biPlanes = 1,
                                 .biBitCount = 24,
                                 .biCompression = BI_RGB}};
  void* bits = nullptr;
  return base::win::ScopedGDIObject<HBITMAP>(
      ::CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0));
}

}  // namespace updater::test

#endif  // CHROME_UPDATER_WIN_UI_UI_TEST_UTIL_H_

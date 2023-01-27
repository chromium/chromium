// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEADLESS_TEST_BITMAP_UTILS_H_
#define COMPONENTS_HEADLESS_TEST_BITMAP_UTILS_H_

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace headless {

// Find a rectangle with the specified color |rect_color| and verify that
// it is surrounded by the background color |bkgr_color|.
bool CheckColoredRect(const SkBitmap& bitmap,
                      SkColor rect_color,
                      SkColor bkgr_color,
                      int margins);

bool CheckColoredRect(const SkBitmap& bitmap,
                      SkColor rect_color,
                      SkColor bkgr_color);

}  // namespace headless

#endif  // COMPONENTS_HEADLESS_TEST_BITMAP_UTILS_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_IMAGE_VISUAL_DIFF_H_
#define COMPONENTS_WEBAPPS_BROWSER_IMAGE_VISUAL_DIFF_H_

#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {
// Returns true if the two bitmaps have more than a 10% difference, or false
// otherwise. A pixel is considered "different" if any of its RGBA values in the
// 'after' bitmap does not precisely match its corresponding pixel in the
// 'before' bitmap.For example, if a pixel changes from grey ([127, 127, 127,
// 255]) to white ([255, 255, 255, 255]), it counts as a difference. Returns
// false otherwise.
bool HasMoreThanTenPercentImageDiff(const SkBitmap* before,
                                    const SkBitmap* after);
}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_BROWSER_IMAGE_VISUAL_DIFF_H_

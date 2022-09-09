// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MONOGRAM_UTILS_H_
#define CHROME_BROWSER_UI_MONOGRAM_UTILS_H_

#include <string>

#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Canvas;
}

namespace monogram {

// Draws a monogram in a colored circle on the passed-in `canvas`.
// |monogram_text| is a std::u16string in order to support 2 letter
// monograms.
void DrawMonogramInCanvas(gfx::Canvas* canvas,
                          int canvas_size,
                          int circle_size,
                          const std::u16string& monogram_text,
                          SkColor monoggram_color,
                          SkColor background_color);

}  // namespace monogram

#endif  // CHROME_BROWSER_UI_MONOGRAM_UTILS_H_

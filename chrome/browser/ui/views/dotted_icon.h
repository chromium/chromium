// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOTTED_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_DOTTED_ICON_H_

#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Canvas;
class Rect;
}  // namespace gfx

// Using the `canvas` this function paints a half-full/half-dotted ring at
// `ring_bounds`, which is also used to determine where the ring is painted.
// Used to draw pending/inactive states around circular icons. A common pattern
// for this usage is to shrink the desired icon and then draw the dotted path
// using the original icon bounds.
// `opacity_ratio` is expected to have a value between 0.0 and 1.0, otherwise it
// will clamped. It can be used with animation current value to fade in the
// ring.
void PaintRingDottedPath(gfx::Canvas* canvas,
                         const gfx::Rect& ring_bounds,
                         SkColor ring_color,
                         double opacity_ratio = 1.);

#endif  // CHROME_BROWSER_UI_VIEWS_DOTTED_ICON_H_

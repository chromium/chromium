// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_PAINT_UTILS_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_PAINT_UTILS_LINUX_H_

#include "ui/base/ui_base_types.h"
#include "ui/gfx/shadow_value.h"

class SkRRect;

namespace gfx {
class Canvas;
class Insets;
}  // namespace gfx

namespace views {
class FrameBackground;
class View;
}  // namespace views

// Paint the window borders, shadows and the background of the top bar area for
// BrowserFrameViewLinux and PictureInPictureBrowserFrameView on Linux.
void PaintRestoredFrameBorderLinux(gfx::Canvas& canvas,
                                   const views::View& view,
                                   views::FrameBackground* frame_background,
                                   const SkRRect& clip,
                                   bool showing_shadow,
                                   bool is_active,
                                   const gfx::Insets& border,
                                   const gfx::ShadowValues& shadow_values,
                                   bool tiled);

// Get the insets from the native window edge to the client view when the window
// is restored for BrowserFrameViewLayoutLinux and
// PictureInPictureBrowserFrameView on Linux.
gfx::Insets GetRestoredFrameBorderInsetsLinux(
    bool showing_shadow,
    const gfx::Insets& default_border,
    const gfx::ShadowValues& shadow_values,
    int resize_border);

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_PAINT_UTILS_LINUX_H_

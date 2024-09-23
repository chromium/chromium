// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_BUBBLE_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_BUBBLE_UTILS_H_

#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Rect;
class Outsets;
class Size;
}  // namespace gfx

namespace ash {

// Helper to compute bubble bounds around caret and also make sure it's always
// fully visible on-screen. The caret bounds and returned bubble bounds are
// based on in screen coordinates.
gfx::Rect GetBubbleBoundsAroundCaret(const gfx::Rect& caret_bounds,
                                     const gfx::Outsets& bubble_border_outsets,
                                     const gfx::Size& bubble_size);

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_BUBBLE_UTILS_H_

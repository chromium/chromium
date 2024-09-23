// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/emoji/bubble_utils.h"

#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace {

constexpr int kPaddingAroundCursor = 8;
constexpr int kScreenEdgePadding = 16;

}  // namespace

namespace ash {

gfx::Rect GetBubbleBoundsAroundCaret(const gfx::Rect& caret_bounds,
                                     const gfx::Outsets& bubble_border_outsets,
                                     const gfx::Size& bubble_size) {
  gfx::Rect screen_work_area = display::Screen::GetScreen()
                                   ->GetDisplayMatching(caret_bounds)
                                   .work_area();

  screen_work_area.Inset(kScreenEdgePadding);

  gfx::Rect anchor = caret_bounds;
  anchor.Outset(gfx::Outsets::VH(kPaddingAroundCursor, 0));

  // Try to place it under at the bottom left of the selection.
  gfx::Rect contents_bounds = gfx::Rect(anchor.bottom_left(), bubble_size);

  // If horizontally offscreen, just move it to the right edge of the screen.
  if (contents_bounds.right() > screen_work_area.right()) {
    contents_bounds.set_x(screen_work_area.right() - bubble_size.width());
  }

  // If vertically offscreen, try above the selection.
  if (contents_bounds.bottom() > screen_work_area.bottom()) {
    contents_bounds.set_y(anchor.y() - bubble_size.height());
  }

  // If still vertically offscreen, just move it to the bottom of the screen.
  if (contents_bounds.y() < screen_work_area.y()) {
    contents_bounds.set_y(screen_work_area.bottom() - bubble_size.height());
  }

  // Compute widget bounds, which includes the border and shadow around the
  // main contents. Then, adjust again to ensure the whole widget is onscreen.
  gfx::Rect widget_bounds(contents_bounds);
  widget_bounds.Outset(bubble_border_outsets);
  widget_bounds.AdjustToFit(screen_work_area);

  return widget_bounds;
}

}  // namespace ash

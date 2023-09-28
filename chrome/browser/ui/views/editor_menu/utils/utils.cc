// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/utils/utils.h"

#include <algorithm>

#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos::editor_menu {

namespace {

constexpr int kEditorMenuMinWidthDip = 320;
constexpr int kEditorMenuMaxWidthDip = 592;

// Spacing between the editor menu and the anchor view (context menu).
constexpr int kEditorMenuMarginDip = 8;

}  // namespace

int GetEditorMenuWidth(int anchor_view_width) {
  return std::clamp(anchor_view_width, kEditorMenuMinWidthDip,
                    kEditorMenuMaxWidthDip);
}

// TODO(b/302043981): The editor menu can still appear off-screen when the
// anchor view is very tall. Improve the bounds logic to deal with such cases.
gfx::Rect GetEditorMenuBounds(const gfx::Rect& anchor_view_bounds,
                              const gfx::Size& editor_menu_preferred_size) {
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()
          ->GetDisplayMatching(anchor_view_bounds)
          .work_area();

  // Try to position the editor menu above the anchor view. If that places the
  // editor menu partially offscreen, position it below the anchor view instead.
  int y = anchor_view_bounds.y() - kEditorMenuMarginDip -
          editor_menu_preferred_size.height();
  if (y < screen_work_area.y()) {
    y = anchor_view_bounds.bottom() + kEditorMenuMarginDip;
  }

  // Try to align the left edges of the editor menu and anchor view. If that
  // places the editor menu partially offscreen, align the right edges instead.
  int x = anchor_view_bounds.x();
  if (x + editor_menu_preferred_size.width() > screen_work_area.right()) {
    x = anchor_view_bounds.right() - editor_menu_preferred_size.width();
  }

  return gfx::Rect({x, y}, editor_menu_preferred_size);
}

}  // namespace chromeos::editor_menu

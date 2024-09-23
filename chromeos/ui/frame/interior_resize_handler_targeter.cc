// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/interior_resize_handler_targeter.h"

#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "ui/aura/window.h"

namespace chromeos {

InteriorResizeHandleTargeter::InteriorResizeHandleTargeter(
    WindowStateTypeCallback window_state_type_cb)
    : window_state_type_cb_(std::move(window_state_type_cb)) {
  SetInsets(gfx::Insets(chromeos::kResizeInsideBoundsSize));
}

InteriorResizeHandleTargeter::~InteriorResizeHandleTargeter() = default;

bool InteriorResizeHandleTargeter::GetHitTestRects(
    aura::Window* target,
    gfx::Rect* hit_test_rect_mouse,
    gfx::Rect* hit_test_rect_touch) const {
  if (target == window() && window()->parent() &&
      window()->parent()->targeter()) {
    // Defer to the parent's targeter on whether |window_| should be able to
    // receive the event. This should be EasyResizeWindowTargeter, which is
    // installed on the container window, and is necessary for
    // kResizeOutsideBoundsSize to work.
    return window()->parent()->targeter()->GetHitTestRects(
        target, hit_test_rect_mouse, hit_test_rect_touch);
  }

  return WindowTargeter::GetHitTestRects(target, hit_test_rect_mouse,
                                         hit_test_rect_touch);
}

bool InteriorResizeHandleTargeter::ShouldUseExtendedBounds(
    const aura::Window* target) const {
  // Fullscreen/maximized windows can't be drag-resized.
  if (IsMaximizedOrFullscreenOrPinnedWindowStateType(
          window_state_type_cb_.Run(window()))) {
    return false;
  }

  // The shrunken hit region only applies to children of |window()|.
  return target->parent() == window();
}

}  // namespace chromeos

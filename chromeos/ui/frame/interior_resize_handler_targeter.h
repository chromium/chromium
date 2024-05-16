// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_INTERIOR_RESIZE_HANDLER_TARGETER_H_
#define CHROMEOS_UI_FRAME_INTERIOR_RESIZE_HANDLER_TARGETER_H_

#include "base/functional/callback.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/window_targeter.h"

namespace chromeos {

// This window targeter reserves space for the portion of the resize handles
// that extend within a top level window. This targeter is should only be
// installed on frame windows.
class InteriorResizeHandleTargeter : public aura::WindowTargeter {
 public:
  using WindowStateTypeCallback =
      base::RepeatingCallback<WindowStateType(const aura::Window*)>;

  explicit InteriorResizeHandleTargeter(
      WindowStateTypeCallback window_state_type_cb);
  InteriorResizeHandleTargeter(const InteriorResizeHandleTargeter&) = delete;
  InteriorResizeHandleTargeter& operator=(const InteriorResizeHandleTargeter&) =
      delete;
  ~InteriorResizeHandleTargeter() override;

  // aura::WindowTargeter:
  bool GetHitTestRects(aura::Window* target,
                       gfx::Rect* hit_test_rect_mouse,
                       gfx::Rect* hit_test_rect_touch) const override;
  bool ShouldUseExtendedBounds(const aura::Window* target) const override;

 private:
  // Callback that gets the WindowStateType.
  const WindowStateTypeCallback window_state_type_cb_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_INTERIOR_RESIZE_HANDLER_TARGETER_H_

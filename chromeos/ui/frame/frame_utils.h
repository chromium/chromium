// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_FRAME_UTILS_H_
#define CHROMEOS_UI_FRAME_FRAME_UTILS_H_

#include "base/component_export.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class NonClientFrameView;
}

namespace chromeos {

// Returns the HitTestCompat for the specified point.
COMPONENT_EXPORT(CHROMEOS_UI_FRAME)
int FrameBorderNonClientHitTest(views::NonClientFrameView* view,
                                const gfx::Point& point_in_widget);

// Resolve the inferred opacity and updates the params.
COMPONENT_EXPORT(CHROMEOS_UI_FRAME)
void ResolveInferredOpacity(views::Widget::InitParams* params);

// Checks whether we should draw the restored window frame on |widget|.
COMPONENT_EXPORT(CHROMEOS_UI_FRAME)
bool ShouldUseRestoreFrame(const views::Widget* widget);

// Gets the snap direction given a button associated with left/top or
// right/bottom. Takes into account the orientation of the display.
SnapDirection GetSnapDirectionForWindow(aura::Window* window, bool left_top);

// Returns the corner radius of frame based on the state of the `native_window`
// associated with the frame.
COMPONENT_EXPORT(CHROMEOS_UI_FRAME)
int GetFrameCornerRadius(const aura::Window* native_window);

// Returns true if the ClassProperty associated with native_window of the frame
// can effect the radius of the frame.
COMPONENT_EXPORT(CHROMEOS_UI_FRAME)
bool CanPropertyEffectFrameRadius(const void* class_property_key);

// Returns true if window should have rounded corners for a given
// `window_state`.
COMPONENT_EXPORT(CHROMEOS_UI_FRAME)
bool ShouldWindowStateHaveRoundedCorners(WindowStateType window_state);

// Returns true if the `native_window` that is associated with the frame should
// have rounded corners.
COMPONENT_EXPORT(CHROMEOS_UI_FRAME)
bool ShouldWindowHaveRoundedCorners(const aura::Window* native_window);

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_FRAME_UTILS_H_

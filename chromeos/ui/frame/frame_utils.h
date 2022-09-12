// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_FRAME_UTILS_H_
#define CHROMEOS_UI_FRAME_FRAME_UTILS_H_

#include "base/component_export.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "third_party/skia/include/core/SkColor.h"
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

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_FRAME_UTILS_H_

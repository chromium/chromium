// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/glic/border/border_view.h"

namespace glic {

BorderView::BorderView() = default;

BorderView::~BorderView() = default;

void BorderView::OnPaint(gfx::Canvas* canvas) {
  // Paint onto `canvas`.
  // Call `SchedulePaint()` if the animation hasn't finished.
}

void BorderView::OnChildViewAdded(views::View* observed_view,
                                  views::View* child) {
  // Use this API to make sure our border view is always the z-top-most of the
  // contents_web_view_ of the browser view.
}

void BorderView::OnAnimationStep(base::TimeTicks timestamp) {
  // Update the border style based on`timestamp` and the motion curve(s).
}

void BorderView::OnCompositingShuttingDown(ui::Compositor* compositor) {}

// TODO(crbug.com/381424645): Implementation.
void BorderView::StartAnimation() {}

void BorderView::CancelAnimation() {}

}  // namespace glic

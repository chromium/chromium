// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view_drag_entrypoint_controller.h"

#include "content/public/common/drop_data.h"
#include "ui/views/view_class_properties.h"

MultiContentsViewDragEntrypointController::
    MultiContentsViewDragEntrypointController(views::View& drop_target_view)
    : drop_target_view_(drop_target_view) {
  CHECK_NE(nullptr, drop_target_view.parent());
}

void MultiContentsViewDragEntrypointController::OnWebContentsDragUpdate(
    const content::DropData& data,
    const gfx::PointF& point) {
  CHECK_LE(point.x(), drop_target_view_->parent()->width());

  // TODO(crbug.com/394369035): Settle on an appropriate value for this.
  constexpr int kDropEntryPointWidth = 100;

  const bool should_show_drop_zone =
      data.url.is_valid() &&
      point.x() >= drop_target_view_->parent()->width() - kDropEntryPointWidth;
  // TODO(crbug.com/394369035): Add a timer to delay showing the drop zone.
  drop_target_view_->SetVisible(should_show_drop_zone);
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"

#include "base/task/single_thread_task_runner.h"
#include "content/public/common/drop_data.h"
#include "ui/views/view_class_properties.h"

MultiContentsViewDropTargetController::MultiContentsViewDropTargetController(
    views::View& drop_target_view)
    : drop_target_view_(drop_target_view) {
  CHECK_NE(nullptr, drop_target_view.parent());
}

void MultiContentsViewDropTargetController::OnWebContentsDragUpdate(
    const content::DropData& data,
    const gfx::PointF& point) {
  CHECK_LE(point.x(), drop_target_view_->parent()->width());

  // TODO(crbug.com/394369035): Settle on an appropriate value for this.
  constexpr int kDropEntryPointWidth = 100;

  const bool should_show_drop_zone =
      data.url.is_valid() &&
      point.x() >= drop_target_view_->parent()->width() - kDropEntryPointWidth;

  UpdateDropTargetTimer(should_show_drop_zone);
}

void MultiContentsViewDropTargetController::OnWebContentsDragExit() {
  UpdateDropTargetTimer(/*should_run_timer=*/false);
}

void MultiContentsViewDropTargetController::UpdateDropTargetTimer(
    bool should_run_timer) {
  if (!should_run_timer) {
    // The view itself isn't hidden immediately. If the view is already
    // visible, then it has the responsibility of handling drags and hiding
    // itself.
    show_drop_target_timer_.Stop();
  } else if (!drop_target_view_->GetVisible() &&
             !show_drop_target_timer_.IsRunning()) {
    // TODO(crbug.com/394369035): Settle on an appropriate value for this.
    constexpr base::TimeDelta kDropTargetDelay = base::Seconds(1);
    show_drop_target_timer_.Start(
        FROM_HERE, kDropTargetDelay, this,
        &MultiContentsViewDropTargetController::ShowDropTarget);
  }
}

void MultiContentsViewDropTargetController::ShowDropTarget() {
  drop_target_view_->SetVisible(true);
}

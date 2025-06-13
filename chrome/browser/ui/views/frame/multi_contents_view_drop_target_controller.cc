// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"

#include "base/check_deref.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ref.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "content/public/common/drop_data.h"
#include "ui/views/view_class_properties.h"

MultiContentsViewDropTargetController::MultiContentsViewDropTargetController(
    MultiContentsDropTargetView& drop_target_view)
    : drop_target_view_(drop_target_view),
      drop_target_parent_view_(CHECK_DEREF(drop_target_view.parent())) {}

MultiContentsViewDropTargetController::
    ~MultiContentsViewDropTargetController() = default;

MultiContentsViewDropTargetController::DropTargetShowTimer::DropTargetShowTimer(
    MultiContentsDropTargetView::DropSide drop_side)
    : drop_side(drop_side) {}

void MultiContentsViewDropTargetController::OnWebContentsDragUpdate(
    const content::DropData& data,
    const gfx::PointF& point,
    bool is_in_split_view) {
  CHECK_LE(point.x(), drop_target_parent_view_->width());

  if (!data.url.is_valid() || is_in_split_view) {
    ResetDropTargetTimer();
    return;
  }

  const int drop_entry_point_width =
      drop_target_view_->GetPreferredSize().width();
  const bool is_rtl = base::i18n::IsRTL();
  if (point.x() >= drop_target_parent_view_->width() - drop_entry_point_width) {
    StartOrUpdateDropTargetTimer(
        is_rtl ? MultiContentsDropTargetView::DropSide::START
               : MultiContentsDropTargetView::DropSide::END);
  } else if (point.x() <= drop_entry_point_width) {
    StartOrUpdateDropTargetTimer(
        is_rtl ? MultiContentsDropTargetView::DropSide::END
               : MultiContentsDropTargetView::DropSide::START);
  } else {
    ResetDropTargetTimer();
  }
}

void MultiContentsViewDropTargetController::OnWebContentsDragExit() {
  ResetDropTargetTimer();
}

void MultiContentsViewDropTargetController::StartOrUpdateDropTargetTimer(
    MultiContentsDropTargetView::DropSide drop_side) {
  if (drop_target_view_->GetVisible()) {
    return;
  }

  if (show_drop_target_timer_.has_value()) {
    CHECK(show_drop_target_timer_->timer.IsRunning());
    show_drop_target_timer_->drop_side = drop_side;
    return;
  }

  show_drop_target_timer_.emplace(drop_side);

  show_drop_target_timer_->timer.Start(
      FROM_HERE, features::kSideBySideShowDropTargetDelay.Get(), this,
      &MultiContentsViewDropTargetController::ShowTimerDelayedDropTarget);
}

void MultiContentsViewDropTargetController::ResetDropTargetTimer() {
  show_drop_target_timer_.reset();
}

void MultiContentsViewDropTargetController::ShowTimerDelayedDropTarget() {
  CHECK(show_drop_target_timer_.has_value());
  CHECK(!drop_target_view_->GetVisible());
  drop_target_view_->Show(show_drop_target_timer_->drop_side);
  show_drop_target_timer_.reset();
}

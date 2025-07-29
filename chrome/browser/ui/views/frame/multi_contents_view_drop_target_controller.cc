// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ref.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "content/public/common/drop_data.h"
#include "ui/views/view_class_properties.h"

MultiContentsViewDropTargetController::MultiContentsViewDropTargetController(
    MultiContentsDropTargetView& drop_target_view)
    : drop_target_view_(drop_target_view),
      drop_target_parent_view_(CHECK_DEREF(drop_target_view.parent())) {}

MultiContentsViewDropTargetController::
    ~MultiContentsViewDropTargetController() {
  on_will_destroy_callback_list_.Notify();
}

MultiContentsViewDropTargetController::DropTargetShowTimer::DropTargetShowTimer(
    MultiContentsDropTargetView::DropSide drop_side)
    : drop_side(drop_side) {}

void MultiContentsViewDropTargetController::OnTabDragUpdated(
    TabDragDelegate::DragController& controller,
    const gfx::Point& point_in_screen) {
  // Only allow creating split with a single dragged tab.
  if (controller.GetSessionData().num_dragging_tabs() != 1) {
    ResetDropTargetTimer();
    drop_target_view_->Hide();
    return;
  }

  const gfx::Point point_in_parent = views::View::ConvertPointFromScreen(
      &drop_target_parent_view_.get(), point_in_screen);
  HandleDragUpdate(gfx::PointF(point_in_parent));
}

void MultiContentsViewDropTargetController::OnTabDragEntered() {}

void MultiContentsViewDropTargetController::OnTabDragExited() {
  ResetDropTargetTimer();
  drop_target_view_->Hide();
}

void MultiContentsViewDropTargetController::OnTabDragEnded() {
  ResetDropTargetTimer();
  drop_target_view_->Hide();
}

bool MultiContentsViewDropTargetController::CanDropTab() {
  // The drop target view is visible iff the last drag point was over
  // it (i.e. if the view is visible, then we can assume that the drop is
  // happening on it).
  return drop_target_view_->GetVisible() && !drop_target_view_->IsClosing();
}

void MultiContentsViewDropTargetController::HandleTabDrop(
    TabDragDelegate::DragController& controller) {
  drop_target_view_->HandleTabDrop(controller);
}

base::CallbackListSubscription
MultiContentsViewDropTargetController::RegisterWillDestroyCallback(
    base::OnceClosure callback) {
  return on_will_destroy_callback_list_.Add(std::move(callback));
}

void MultiContentsViewDropTargetController::OnWebContentsDragUpdate(
    const content::DropData& data,
    const gfx::PointF& point,
    bool is_in_split_view) {
  // "Drag update" events can still be delivered even if the point is out of the
  // contents area, particularly while the drop target is animating in and
  // shifting them.
  if ((point.x() < 0) || (point.x() > drop_target_parent_view_->width())) {
    ResetDropTargetTimer();
    return;
  }
  if (!data.url.is_valid() || is_in_split_view) {
    ResetDropTargetTimer();
    return;
  }
  HandleDragUpdate(point);
}

void MultiContentsViewDropTargetController::OnWebContentsDragExit() {
  ResetDropTargetTimer();
}

void MultiContentsViewDropTargetController::OnWebContentsDragEnded() {
  ResetDropTargetTimer();
  drop_target_view_->Hide();
}

void MultiContentsViewDropTargetController::HandleDragUpdate(
    const gfx::PointF& point_in_view) {
  CHECK_LE(0, point_in_view.x());
  CHECK_LE(point_in_view.x(), drop_target_parent_view_->width());
  const int drop_entry_point_width =
      drop_target_view_->GetMaxWidth(drop_target_parent_view_->width());
  const bool is_rtl = base::i18n::IsRTL();
  if (point_in_view.x() >=
      drop_target_parent_view_->width() - drop_entry_point_width) {
    StartOrUpdateDropTargetTimer(
        is_rtl ? MultiContentsDropTargetView::DropSide::START
               : MultiContentsDropTargetView::DropSide::END);
    return;
  } else if (point_in_view.x() <= drop_entry_point_width) {
    StartOrUpdateDropTargetTimer(
        is_rtl ? MultiContentsDropTargetView::DropSide::END
               : MultiContentsDropTargetView::DropSide::START);
    return;
  }
  ResetDropTargetTimer();
  drop_target_view_->Hide();
  return;
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

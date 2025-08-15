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
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/views/view_class_properties.h"

MultiContentsViewDropTargetController::MultiContentsViewDropTargetController(
    MultiContentsDropTargetView& drop_target_view,
    DropDelegate& drop_delegate)
    : drop_target_view_(drop_target_view),
      drop_target_parent_view_(CHECK_DEREF(drop_target_view.parent())),
      drop_delegate_(drop_delegate) {
  drop_target_view_->SetDragDelegate(this);
}

MultiContentsViewDropTargetController::
    ~MultiContentsViewDropTargetController() {
  on_will_destroy_callback_list_.Notify();
  drop_target_view_->SetDragDelegate(nullptr);
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
  HandleDragUpdate(point_in_parent);
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

bool MultiContentsViewDropTargetController::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  *formats = ui::OSExchangeData::URL;
  format_types->insert(ui::ClipboardFormatType::UrlType());
  return true;
}

bool MultiContentsViewDropTargetController::CanDrop(
    const ui::OSExchangeData& data) {
  if (!data.HasURL(ui::FilenameToURLPolicy::CONVERT_FILENAMES)) {
    return false;
  }
  auto urls = data.GetURLs(ui::FilenameToURLPolicy::CONVERT_FILENAMES);
  return urls.has_value() && !urls.value().empty();
}

void MultiContentsViewDropTargetController::OnDragEntered(
    const ui::DropTargetEvent& event) {
  if (!drop_target_view_->GetVisible()) {
    return;
  }

  CHECK(drop_target_view_->state().has_value());
  if (*drop_target_view_->state() !=
      MultiContentsDropTargetView::DropTargetState::kNudge) {
    return;
  }

  CHECK(drop_target_view_->side().has_value());
  drop_target_view_->Show(
      drop_target_view_->side().value(),
      MultiContentsDropTargetView::DropTargetState::kNudgeToFull);
}

void MultiContentsViewDropTargetController::OnDragExited() {
  if (!drop_target_view_->GetVisible()) {
    return;
  }

  // If the target is not a nudge or expanded nudge, then hide it immediately.
  // Otherwise, we should still show even when the drag exits its area.
  CHECK(drop_target_view_->state().has_value());
  if (*drop_target_view_->state() ==
      MultiContentsDropTargetView::DropTargetState::kFull) {
    drop_target_view_->Hide();
  }
}

void MultiContentsViewDropTargetController::OnDragDone() {
  drop_target_view_->Hide();
}

int MultiContentsViewDropTargetController::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_LINK;
}

views::View::DropCallback
MultiContentsViewDropTargetController::GetDropCallback(
    const ui::DropTargetEvent& event) {
  return base::BindOnce(&MultiContentsViewDropTargetController::DoDrop,
                        base::Unretained(this));
}

void MultiContentsViewDropTargetController::DoDrop(
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  CHECK(drop_target_view_->side().has_value());
  MultiContentsDropTargetView::DropSide side =
      drop_target_view_->side().value();
  drop_target_view_->Hide();
  auto urls = event.data().GetURLs(ui::FilenameToURLPolicy::CONVERT_FILENAMES);
  CHECK(urls.has_value());
  drop_delegate_->HandleLinkDrop(side, urls.value());
  output_drag_op = ui::mojom::DragOperation::kLink;
}

void MultiContentsViewDropTargetController::HandleTabDrop(
    TabDragDelegate::DragController& controller) {
  CHECK(drop_target_view_->GetVisible());
  CHECK(drop_target_view_->side().has_value());
  MultiContentsDropTargetView::DropSide side =
      drop_target_view_->side().value();
  drop_target_view_->Hide();
  drop_delegate_->HandleTabDrop(side, controller);
}

base::CallbackListSubscription
MultiContentsViewDropTargetController::RegisterWillDestroyCallback(
    base::OnceClosure callback) {
  return on_will_destroy_callback_list_.Add(std::move(callback));
}

void MultiContentsViewDropTargetController::OnWebContentsDragUpdate(
    const content::DropData& data,
    const gfx::Point& point,
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

  if (base::FeatureList::IsEnabled(features::kSideBySideDropTargetNudge)) {
    HandleDragUpdateForNudge(point);
  } else {
    HandleDragUpdate(point);
  }
}

void MultiContentsViewDropTargetController::OnWebContentsDragExit() {
  ResetDropTargetTimer();
}

void MultiContentsViewDropTargetController::OnWebContentsDragEnded() {
  ResetDropTargetTimer();
  drop_target_view_->Hide();
}

bool MultiContentsViewDropTargetController::IsDropTimerRunningForTesting() {
  return show_drop_target_timer_.has_value() &&
         show_drop_target_timer_->timer.IsRunning();
}

void MultiContentsViewDropTargetController::HandleDragUpdate(
    const gfx::Point& point_in_view) {
  CHECK_LE(0, point_in_view.x());
  CHECK_LE(point_in_view.x(), drop_target_parent_view_->width());
  const bool is_rtl = base::i18n::IsRTL();

  const int drop_entry_point_width = MultiContentsDropTargetView::GetMaxWidth(
      drop_target_parent_view_->width(),
      MultiContentsDropTargetView::DropTargetState::kFull);
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
}

void MultiContentsViewDropTargetController::HandleDragUpdateForNudge(
    const gfx::Point& point_in_view) {
  CHECK_LE(0, point_in_view.x());
  CHECK_LE(point_in_view.x(), drop_target_parent_view_->width());
  CHECK(base::FeatureList::IsEnabled(features::kSideBySideDropTargetNudge));
  const bool is_rtl = base::i18n::IsRTL();
  const float point_ratio =
      (1.0f * point_in_view.x()) / drop_target_parent_view_->width();
  const float nudge_ratio = features::kSideBySideDropTargetNudgeShowRatio.Get();

  // Either hide or show the drop target if the drag is in the trigger area.
  if (point_ratio > nudge_ratio && point_ratio < 1.0f - nudge_ratio) {
    drop_target_view_->Hide();
  } else if (point_ratio <= nudge_ratio) {
    drop_target_view_->Show(
        is_rtl ? MultiContentsDropTargetView::DropSide::END
               : MultiContentsDropTargetView::DropSide::START,
        MultiContentsDropTargetView::DropTargetState::kNudge);
  } else if (point_ratio >= 1.0f - nudge_ratio) {
    drop_target_view_->Show(
        is_rtl ? MultiContentsDropTargetView::DropSide::START
               : MultiContentsDropTargetView::DropSide::END,
        MultiContentsDropTargetView::DropTargetState::kNudge);
  }
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
  drop_target_view_->Show(show_drop_target_timer_->drop_side,
                          MultiContentsDropTargetView::DropTargetState::kFull);
  show_drop_target_timer_.reset();
}

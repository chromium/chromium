// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"

#include <algorithm>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/user_metrics.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/types/to_address.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/drop_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/views/view_class_properties.h"

MultiContentsViewDropTargetController::MultiContentsViewDropTargetController(
    MultiContentsDropTargetView& drop_target_view,
    DropDelegate& drop_delegate,
    PrefService* prefs)
    : drop_target_view_(drop_target_view),
      drop_target_parent_view_(CHECK_DEREF(drop_target_view.parent())),
      drop_delegate_(drop_delegate),
      prefs_(prefs) {
  drop_target_view_->SetDragDelegate(this);

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kSplitViewDragAndDropNudgeShownCount,
      base::BindRepeating(&MultiContentsViewDropTargetController::
                              OnDragAndDropNudgeShownCountChange,
                          base::Unretained(this)));
  OnDragAndDropNudgeShownCountChange();
  pref_change_registrar_.Add(
      prefs::kSplitViewDragAndDropNudgeUsedCount,
      base::BindRepeating(&MultiContentsViewDropTargetController::
                              OnDragAndDropNudgeUsedCountChange,
                          base::Unretained(this)));
  OnDragAndDropNudgeUsedCountChange();
}

MultiContentsViewDropTargetController::
    ~MultiContentsViewDropTargetController() {
  hide_drop_target_timer_.Stop();
  on_will_destroy_callback_list_.Notify();
  drop_target_view_->SetDragDelegate(nullptr);
}

MultiContentsViewDropTargetController::DropTargetShowTimer::DropTargetShowTimer(
    MultiContentsDropTargetView::DropSide drop_side,
    MultiContentsDropTargetView::DragType drag_type)
    : drop_side(drop_side), drag_type(drag_type) {}

void MultiContentsViewDropTargetController::OnTabDragUpdated(
    TabDragDelegate::DragController& controller,
    const gfx::Point& point_in_screen) {
  // Only allow creating split with a single dragged tab.
  if (controller.GetSessionData().num_dragging_tabs() != 1) {
    ResetDropTargetTimers();
    HideDropTarget();
    return;
  }

  const gfx::Point point_in_parent = views::View::ConvertPointFromScreen(
      &drop_target_parent_view_.get(), point_in_screen);
  if (PointOverlapsWithOSDropTarget(point_in_parent)) {
    ResetDropTargetTimers();
    HideDropTarget();
    return;
  }
  HandleDragUpdate(point_in_parent,
                   MultiContentsDropTargetView::DragType::kTab);
}

void MultiContentsViewDropTargetController::OnTabDragEntered() {}

void MultiContentsViewDropTargetController::OnTabDragExited() {
  ResetDropTargetTimers();
  HideDropTarget();
}

void MultiContentsViewDropTargetController::OnTabDragEnded() {
  ResetDropTargetTimers();
  HideDropTarget();
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
  return !urls.empty();
}

void MultiContentsViewDropTargetController::OnDragEntered(
    const ui::DropTargetEvent& event) {
  hide_drop_target_timer_.Stop();

  if (!drop_target_view_->GetVisible()) {
    return;
  }

  CHECK(drop_target_view_->state().has_value());
  if (*drop_target_view_->state() !=
      MultiContentsDropTargetView::DropTargetState::kNudge) {
    return;
  }

  drop_target_view_->Show(
      drop_target_view_->side().value(),
      MultiContentsDropTargetView::DropTargetState::kNudgeToFull,
      MultiContentsDropTargetView::DragType::kLink);
}

void MultiContentsViewDropTargetController::OnDragExited() {
  if (!drop_target_view_->GetVisible()) {
    return;
  }

  CHECK(drop_target_view_->state().has_value());
  if (*drop_target_view_->state() ==
      MultiContentsDropTargetView::DropTargetState::kFull) {
    // If the target is full expanded, then hide it immediately.
    HideDropTarget();
  } else {
    // If we are we a nudge or expanded nudge evaluate hiding the drop target
    // from a posted task. This is so we can determine if we are exiting the
    // drop target into the web content area.
    StartDropTargetHideTimer();
  }
}

void MultiContentsViewDropTargetController::OnDragDone() {
  HideDropTarget(/*suppress_animation=*/true);
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
  HideDropTarget(/*suppress_animation=*/true);
  drop_delegate_->HandleLinkDrop(side, event);
  output_drag_op = ui::mojom::DragOperation::kLink;

  if (drop_target_view_->state() !=
      MultiContentsDropTargetView::DropTargetState::kNudgeToFull) {
    return;
  }
  prefs_->SetInteger(prefs::kSplitViewDragAndDropNudgeUsedCount,
                     nudge_used_count_ + 1);
  base::RecordAction(base::UserMetricsAction("Tabs.SplitView.NudgeUsed"));
}

void MultiContentsViewDropTargetController::HandleTabDrop(
    TabDragDelegate::DragController& controller) {
  CHECK(drop_target_view_->GetVisible());
  CHECK(drop_target_view_->side().has_value());
  MultiContentsDropTargetView::DropSide side =
      drop_target_view_->side().value();
  HideDropTarget(/*suppress_animation=*/true);
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
  hide_drop_target_timer_.Stop();
  // "Drag update" events can still be delivered even if the point is out of the
  // contents area, particularly while the drop target is animating in and
  // shifting them.
  if ((point.x() < 0) || (point.x() > drop_target_parent_view_->width())) {
    ResetDropTargetTimers();
    return;
  }
  if (data.url_infos.empty() || !data.url_infos.front().url.IsStandard() ||
      is_in_split_view) {
    ResetDropTargetTimers();
    return;
  }

  if (base::FeatureList::IsEnabled(features::kSideBySideDropTargetNudge) &&
      ShouldShowNudge() && drop_target_view_->ShouldShowAnimation()) {
    HandleDragUpdateForNudge(point);
  } else {
    HandleDragUpdate(point, MultiContentsDropTargetView::DragType::kLink);
  }
}

void MultiContentsViewDropTargetController::OnWebContentsDragExit() {
  ResetDropTargetTimers();

  if (drop_target_view_->GetVisible()) {
    // Evaluate determining whether to hide the drop target on a new task
    // This is so we avoid hiding the view if we are entering the drop target.
    StartDropTargetHideTimer();
  }
}

void MultiContentsViewDropTargetController::OnWebContentsDragEnded() {
  ResetDropTargetTimers();
  HideDropTarget();
}

bool MultiContentsViewDropTargetController::IsDropTimerRunningForTesting() {
  return show_drop_target_timer_.has_value() &&
         show_drop_target_timer_->timer.IsRunning();
}

void MultiContentsViewDropTargetController::HandleDragUpdate(
    const gfx::Point& point_in_view,
    MultiContentsDropTargetView::DragType drag_type) {
  CHECK_LE(0, point_in_view.x());
  CHECK_LE(point_in_view.x(), drop_target_parent_view_->width());
  const bool is_rtl = base::i18n::IsRTL();

  const int drop_entry_point_width = MultiContentsDropTargetView::GetMaxWidth(
      drop_target_parent_view_->width(),
      MultiContentsDropTargetView::DropTargetState::kFull, drag_type);
  if (point_in_view.x() >=
      drop_target_parent_view_->width() - drop_entry_point_width) {
    StartOrUpdateDropTargetTimer(
        is_rtl ? MultiContentsDropTargetView::DropSide::START
               : MultiContentsDropTargetView::DropSide::END,
        drag_type);
    return;
  } else if (point_in_view.x() <= drop_entry_point_width) {
    StartOrUpdateDropTargetTimer(
        is_rtl ? MultiContentsDropTargetView::DropSide::END
               : MultiContentsDropTargetView::DropSide::START,
        drag_type);
    return;
  }
  ResetDropTargetTimers();
  HideDropTarget();
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
    HideDropTarget();
    show_nudge_timer_.reset();
    return;
  }

  MultiContentsDropTargetView::DropSide side;
  if (point_ratio <= nudge_ratio) {
    side = is_rtl ? MultiContentsDropTargetView::DropSide::END
                  : MultiContentsDropTargetView::DropSide::START;

  } else {
    CHECK(point_ratio >= 1.0f - nudge_ratio);
    side = is_rtl ? MultiContentsDropTargetView::DropSide::START
                  : MultiContentsDropTargetView::DropSide::END;
  }

  // Avoid transitioning to the `kNudge` state if the drop target view is
  // already visible on that side. If the timer is already running for this
  // side, don't restart the timer.
  const bool nudge_timer_running_on_same_side =
      show_nudge_timer_.has_value() && show_nudge_timer_->timer.IsRunning() &&
      show_nudge_timer_->drop_side == side;
  if (drop_target_view_->side() != side && !nudge_timer_running_on_same_side) {
    StartNudgeShowTimer(side);
  }
}

void MultiContentsViewDropTargetController::StartOrUpdateDropTargetTimer(
    MultiContentsDropTargetView::DropSide drop_side,
    MultiContentsDropTargetView::DragType drag_type) {
  if (drop_target_view_->GetVisible()) {
    return;
  }

  if (show_drop_target_timer_.has_value()) {
    CHECK(show_drop_target_timer_->timer.IsRunning());
    show_drop_target_timer_->drop_side = drop_side;
    show_drop_target_timer_->drag_type = drag_type;
    return;
  }

  show_drop_target_timer_.emplace(drop_side, drag_type);

  base::TimeDelta show_delay;
  if (drag_type == MultiContentsDropTargetView::DragType::kTab) {
    show_delay = features::kSideBySideShowDropTargetDelay.Get();
  } else if (base::Time::Now() - drop_target_last_hidden_ <
             features::kSideBySideShowDropTargetForLinkAfterHideLookbackWindow
                 .Get()) {
    // If a drop target was recently closed for a link drag, use a longer delay
    // to avoid blocking elements on the page.
    show_delay = features::kSideBySideShowDropTargetForLinkAfterHideDelay.Get();
  } else {
    show_delay = features::kSideBySideShowDropTargetForLinkDelay.Get();
  }

  show_drop_target_timer_->timer.Start(
      FROM_HERE, show_delay, this,
      &MultiContentsViewDropTargetController::ShowTimerDelayedDropTarget);
}

void MultiContentsViewDropTargetController::ResetDropTargetTimers() {
  show_drop_target_timer_.reset();
  show_nudge_timer_.reset();
}

void MultiContentsViewDropTargetController::ShowTimerDelayedDropTarget() {
  CHECK(show_drop_target_timer_.has_value());
  CHECK(!drop_target_view_->GetVisible());
  drop_target_view_->Show(show_drop_target_timer_->drop_side,
                          MultiContentsDropTargetView::DropTargetState::kFull,
                          show_drop_target_timer_->drag_type);
  show_drop_target_timer_.reset();
}

void MultiContentsViewDropTargetController::StartDropTargetHideTimer() {
  hide_drop_target_timer_.Start(
      FROM_HERE, features::kSideBySideHideDropTargetDelay.Get(),
      base::BindOnce(&MultiContentsViewDropTargetController::HideDropTarget,
                     base::Unretained(this), false));
}

void MultiContentsViewDropTargetController::HideDropTarget(
    bool suppress_animation) {
  if (drop_target_view_->GetVisible()) {
    drop_target_view_->Hide(suppress_animation);
    drop_target_last_hidden_ = base::Time::Now();
  }
}

void MultiContentsViewDropTargetController::StartNudgeShowTimer(
    MultiContentsDropTargetView::DropSide drop_side) {
  show_nudge_timer_.emplace(drop_side,
                            MultiContentsDropTargetView::DragType::kLink);
  show_nudge_timer_->timer.Start(
      FROM_HERE, features::kSideBySideShowNudgeDelay.Get(),
      base::BindOnce(
          &MultiContentsViewDropTargetController::ShowTimerDelayedNudge,
          base::Unretained(this), drop_side));
}

void MultiContentsViewDropTargetController::ShowTimerDelayedNudge(
    MultiContentsDropTargetView::DropSide drop_side) {
  if (drop_target_view_->side() != drop_side) {
    drop_target_view_->Show(
        drop_side, MultiContentsDropTargetView::DropTargetState::kNudge,
        MultiContentsDropTargetView::DragType::kLink);
    prefs_->SetInteger(prefs::kSplitViewDragAndDropNudgeShownCount,
                       nudge_shown_count_ + 1);
    base::RecordAction(base::UserMetricsAction("Tabs.SplitView.NudgeShown"));
  }
}

bool MultiContentsViewDropTargetController::PointOverlapsWithOSDropTarget(
    const gfx::Point& point_in_view) {
  if (!drop_target_parent_view_->GetWidget() ||
      !drop_target_parent_view_->GetWidget()->IsMaximized()) {
    return false;
  }

  const gfx::Point point_in_screen = views::View::ConvertPointToScreen(
      drop_target_view_->parent(), point_in_view);
  const views::Widget* top_level_widget =
      drop_target_parent_view_->GetWidget()->GetTopLevelWidget();
  const gfx::Rect screen_bounds = top_level_widget->GetWorkAreaBoundsInScreen();
  const int screen_width = screen_bounds.width();

  // On some platforms, the point may have negative values if using
  // multiple displays.
  const int drag_x_relative_to_screen_bounds =
      point_in_screen.x() - screen_bounds.x();

  const float hide_for_os_width = std::max(
      features::kSideBySideDropTargetHideForOSWidth.Get(),
      static_cast<int>(
          screen_width *
          features::kSideBySideDropTargetHideForOSPercentage.Get() / 100));

  return (drag_x_relative_to_screen_bounds < hide_for_os_width) ||
         (drag_x_relative_to_screen_bounds > screen_width - hide_for_os_width);
}

void MultiContentsViewDropTargetController::
    OnDragAndDropNudgeShownCountChange() {
  nudge_shown_count_ =
      prefs_->GetInteger(prefs::kSplitViewDragAndDropNudgeShownCount);
}

void MultiContentsViewDropTargetController::
    OnDragAndDropNudgeUsedCountChange() {
  nudge_used_count_ =
      prefs_->GetInteger(prefs::kSplitViewDragAndDropNudgeUsedCount);
}

bool MultiContentsViewDropTargetController::ShouldShowNudge() {
  return nudge_shown_count_ <
             features::kSideBySideDropTargetNudgeShownLimit.Get() &&
         nudge_used_count_ <
             features::kSideBySideDropTargetNudgeUsedLimit.Get();
}

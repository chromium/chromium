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
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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

namespace {
static constexpr base::TimeDelta kShowDropTargetForLinkAfterHideLookbackWindow =
    base::Seconds(30);
static constexpr base::TimeDelta kHideDropTargetDelay = base::Milliseconds(100);
static constexpr base::TimeDelta kShowNudgeDelay = base::Milliseconds(1000);

// Returns a value between `min_value` and `max_value`, scaled linearly
// according to `current_size`.
int GetValueScaledToSize(int current_size, int min_value, int max_value) {
  static const float kMinSize = MultiContentsDropTargetView::kDropTargetMinSize;
  static const float kMaxSize = MultiContentsDropTargetView::kDropTargetMaxSize;

  // Scale linearly between min and max delay based on the size.
  const float size_ratio =
      std::clamp((current_size - kMinSize) / (kMaxSize - kMinSize), 0.0f, 1.0f);
  return min_value + (size_ratio * (max_value - min_value));
}

// Returns whether the point is within the bounds of the view, but not within
// the bottom |reserved_height_for_scrolling| dips.
bool IsPointEligibleForDrag(gfx::Point point_in_view,
                            const raw_ref<views::View> view,
                            int reserved_height_for_scrolling = 0) {
  gfx::Rect eligible_bounds = view->GetLocalBounds();
  eligible_bounds.Inset(
      gfx::Insets().set_bottom(reserved_height_for_scrolling));
  return eligible_bounds.Contains(gfx::Rect(point_in_view, gfx::Size()));
}

}  // namespace

// static
int MultiContentsViewDropTargetController::DropTargetConstants::GetHideWidth() {
  return
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
      32;
#elif BUILDFLAG(IS_LINUX)
      50;
#else
      0;
#endif
}

// static
double MultiContentsViewDropTargetController::DropTargetConstants::
    GetHidePercentage() {
  return
#if BUILDFLAG(IS_WIN)
      1.4;
#else
      0;
#endif
}

MultiContentsViewDropTargetController::MultiContentsViewDropTargetController(
    MultiContentsDropTargetView& drop_target_view,
    DropDelegate& drop_delegate,
    PrefService* prefs,
    TabStripModel* tab_strip_model)
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

  if (tab_strip_model) {
    tab_strip_model_observer_.Observe(tab_strip_model);
  }
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

TabDragContext* MultiContentsViewDropTargetController::OnTabDragUpdated(
    TabDragTarget::DragController& controller,
    const gfx::Point& point_in_screen) {
  const auto& drag_data = controller.GetSessionData();
  // Only allow creating split with a single dragged tab, that is not a tab
  // group drag (i.e. the tab should not be the only member of its group).
  // Allowing a group to turn into a split would circumvent the group deletion
  // flow, such as requesting user confirmation.
  const bool dragging_single_ungrouped_tab =
      drag_data.num_dragging_tabs() == 1 && drag_data.dragging_groups.empty();

  const gfx::Point point_in_parent = views::View::ConvertPointFromScreen(
      &drop_target_parent_view_.get(), point_in_screen);
  if (!dragging_single_ungrouped_tab ||
      PointOverlapsWithOSDropTarget(point_in_screen) ||
      !IsPointEligibleForDrag(point_in_parent, drop_target_parent_view_)) {
    ResetDropTargetTimers();
    HideDropTarget();
    return nullptr;
  }
  HandleDragUpdate(point_in_parent,
                   MultiContentsDropTargetView::DragType::kTab);
  return nullptr;
}

void MultiContentsViewDropTargetController::OnTabDragEntered() {}

void MultiContentsViewDropTargetController::OnTabDragExited(
    const gfx::Point& point_in_screen) {
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

void MultiContentsViewDropTargetController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    ResetDropTargetTimers();
    HideDropTarget();
  }
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
    TabDragTarget::DragController& controller) {
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
  if (!IsPointEligibleForDrag(point, drop_target_parent_view_,
                              kReservedHeightForScrollingDown)) {
    ResetDropTargetTimers();
    return;
  }
  if (data.url_infos.empty() || !data.url_infos.front().url.IsStandard() ||
      is_in_split_view) {
    ResetDropTargetTimers();
    return;
  }

  HandleDragUpdate(point, MultiContentsDropTargetView::DragType::kLink);
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
  CHECK(IsPointEligibleForDrag(point_in_view, drop_target_parent_view_));
  const bool is_rtl = base::i18n::IsRTL();

  bool can_show_nudge =
      drag_type == MultiContentsDropTargetView::DragType::kLink &&
      ShouldShowNudge() && drop_target_view_->ShouldShowAnimation();

  const int drop_entry_point_width =
      can_show_nudge
          ? drop_target_parent_view_->width() * kNudgeShowRatio
          : MultiContentsDropTargetView::GetMaxSize(
                drop_target_parent_view_->width(),
                MultiContentsDropTargetView::DropTargetState::kFull, drag_type);
  const int drop_entry_point_height = MultiContentsDropTargetView::GetMaxSize(
      drop_target_parent_view_->height(),
      MultiContentsDropTargetView::DropTargetState::kFull, drag_type);

  MultiContentsDropTargetView::DropSide drop_side;
  if (point_in_view.x() <= drop_entry_point_width) {
    drop_side = is_rtl ? MultiContentsDropTargetView::DropSide::END
                       : MultiContentsDropTargetView::DropSide::START;
  } else if (point_in_view.x() >=
             drop_target_parent_view_->width() - drop_entry_point_width) {
    drop_side = is_rtl ? MultiContentsDropTargetView::DropSide::START
                       : MultiContentsDropTargetView::DropSide::END;
  } else if (base::FeatureList::IsEnabled(tabs::kSplitViewHorizontal) &&
             point_in_view.y() >=
                 drop_target_parent_view_->height() - drop_entry_point_height) {
    drop_side = MultiContentsDropTargetView::DropSide::BOTTOM;
  } else {
    ResetDropTargetTimers();
    HideDropTarget();
    return;
  }

  // If we are showing the nudge on the wrong side, hide the drop target.
  if (can_show_nudge && drop_target_view_->GetVisible() &&
      drop_target_view_->side() != drop_side) {
    HideDropTarget();
  }

  if (can_show_nudge &&
      drop_side != MultiContentsDropTargetView::DropSide::BOTTOM) {
    // Avoid transitioning to the `kNudge` state if the drop target view is
    // already visible on that side. If the timer is already running for this
    // side, don't restart the timer.
    const bool nudge_timer_running_on_same_side =
        show_nudge_timer_.has_value() && show_nudge_timer_->timer.IsRunning() &&
        show_nudge_timer_->drop_side == drop_side;
    if (drop_target_view_->side() != drop_side &&
        !nudge_timer_running_on_same_side) {
      StartNudgeShowTimer(drop_side);
    }
  } else {
    StartOrUpdateDropTargetTimer(
        point_in_view,
        drop_side == MultiContentsDropTargetView::DropSide::BOTTOM
            ? drop_entry_point_height
            : drop_entry_point_width,
        drop_side, drag_type);
  }
}

void MultiContentsViewDropTargetController::StartOrUpdateDropTargetTimer(
    const gfx::Point& point_in_view,
    int drop_entry_point_size,
    MultiContentsDropTargetView::DropSide drop_side,
    MultiContentsDropTargetView::DragType drag_type) {
  if (drop_target_view_->GetVisible() && !drop_target_view_->IsClosing()) {
    return;
  }

  if (show_drop_target_timer_.has_value()) {
    CHECK(show_drop_target_timer_->timer.IsRunning());
    show_drop_target_timer_->drop_side = drop_side;
    show_drop_target_timer_->drag_type = drag_type;

    if (base::FeatureList::IsEnabled(features::kSplitViewDragAndDropVelocity)) {
      // Restart the timer if the latest point in the view exceeded the
      // movement threshold.
      const int min_distance =
          features::kSplitViewDragAndDropMinDistanceThreshold.Get();
      const int max_distance =
          features::kSplitViewDragAndDropMaxDistanceThreshold.Get();
      const int distance_threshold = GetValueScaledToSize(
          drop_entry_point_size, min_distance, max_distance);
      const float distance =
          (point_in_view - drag_point_at_timer_start_).Length();
      if (distance > distance_threshold) {
        show_drop_target_timer_->timer.Reset();
        drag_point_at_timer_start_ = point_in_view;
      }
    }
    return;
  }

  show_drop_target_timer_.emplace(drop_side, drag_type);
  drag_point_at_timer_start_ = point_in_view;

  base::TimeDelta show_delay;
  if (base::FeatureList::IsEnabled(features::kSplitViewDragAndDropVelocity)) {
    const base::TimeDelta min_delay =
        features::kSplitViewDragAndDropMinDelay.Get();
    const base::TimeDelta max_delay =
        features::kSplitViewDragAndDropMaxDelay.Get();
    show_delay = base::Milliseconds(
        GetValueScaledToSize(drop_entry_point_size, min_delay.InMilliseconds(),
                             max_delay.InMilliseconds()));
  } else if (drag_type == MultiContentsDropTargetView::DragType::kTab) {
    show_delay = features::kShowDropTargetForTabDelay.Get();
  } else if (base::Time::Now() - drop_target_last_hidden_ <
             kShowDropTargetForLinkAfterHideLookbackWindow) {
    // If a drop target was recently closed for a link drag, use a longer delay
    // to avoid blocking elements on the page.
    show_delay = kShowDropTargetForLinkAfterHideDelay;
  } else {
    show_delay = kShowDropTargetForLinkDelay;
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
      FROM_HERE, kHideDropTargetDelay,
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
      FROM_HERE, kShowNudgeDelay,
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
    const gfx::Point& point_in_screen) {
  if (!drop_target_parent_view_->GetWidget() ||
      !drop_target_parent_view_->GetWidget()->IsMaximized()) {
    return false;
  }

  const views::Widget* top_level_widget =
      drop_target_parent_view_->GetWidget()->GetTopLevelWidget();
  const gfx::Rect screen_bounds = top_level_widget->GetWorkAreaBoundsInScreen();
  const int screen_width = screen_bounds.width();

  // On some platforms, the point may have negative values if using
  // multiple displays.
  const int drag_x_relative_to_screen_bounds =
      point_in_screen.x() - screen_bounds.x();

  const float hide_for_os_width = std::max(
      DropTargetConstants::GetHideWidth(),
      static_cast<int>(screen_width * DropTargetConstants::GetHidePercentage() /
                       100));

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
  bool showing_nudge = drop_target_view_->GetVisible() &&
                       !drop_target_view_->IsClosing() &&
                       drop_target_view_->state() ==
                           MultiContentsDropTargetView::DropTargetState::kNudge;
  return (nudge_shown_count_ < kNudgeShownLimit || showing_nudge) &&
         nudge_used_count_ < kNudgeUsedLimit;
}

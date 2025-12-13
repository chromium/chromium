// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_DROP_TARGET_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_DROP_TARGET_CONTROLLER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view.h"

namespace content {
struct DropData;
}  // namespace content

class PrefService;

// `MultiContentsViewDropTargetController` is responsible for handling
// the drag-entrypoint of a single `MultiContentsView`. This includes dragging
// links,  bookmarks, or tab headers to create a split view.
// There exists one `MultiContentsViewDropTargetController` per
// `MultiContentesView`.
class MultiContentsViewDropTargetController final
    : public TabDragDelegate,
      public MultiContentsDropTargetView::DragDelegate {
 public:
  // Delegate for handling the drop callback.
  class DropDelegate {
   public:
    virtual ~DropDelegate() = default;

    // Handles links that are dropped on the view.
    virtual void HandleLinkDrop(MultiContentsDropTargetView::DropSide side,
                                const ui::DropTargetEvent& event) = 0;

    // Handles tabs that are dropped on the view.
    virtual void HandleTabDrop(MultiContentsDropTargetView::DropSide side,
                               TabDragDelegate::DragController& controller) = 0;
  };

  MultiContentsViewDropTargetController(
      MultiContentsDropTargetView& drop_target_view,
      DropDelegate& drop_delegate,
      PrefService* prefs);
  ~MultiContentsViewDropTargetController() override;
  MultiContentsViewDropTargetController(
      const MultiContentsViewDropTargetController&) = delete;
  MultiContentsViewDropTargetController& operator=(
      const MultiContentsViewDropTargetController&) = delete;

  // TabDragDelegate
  void OnTabDragUpdated(TabDragDelegate::DragController& controller,
                        const gfx::Point& point_in_screen) override;
  void OnTabDragEntered() override;
  void OnTabDragExited() override;
  void OnTabDragEnded() override;
  bool CanDropTab() override;
  void HandleTabDrop(TabDragDelegate::DragController& controller) override;
  base::CallbackListSubscription RegisterWillDestroyCallback(
      base::OnceClosure callback) override;

  // Handles a drag within the web contents area.
  // `point` should be relative to the multi contents view.
  void OnWebContentsDragUpdate(const content::DropData& data,
                               const gfx::Point& point,
                               bool is_in_split_view);
  void OnWebContentsDragExit();
  void OnWebContentsDragEnded();

  // MultiContentsDropTargetView::DragDelegate:
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragExited() override;
  void OnDragDone() override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  views::View::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;

  bool IsDropTimerRunningForTesting();

 private:
  void DoDrop(const ui::DropTargetEvent& event,
              ui::mojom::DragOperation& output_drag_op,
              std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  // Represents a timer for delaying when a specific drop target view is shown.
  struct DropTargetShowTimer {
    DropTargetShowTimer(MultiContentsDropTargetView::DropSide drop_side,
                        MultiContentsDropTargetView::DragType drag_type);
    base::OneShotTimer timer;
    MultiContentsDropTargetView::DropSide drop_side;
    MultiContentsDropTargetView::DragType drag_type;
  };

  // Updates the timers for a drag at the given point.
  // Assumes the dragged data is droppable (e.g. tab or link).
  void HandleDragUpdate(const gfx::Point& point_in_view,
                        MultiContentsDropTargetView::DragType drag_type);
  void HandleDragUpdateForNudge(const gfx::Point& point_in_view);

  // Starts or updates a running timer to show `target_to_show`.
  void StartOrUpdateDropTargetTimer(
      MultiContentsDropTargetView::DropSide drop_side,
      MultiContentsDropTargetView::DragType drag_type);
  void ResetDropTargetTimers();

  // Shows the drop target that should be displayed at the end of the delay.
  void ShowTimerDelayedDropTarget();

  // Timer to hide the drop target if the drag isn't over web contents or
  // drop target.
  void StartDropTargetHideTimer();

  // Hide the drop target view.
  void HideDropTarget(bool suppress_animation = false);

  // Timer to show the nudge after a small delay.
  void StartNudgeShowTimer(MultiContentsDropTargetView::DropSide drop_side);

  // Actually show the drop target nudge.
  void ShowTimerDelayedNudge(MultiContentsDropTargetView::DropSide drop_side);

  // Used to determine if the drop target should be hidden because the OS drop
  // target would be visible. Estimation based on when OS drop targets typically
  // show. Only returns true if the browser is maximized.
  bool PointOverlapsWithOSDropTarget(const gfx::Point& point_in_view);

  // Keeps the value of nudge_shown_count_ in sync with the pref.
  void OnDragAndDropNudgeShownCountChange();

  // Keeps the value of nudge_used_count_ in sync with the pref.
  void OnDragAndDropNudgeUsedCountChange();

  // Whether the nudge should be shown, based on the number of times it has been
  // shown/used in the past.
  bool ShouldShowNudge();

  // This timer is used for showing the drop target a delay, and may be
  // canceled in case a drag exits the drop area before the target is shown.
  std::optional<DropTargetShowTimer> show_drop_target_timer_ = std::nullopt;

  base::OneShotTimer hide_drop_target_timer_;

  std::optional<DropTargetShowTimer> show_nudge_timer_ = std::nullopt;

  // Stores the most recent time the drop target was hidden. Used to calculate
  // the show timer for link drags.
  base::Time drop_target_last_hidden_;

  // The view that is displayed when drags hover over the "drop" region of
  // the content area.
  const raw_ref<MultiContentsDropTargetView> drop_target_view_;
  const raw_ref<views::View> drop_target_parent_view_;
  const raw_ref<DropDelegate> drop_delegate_;

  base::OnceClosureList on_will_destroy_callback_list_;

  // Used to read/write the nudge count pref.
  raw_ptr<PrefService> prefs_;
  PrefChangeRegistrar pref_change_registrar_;
  // Tracks the value of prefs::kSplitViewDragAndDropNudgeShownCount.
  int nudge_shown_count_;
  // Tracks the value of prefs::kSplitViewDragAndDropNudgeUsedCount.
  int nudge_used_count_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_DROP_TARGET_CONTROLLER_H_

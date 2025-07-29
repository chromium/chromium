// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_DROP_TARGET_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_DROP_TARGET_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view.h"

namespace content {
struct DropData;
}  // namespace content

// `MultiContentsViewDropTargetController` is responsible for handling
// the drag-entrypoint of a single `MultiContentsView`. This includes dragging
// links,  bookmarks, or tab headers to create a split view.
// There exists one `MultiContentsViewDropTargetController` per
// `MultiContentesView`.
class MultiContentsViewDropTargetController final : public TabDragDelegate {
 public:
  explicit MultiContentsViewDropTargetController(
      MultiContentsDropTargetView& drop_target_view);
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
                               const gfx::PointF& point,
                               bool is_in_split_view);
  void OnWebContentsDragExit();
  void OnWebContentsDragEnded();

 private:
  // Represents a timer for delaying when a specific drop target view is shown.
  struct DropTargetShowTimer {
    explicit DropTargetShowTimer(
        MultiContentsDropTargetView::DropSide drop_side);
    base::OneShotTimer timer;
    MultiContentsDropTargetView::DropSide drop_side;
  };

  // Updates the timers for a drag at the given point.
  // Assumes the dragged data is droppable (e.g. tab or link).
  void HandleDragUpdate(const gfx::PointF& point_in_view);

  // Starts or updates a running timer to show `target_to_show`.
  void StartOrUpdateDropTargetTimer(
      MultiContentsDropTargetView::DropSide drop_side);
  void ResetDropTargetTimer();

  // Shows the drop target that should be displayed at the end of the delay.
  void ShowTimerDelayedDropTarget();

  // This timer is used for showing the drop target a delay, and may be
  // canceled in case a drag exits the drop area before the target is shown.
  std::optional<DropTargetShowTimer> show_drop_target_timer_ = std::nullopt;

  // The view that is displayed when drags hover over the "drop" region of
  // the content area.
  const raw_ref<MultiContentsDropTargetView> drop_target_view_;
  const raw_ref<views::View> drop_target_parent_view_;

  base::OnceClosureList on_will_destroy_callback_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_DROP_TARGET_CONTROLLER_H_

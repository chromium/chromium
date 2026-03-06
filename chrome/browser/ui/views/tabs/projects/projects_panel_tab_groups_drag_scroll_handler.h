// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_DRAG_SCROLL_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_DRAG_SCROLL_HANDLER_H_

#include "base/callback_list.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/point.h"

namespace views {
class ScrollView;
}  // namespace views

// This handler is responsible for managing scrolling as tab groups are dragged
// near the edge of their scroll view.
class ProjectsPanelTabGroupsDragScrollHandler {
 public:
  ProjectsPanelTabGroupsDragScrollHandler() = default;
  ~ProjectsPanelTabGroupsDragScrollHandler() = default;
  ProjectsPanelTabGroupsDragScrollHandler(
      const ProjectsPanelTabGroupsDragScrollHandler& other) = delete;
  ProjectsPanelTabGroupsDragScrollHandler& operator=(
      const ProjectsPanelTabGroupsDragScrollHandler&) = delete;

  // Updates scrolling according to the new drag bounds. This may either stop
  // or start scrolling, or update the rate.
  void OnDraggedTabGroupPositionUpdated(
      views::ScrollView& scroll_view,
      const gfx::Point& drag_location_in_scroll_view);

  // Stops scrolling, or does nothing if scrolling is not in progress.
  void StopScrolling();

 private:
  void StartOrContinueScrolling(views::ScrollView& scroll_view,
                                float vertical_increments);
  void UpdateScrollOffset(views::ScrollView& scroll_view);

  // Timer used to update scroll offsets at a constant rate.
  base::RepeatingTimer scroll_timer_;

  // The offset to apply on each scroll event. Negative values scroll upwards.
  float vertical_scroll_increment_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_DRAG_SCROLL_HANDLER_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_DRAGGING_TABS_SESSION_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_DRAGGING_TABS_SESSION_H_

#include "chrome/browser/ui/views/tabs/dragging/drag_session_data.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_strip_scroll_session.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

// Handles dragging tabs within a single TabDragContext on behalf of
// TabDragController.
class DraggingTabsSession final : public TabDragWithScrollManager {
 public:
  // `drag_data` is a copy of the drag configuration for the full session.
  // `attached_context` is the context in which the tabs are being dragged.
  // `offset_to_width_ratio_` is the desired x offset into the source
  // TabSlotView (e.g. 0.5 if the drag started in the center of the tab).
  // `initial_move` should be true if the drag session is beginning, and
  // the tabs have not been moved from their initial positions.
  // `point_in_screen` is the initial cursor screen position.
  explicit DraggingTabsSession(DragSessionData drag_data,
                               TabDragContext* attached_context,
                               float offset_to_width_ratio_,
                               bool initial_move,
                               gfx::Point point_in_screen);
  ~DraggingTabsSession() final;

  // TabDragWithScrollManager:
  void MoveAttached(gfx::Point point_in_screen) override;
  gfx::Rect GetEnclosingRectForDraggedTabs() override;
  gfx::Point GetLastPointInScreen() override;
  views::View* GetAttachedContext() override;
  views::ScrollView* GetScrollView() override;

 private:
  void MoveAttachedImpl(gfx::Point point_in_screen, bool just_attached);

  // Retrieves the bounds of the dragged tabs relative to the attached
  // TabDragContext. `tab_strip_point` is in the attached
  // TabDragContext's coordinate system.
  gfx::Rect GetDraggedViewTabStripBounds(gfx::Point tab_strip_point) const;

  // Returns true if the tabs were originally one after the other in
  // `source_context_`.
  bool AreTabsConsecutive() const;

  // Helper method for TabDragController::MoveAttached to precompute the tab
  // group membership of selected tabs after performing the move.
  std::optional<tab_groups::TabGroupId> CalculateGroupForDraggedTabs(
      int to_index);

  // Calculates where to position the dragged tabs, given the cursor is at
  // `point_in_screen`. Keeps the same point within the source view under the
  // cursor, unless that would push tabs outside the drag area.
  gfx::Point GetAttachedDragPoint(gfx::Point point_in_screen);

  // Data about the tabs and groups being dragged.
  const DragSessionData drag_data_;
  // The context in which `this` will drag tabs.
  const raw_ptr<TabDragContext> attached_context_;
  // This is the horizontal offset of the mouse from the leading edge of the
  // first tab where dragging began. This is used to ensure that the dragged
  // tabs are always positioned at the correct location during the drag.
  const int mouse_offset_;

  // True if MoveAttached() has never been called, and `attached_context_` is
  // the source context in which the drag originated.
  bool initial_move_;
  // The horizontal position of the mouse cursor in `attached_context_`
  // coordinates at the time of the last re-order event.
  int last_move_attached_context_loc_;

  gfx::Point last_point_in_screen_;

  // the scrolling session that handles scrolling when the tabs are dragged
  // to the scrollable regions of the tab_strip.
  std::unique_ptr<TabStripScrollSession> tab_strip_scroll_session_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_DRAGGING_TABS_SESSION_H_

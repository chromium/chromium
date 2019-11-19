// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTEXT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTEXT_H_

#include <vector>

#include "base/optional.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/gfx/geometry/rect.h"

class Tab;
class TabGroupHeader;
class TabSlotView;
class TabStrip;
class TabStripModel;
class TabDragController;

namespace views {
class View;
}

// Provides tabstrip functionality specifically for TabDragController, much of
// which should not otherwise be in TabStrip's public interface.
class TabDragContext {
 public:
  virtual ~TabDragContext() = default;

  virtual views::View* AsView() = 0;
  virtual const views::View* AsView() const = 0;
  virtual Tab* GetTabAt(int index) const = 0;
  virtual int GetIndexOf(const TabSlotView* view) const = 0;
  virtual int GetTabCount() const = 0;
  virtual bool IsTabPinned(const Tab* tab) const = 0;
  virtual int GetPinnedTabCount() const = 0;
  virtual TabGroupHeader* GetTabGroupHeader(TabGroupId group) const = 0;
  virtual TabStripModel* GetTabStripModel() = 0;

  // Returns the index of the active tab in touch mode, or no value if not in
  // touch mode.
  virtual base::Optional<int> GetActiveTouchIndex() const = 0;

  // Returns the tab drag controller owned by this delegate, or null if none.
  virtual TabDragController* GetDragController() = 0;

  // Takes ownership of |controller|.
  virtual void OwnDragController(TabDragController* controller) = 0;

  // Releases ownership of the current TabDragController.
  virtual TabDragController* ReleaseDragController() = 0;

  // Destroys the current TabDragController. This cancel the existing drag
  // operation.
  virtual void DestroyDragController() = 0;

  // Returns true if a drag session is currently active.
  virtual bool IsDragSessionActive() const = 0;

  // Returns true if a tab is being dragged into this tab strip.
  virtual bool IsActiveDropTarget() const = 0;

  // Returns the x-coordinates of the tabs.
  virtual std::vector<int> GetTabXCoordinates() const = 0;

  // Returns the width of the active tab.
  virtual int GetActiveTabWidth() const = 0;

  // Returns the width of the area that contains tabs. This does not include
  // the width of the new tab button.
  virtual int GetTabAreaWidth() const = 0;

  // Returns where the drag region ends; tabs dragged past this should detach.
  virtual int TabDragAreaEndX() const = 0;

  // Returns the horizontal drag threshold - the amount a tab drag must move to
  // trigger a reorder. This is dependent on the width of tabs. The smaller the
  // tabs compared to the standard size, the smaller the threshold.
  virtual int GetHorizontalDragThreshold() const = 0;

  // Returns the index where the dragged WebContents should be inserted into
  // this tabstrip given the DraggedTabView's bounds |dragged_bounds| in
  // coordinates relative to |attached_tabstrip_| and has had the mirroring
  // transformation applied.
  // |mouse_has_ever_moved_left| and |mouse_has_ever_moved_right| are used
  // only in stacked tabs cases.
  // |group| is set if the drag is originating from a group header, in which
  // case the entire group is dragged and should not be dropped into other
  // groups.
  // NOTE: this is invoked from Attach() before the tabs have been inserted.
  virtual int GetInsertionIndexForDraggedBounds(
      const gfx::Rect& dragged_bounds,
      bool attaching,
      int num_dragged_tabs,
      bool mouse_has_ever_moved_left,
      bool mouse_has_ever_moved_right,
      base::Optional<TabGroupId> group) const = 0;

  // Returns true if |dragged_bounds| is close enough to the next stacked tab
  // so that the active tab should be dragged there.
  virtual bool ShouldDragToNextStackedTab(
      const gfx::Rect& dragged_bounds,
      int index,
      bool mouse_has_ever_moved_right) const = 0;

  // Returns true if |dragged_bounds| is close enough to the previous stacked
  // tab so that the active tab should be dragged there.
  virtual bool ShouldDragToPreviousStackedTab(
      const gfx::Rect& dragged_bounds,
      int index,
      bool mouse_has_ever_moved_left) const = 0;

  // Drags the active tab by |delta|. |initial_positions| is the x-coordinates
  // of the tabs when the drag started.  This is only called when
  // |touch_layout_| is non-null.
  virtual void DragActiveTabStacked(const std::vector<int>& initial_positions,
                                    int delta) = 0;

  // Returns the bounds needed for each of the views, relative to a leading
  // coordinate of 0 for the left edge of the first view's bounds.
  virtual std::vector<gfx::Rect> CalculateBoundsForDraggedViews(
      const std::vector<TabSlotView*>& views) = 0;

  // Sets the bounds of |views| to |bounds|.
  virtual void SetBoundsForDrag(const std::vector<TabSlotView*>& views,
                                const std::vector<gfx::Rect>& bounds) = 0;

  // Used by TabDragController when the user starts or stops dragging.
  virtual void StartedDragging(const std::vector<TabSlotView*>& views) = 0;

  // Invoked when TabDragController detaches a set of tabs.
  virtual void DraggedTabsDetached() = 0;

  // Used by TabDragController when the user stops dragging. |move_only| is
  // true if the move behavior is TabDragController::MOVE_VISIBLE_TABS.
  // |completed| is true if the drag operation completed successfully, false if
  // it was reverted.
  virtual void StoppedDragging(const std::vector<TabSlotView*>& views,
                               const std::vector<int>& initial_positions,
                               bool move_only,
                               bool completed) = 0;

  // Invoked during drag to layout the views being dragged in |views| at
  // |location|. If |initial_drag| is true, this is the initial layout after the
  // user moved the mouse far enough to trigger a drag.
  virtual void LayoutDraggedViewsAt(const std::vector<TabSlotView*>& views,
                                    TabSlotView* source_view,
                                    const gfx::Point& location,
                                    bool initial_drag) = 0;

  // Forces the entire tabstrip to lay out.
  virtual void ForceLayout() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTEXT_H_

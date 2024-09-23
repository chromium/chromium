// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTEXT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTEXT_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

class Tab;
class TabGroupHeader;
class TabSlotView;
class TabStrip;
class TabStripModel;
class TabDragController;

namespace tab_groups {
class TabGroupId;
}

// A limited subset of TabDragContext for use by non-TabDragController clients.
class TabDragContextBase : public views::View {
  METADATA_HEADER(TabDragContextBase, views::View)

 public:
  ~TabDragContextBase() override = default;

  // Called when the TabStrip is changed during a drag session.
  virtual void UpdateAnimationTarget(TabSlotView* tab_slot_view,
                                     const gfx::Rect& target_bounds) = 0;

  // Returns true if a drag session is currently active.
  virtual bool IsDragSessionActive() const = 0;

  // Returns true if this DragContext is in the process of returning tabs to the
  // associated TabContainer.
  virtual bool IsAnimatingDragEnd() const = 0;

  // Immediately completes any ongoing end drag animations, returning the tabs
  // to the associated TabContainer immediately.
  virtual void CompleteEndDragAnimations() = 0;

  // Returns the width of the region in which dragged tabs are allowed to exist.
  virtual int GetTabDragAreaWidth() const = 0;
};

// Provides tabstrip functionality specifically for TabDragController, much of
// which should not otherwise be in TabStrip's public interface.
class TabDragContext : public TabDragContextBase {
  METADATA_HEADER(TabDragContext, TabDragContextBase)

 public:
  ~TabDragContext() override = default;

  virtual Tab* GetTabAt(int index) const = 0;
  virtual std::optional<int> GetIndexOf(const TabSlotView* view) const = 0;
  virtual int GetTabCount() const = 0;
  virtual bool IsTabPinned(const Tab* tab) const = 0;
  virtual int GetPinnedTabCount() const = 0;
  virtual TabGroupHeader* GetTabGroupHeader(
      const tab_groups::TabGroupId& group) const = 0;
  virtual TabStripModel* GetTabStripModel() = 0;

  // Returns the tab drag controller owned by this delegate, or null if none.
  virtual TabDragController* GetDragController() = 0;

  // Takes ownership of |controller|.
  virtual void OwnDragController(
      std::unique_ptr<TabDragController> controller) = 0;

  virtual views::ScrollView* GetScrollView() = 0;

  // Releases ownership of the current TabDragController.
  [[nodiscard]] virtual std::unique_ptr<TabDragController>
  ReleaseDragController() = 0;

  // Set a callback to be called with the controller upon assignment by
  // OwnDragController(controller). Allows tests to get the TabDragController
  // instance as soon as its assigned.
  virtual void SetDragControllerCallbackForTesting(
      base::OnceCallback<void(TabDragController*)> callback) = 0;

  // Destroys the current TabDragController. This cancel the existing drag
  // operation.
  virtual void DestroyDragController() = 0;

  // Returns true if a tab is being dragged into this tab strip.
  virtual bool IsActiveDropTarget() const = 0;

  // Returns the width of the active tab.
  virtual int GetActiveTabWidth() const = 0;

  // Returns where the drag region begins and ends; tabs dragged beyond these
  // points should detach.
  virtual int TabDragAreaEndX() const = 0;
  virtual int TabDragAreaBeginX() const = 0;

  // Returns the horizontal drag threshold - the amount a tab drag must move to
  // trigger a reorder. This is dependent on the width of tabs. The smaller the
  // tabs compared to the standard size, the smaller the threshold.
  virtual int GetHorizontalDragThreshold() const = 0;

  // Returns the index where the dragged WebContents should be inserted into
  // this tabstrip given the DraggedTabView's bounds |dragged_bounds| in
  // coordinates relative to |attached_tabstrip_| and has had the mirroring
  // transformation applied.
  // |dragged_views| are the view children of |attached_tabstrip_| that are
  // part of the drag.
  // |group| is set if the drag is originating from a group header, in which
  // case the entire group is dragged and should not be dropped into other
  // groups.
  virtual int GetInsertionIndexForDraggedBounds(
      const gfx::Rect& dragged_bounds,
      std::vector<raw_ptr<TabSlotView, VectorExperimental>> dragged_views,
      int num_dragged_tabs,
      std::optional<tab_groups::TabGroupId> group) const = 0;

  // Returns the bounds needed for each of the views, relative to a leading
  // coordinate of 0 for the left edge of the first view's bounds.
  virtual std::vector<gfx::Rect> CalculateBoundsForDraggedViews(
      const std::vector<raw_ptr<TabSlotView, VectorExperimental>>& views) = 0;

  // Sets the bounds of |views| to |bounds|.
  virtual void SetBoundsForDrag(
      const std::vector<raw_ptr<TabSlotView, VectorExperimental>>& views,
      const std::vector<gfx::Rect>& bounds) = 0;

  // Used by TabDragController when the user starts or stops dragging.
  virtual void StartedDragging(
      const std::vector<raw_ptr<TabSlotView, VectorExperimental>>& views) = 0;

  // Invoked when TabDragController detaches a set of tabs.
  virtual void DraggedTabsDetached() = 0;

  // Used by TabDragController when the user stops dragging. |completed| is
  // true if the drag operation completed successfully, false if it was
  // reverted.
  virtual void StoppedDragging() = 0;

  // Invoked during drag to layout the views being dragged in |views| at
  // |location|. If |initial_drag| is true, this is the initial layout after the
  // user moved the mouse far enough to trigger a drag.
  virtual void LayoutDraggedViewsAt(
      const std::vector<raw_ptr<TabSlotView, VectorExperimental>>& views,
      TabSlotView* source_view,
      const gfx::Point& location,
      bool initial_drag) = 0;

  // Forces the entire tabstrip to lay out.
  virtual void ForceLayout() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTEXT_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_CONTROLLER_H_

#include <optional>

#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace tab_groups {
class TabGroupId;
}  // namespace tab_groups

namespace views {
class View;
}

class TabSlotView;

// Model/Controller for the TabContainer.
// NOTE: All indices used by this class are in model coordinates.
class TabContainerController {
 public:
  virtual ~TabContainerController() = default;

  // Returns true if |index| is a valid model index.
  virtual bool IsValidModelIndex(int index) const = 0;

  // Returns the index of the active tab.
  virtual std::optional<int> GetActiveIndex() const = 0;

  // Returns the number of pinned tabs in the model. Note that this can be
  // different from the number of pinned tabs in the TabStrip view (and its
  // associated classes) when a tab is being opened, closed, pinned or unpinned.
  virtual int NumPinnedTabsInModel() const = 0;

  // Notifies controller of a drop index update.
  virtual void OnDropIndexUpdate(std::optional<int> index,
                                 bool drop_before) = 0;

  // Returns the |group| collapsed state. Returns false if the group does not
  // exist or is not collapsed.
  // NOTE: This method signature is duplicated in TabStripController; the
  // methods are intended to have equivalent semantics so they can share an
  // implementation.
  virtual bool IsGroupCollapsed(const tab_groups::TabGroupId& group) const = 0;

  // Gets the first tab index in |group|, or nullopt if the group is
  // currently empty. This is always safe to call unlike
  // ListTabsInGroup().
  virtual std::optional<int> GetFirstTabInGroup(
      const tab_groups::TabGroupId& group) const = 0;

  // Returns the range of tabs in the given |group|. This must not be
  // called during intermediate states where the group is not
  // contiguous. For example, if tabs elsewhere in the tab strip are
  // being moved into |group| it may not be contiguous; this method
  // cannot be called.
  virtual gfx::Range ListTabsInGroup(
      const tab_groups::TabGroupId& group) const = 0;

  // Whether the window drag handle area can be extended to include the top of
  // inactive tabs.
  virtual bool CanExtendDragHandle() const = 0;

  // Tab closing mode should remain active as long as the mouse is in or near
  // this view. See `TabContainerImpl::in_tab_close_` for more details on tab
  // closing mode.
  virtual const views::View* GetTabClosingModeMouseWatcherHostView() const = 0;

  // Returns true if any tabs are being animated, anywhere in the TabStrip.
  virtual bool IsAnimatingInTabStrip() const = 0;

  // Retargets the animation of `tab_slot_view` to
  // `target_bounds_in_tab_container_coords`, without disrupting its timing.
  virtual void UpdateAnimationTarget(
      TabSlotView* tab_slot_view,
      gfx::Rect target_bounds_in_tab_container_coords) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_CONTROLLER_H_

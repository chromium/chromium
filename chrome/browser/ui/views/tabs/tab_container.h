// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_

#include <memory>
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/z_orderable_tab_container_element.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

class TabStrip;

// A View that contains a sequence of Tabs for the TabStrip.

// As a BrowserRootView::DropTarget, this can handle drag & drop sessions on
// behalf of BrowserRootView.

// This interface exists only for use by TabContainerImpl and
// CompoundTabContainer as described in go/2-tabcontainers-1-pager. Reuse is
// strongly discouraged outside of that context, as the interface will likely go
// away after that project is completed.
class TabContainer : public views::View, public BrowserRootView::DropTarget {
  METADATA_HEADER(TabContainer, views::View)

 public:
  // This callback is used when calculating animation targets that may increase
  // the width of the tabstrip.
  virtual void SetAvailableWidthCallback(
      base::RepeatingCallback<int()> available_width_callback) = 0;

  // Handle model changes.
  virtual Tab* AddTab(std::unique_ptr<Tab> tab,
                      int model_index,
                      TabPinned pinned) = 0;
  virtual void MoveTab(int from_model_index, int to_model_index) = 0;
  virtual void RemoveTab(int index, bool was_active) = 0;
  virtual void SetTabPinned(int model_index, TabPinned pinned) = 0;
  // Changes the active tab from |prev_active_index| to |new_active_index|.
  virtual void SetActiveTab(std::optional<size_t> prev_active_index,
                            std::optional<size_t> new_active_index) = 0;

  // Removes the tab at `model_index` from the TabContainer's data structures so
  // it can be reparented elsewhere. Unlike RemoveTab, it does not animate the
  // tab closed and it does not destroy the tab, so it should be used only to
  // transfer the tab to another parent and not to close the tab.
  [[nodiscard]] virtual Tab* RemoveTabFromViewModel(int model_index) = 0;

  // Adds the tab to the TabContainer's data structures, but does not transfer
  // ownership of the actual view `tab`.
  virtual Tab* AddTabToViewModel(Tab* tab,
                                 int model_index,
                                 TabPinned pinned) = 0;

  // Returns `view` to this TabContainer in the view hierarchy. N.B. this
  // may be called during `view`'s destruction.
  virtual void ReturnTabSlotView(TabSlotView* view) = 0;

  // Scrolls so the tab at `model_index` is fully visible.
  virtual void ScrollTabToVisible(int model_index) = 0;

  // Animates and scrolls the tab container by an offset.
  virtual void ScrollTabContainerByOffset(int offset) = 0;

  // Handle tab group model changes.
  virtual void OnGroupCreated(const tab_groups::TabGroupId& group) = 0;
  // Opens the editor bubble for the tab |group| as a result of an explicit user
  // action to create the |group|.
  virtual void OnGroupEditorOpened(const tab_groups::TabGroupId& group) = 0;
  virtual void OnGroupMoved(const tab_groups::TabGroupId& group) = 0;
  virtual void OnGroupContentsChanged(const tab_groups::TabGroupId& group) = 0;
  virtual void OnGroupVisualsChanged(
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData* old_visuals,
      const tab_groups::TabGroupVisualData* new_visuals) = 0;
  virtual void ToggleTabGroup(const tab_groups::TabGroupId& group,
                              bool is_collapsing,
                              ToggleTabGroupCollapsedStateOrigin origin) = 0;
  virtual void OnGroupClosed(const tab_groups::TabGroupId& group) = 0;
  virtual void UpdateTabGroupVisuals(tab_groups::TabGroupId group_id) = 0;
  virtual void NotifyTabGroupEditorBubbleOpened() = 0;
  virtual void NotifyTabGroupEditorBubbleClosed() = 0;

  virtual std::optional<int> GetModelIndexOf(
      const TabSlotView* slot_view) const = 0;
  virtual Tab* GetTabAtModelIndex(int index) const = 0;
  virtual int GetTabCount() const = 0;

  // Returns the model index of the first tab after (or including) `tab` which
  // is not closing.
  virtual std::optional<int> GetModelIndexOfFirstNonClosingTab(
      Tab* tab) const = 0;

  virtual void UpdateHoverCard(
      Tab* tab,
      TabSlotController::HoverCardUpdateType update_type) = 0;

  virtual void HandleLongTap(ui::GestureEvent* event) = 0;

  virtual bool IsRectInContentArea(const gfx::Rect& rect) = 0;

  virtual std::optional<ZOrderableTabContainerElement>
  GetLeadingElementForZOrdering() const = 0;
  virtual std::optional<ZOrderableTabContainerElement>
  GetTrailingElementForZOrdering() const = 0;

  // Animation stuff. Will be public until fully moved down into TabContainer.

  // Called whenever a tab or group header animation has progressed.
  virtual void OnTabSlotAnimationProgressed(TabSlotView* view) = 0;

  // Called whenever a tab close animation has completed. This kills the `tab`.
  virtual void OnTabCloseAnimationCompleted(Tab* tab) = 0;

  // Force recalculation of ideal bounds at the next layout. Used to cause tabs
  // to animate to their ideal bounds after somebody other than TabContainer
  // (cough TabDragController cough) moves tabs directly.
  virtual void InvalidateIdealBounds() = 0;

  // Animates tabs and group views from where they are to where they should be.
  // Callers that want to do fancier things can manipulate starting bounds
  // before calling this and/or replace the animation for some tabs or group
  // views after calling this.
  virtual void AnimateToIdealBounds() = 0;

  // Returns true if any tabs are being animated by this TabContainer.
  virtual bool IsAnimating() const = 0;

  // Stops any ongoing animations, leaving tabs where they are.
  virtual void CancelAnimation() = 0;

  // Stops any ongoing animations and forces a layout.
  virtual void CompleteAnimationAndLayout() = 0;

  // Returns the total width available for the TabContainer's use.
  virtual int GetAvailableWidthForTabContainer() const = 0;

  // See |in_tab_close_| for details on tab closing mode. |source| is the input
  // method used to enter tab closing mode, which determines how it is exited
  // due to user inactivity.
  virtual void EnterTabClosingMode(std::optional<int> override_width,
                                   CloseTabSource source) = 0;
  virtual void ExitTabClosingMode() = 0;

  // Sets the visibility state of all tabs and group headers (if any) based on
  // ShouldTabBeVisible().
  virtual void SetTabSlotVisibility() = 0;

  virtual bool InTabClose() = 0;

  // Returns the TabGroupViews associated with the group `group_id`.
  virtual TabGroupViews* GetGroupViews(
      tab_groups::TabGroupId group_id) const = 0;
  virtual const std::map<tab_groups::TabGroupId,
                         std::unique_ptr<TabGroupViews>>&
  get_group_views_for_testing() const = 0;

  // Returns the current width of the active tab.
  virtual int GetActiveTabWidth() const = 0;

  // Returns the current width of inactive tabs.
  virtual int GetInactiveTabWidth() const = 0;

  // Returns ideal bounds for the tab at `model_index` in this TabContainer's
  // coordinate space.
  virtual gfx::Rect GetIdealBounds(int model_index) const = 0;

  // Returns ideal bounds for the group header associated with `group` in this
  // TabContainer's coordinate space.
  virtual gfx::Rect GetIdealBounds(tab_groups::TabGroupId group) const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_

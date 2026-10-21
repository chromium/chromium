// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/shared/drop_arrow.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_container_controller.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout_helper.h"
#include "chrome/browser/ui/views/tabs/z_orderable_tab_container_element.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/paint_info.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"
#include "ui/views/view_targeter_delegate.h"

class TabStrip;
class TabHoverCardController;
class TabDragPositioningDelegateBase;

// A View that contains a sequence of Tabs for the TabStrip.
//
// As a BrowserRootView::DropTarget, this can handle drag & drop sessions on
// behalf of BrowserRootView.
class TabContainer : public views::View,
                     public BrowserRootView::DropTarget,
                     public views::ViewTargeterDelegate,
                     public views::MouseWatcherListener,
                     public views::BoundsAnimatorObserver {
  METADATA_HEADER(TabContainer, views::View)

 public:
  struct TabInsertionParams {
    std::unique_ptr<Tab> tab;
    int model_index;
    TabPinned pinned;

    TabInsertionParams(std::unique_ptr<Tab> tab, int index, TabPinned pinned);
    ~TabInsertionParams();
    TabInsertionParams(TabInsertionParams&& other) noexcept;
    TabInsertionParams& operator=(TabInsertionParams&& other) noexcept;

    TabInsertionParams(const TabInsertionParams&) = delete;
    TabInsertionParams& operator=(const TabInsertionParams&) = delete;
  };

  TabContainer(TabContainerController& controller,
               TabHoverCardController* hover_card_controller,
               TabDragPositioningDelegateBase* drag_position_delegate,
               TabSlotController& tab_slot_controller);
  ~TabContainer() override;

  // This callback is used when calculating animation targets that may increase
  // the width of the tabstrip.
  void SetAvailableWidthCallback(
      base::RepeatingCallback<int()> available_width_callback);

  // Handle model changes.
  std::vector<Tab*> AddTabs(std::vector<TabInsertionParams> tabs_params);
  void MoveTab(int from_model_index, int to_model_index);
  void RemoveTab(int index, bool was_active);
  void SetTabPinned(int model_index, TabPinned pinned);
  // Changes the active tab from `prev_active_index` to `new_active_index`.
  void SetActiveTab(std::optional<size_t> prev_active_index,
                    std::optional<size_t> new_active_index);

  // Removes the tab at `model_index` from the TabContainer's data structures so
  // it can be reparented elsewhere. Unlike RemoveTab, it does not animate the
  // tab closed and it does not destroy the tab, so it should be used only to
  // transfer the tab to another parent and not to close the tab.
  [[nodiscard]] Tab* RemoveTabFromViewModel(int model_index);

  // Adds the tab to the TabContainer's data structures, but does not transfer
  // ownership of the actual view `tab`.
  Tab* AddTabToViewModel(Tab* tab, int model_index, TabPinned pinned);

  // Returns `view` to this TabContainer in the view hierarchy. N.B. this
  // may be called during `view`'s destruction.
  void ReturnTabSlotView(TabSlotView* view);

  // Handle tab group model changes.
  void OnGroupCreated(const tab_groups::TabGroupId& group);
  // Opens the editor bubble for the tab `group` as a result of an explicit user
  // action to create the `group`.
  void OnGroupEditorOpened(const tab_groups::TabGroupId& group);
  void OnGroupMoved(const tab_groups::TabGroupId& group);
  void OnGroupContentsChanged(const tab_groups::TabGroupId& group);
  void OnGroupVisualsChanged(const tab_groups::TabGroupId& group,
                             const tab_groups::TabGroupVisualData* old_visuals,
                             const tab_groups::TabGroupVisualData* new_visuals);
  void ToggleTabGroup(const tab_groups::TabGroupId& group,
                      bool is_collapsing,
                      ToggleTabGroupCollapsedStateOrigin origin);
  void OnGroupClosed(const tab_groups::TabGroupId& group);
  void UpdateTabGroupVisuals(tab_groups::TabGroupId group_id);
  void NotifyTabstripBubbleOpened();
  void NotifyTabstripBubbleClosed();

  void OnSplitCreated(const std::vector<int>& indices);
  void OnSplitRemoved(const std::vector<int>& indices);
  void OnSplitContentsChanged(const std::vector<int>& indices);

  std::optional<int> GetModelIndexOf(const TabSlotView* slot_view) const;
  Tab* GetTabAtModelIndex(int index) const;
  int GetTabCount() const;

  // Returns the model index of the first tab after (or including) `tab` which
  // is not closing.
  std::optional<int> GetModelIndexOfFirstNonClosingTab(Tab* tab) const;

  void UpdateHoverCard(HoverCardAnchorTarget* anchor_target,
                       TabSlotController::HoverCardUpdateType update_type);

  void HandleLongTap(ui::GestureEvent* event);

  bool IsRectInContentArea(const gfx::Rect& rect);

  std::optional<ZOrderableTabContainerElement> GetLeadingElementForZOrdering()
      const;
  std::optional<ZOrderableTabContainerElement> GetTrailingElementForZOrdering()
      const;

  // Animation stuff. Will be public until fully moved down into TabContainer.

  // Called whenever a tab or group header animation has progressed.
  void OnTabSlotAnimationProgressed(TabSlotView* view);

  // Called whenever a tab close animation has completed. This kills the `tab`.
  void OnTabCloseAnimationCompleted(Tab* tab);

  // Force recalculation of ideal bounds at the next layout. Used to cause tabs
  // to animate to their ideal bounds after somebody other than TabContainer
  // (cough TabDragController cough) moves tabs directly.
  void InvalidateIdealBounds();

  // Animates tabs and group views from where they are to where they should be.
  // Callers that want to do fancier things can manipulate starting bounds
  // before calling this and/or replace the animation for some tabs or group
  // views after calling this.
  void AnimateToIdealBounds();

  // Returns true if any tabs are being animated by this TabContainer.
  bool IsAnimating() const;

  // Stops any ongoing animations, leaving tabs where they are.
  void CancelAnimation();

  // Stops any ongoing animations and forces a layout.
  void CompleteAnimationAndLayout();

  // Returns the total width available for the TabContainer's use.
  int GetAvailableWidthForTabContainer() const;

  // See `in_tab_close_` for details on tab closing mode. `source` is the input
  // method used to enter tab closing mode, which determines how it is exited
  // due to user inactivity.
  void EnterTabClosingMode(std::optional<int> override_width,
                           CloseTabSource source);
  void ExitTabClosingMode();

  // Sets the visibility state of all tabs and group headers (if any) based on
  // ShouldTabBeVisible().
  void SetTabSlotVisibility();

  bool InTabClose();

  // Returns the TabGroupViews associated with the group `group_id`.
  TabGroupViews* GetGroupViews(tab_groups::TabGroupId group_id) const;
  const std::map<tab_groups::TabGroupId, std::unique_ptr<TabGroupViews>>&
  get_group_views_for_testing() const;
  std::map<tab_groups::TabGroupId, TabGroupHeader*> GetGroupHeaders() const;

  // Returns ideal bounds for the tab at `model_index` in this TabContainer's
  // coordinate space.
  gfx::Rect GetIdealBounds(int model_index) const;

  // Returns ideal bounds for the group header associated with `group` in this
  // TabContainer's coordinate space.
  gfx::Rect GetIdealBounds(tab_groups::TabGroupId group) const;

  // views::View
  void Layout(PassKey) override;
  void PaintChildren(const views::PaintInfo& paint_info) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;

  // BrowserRootView::DropTarget:
  std::optional<BrowserRootView::DropIndex> GetDropIndex(
      const ui::DropTargetEvent& event) override;
  BrowserRootView::DropTarget* GetDropTarget(
      gfx::Point loc_in_local_coords) override;
  views::View* GetViewForDrop() override;
  void HandleDragUpdate(
      const std::optional<BrowserRootView::DropIndex>& index) override;
  void HandleDragExited() override;

  // views::ViewTargeterDelegate:
  views::View* TargetForRect(views::View* root, const gfx::Rect& rect) override;

  // MouseWatcherListener:
  void MouseMovedOutOfHost() override;

  // views::BoundsAnimatorObserver:
  void OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) override;
  void OnBoundsAnimatorDone(views::BoundsAnimator* animator) override;

  const std::vector<ZOrderableTabContainerElement>& GetZOrderCacheForTesting()
      const {
    return z_ordered_children_cache_;
  }

  const views::BoundsAnimator& GetBoundsAnimatorForTesting() const {
    return bounds_animator_;
  }

  // Used to simulate PaintChildren in unittests which is the only time in which
  // the production containers should check/update the zorder.
  void UpdateZOrderCacheForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(TabContainerTest, GetChildIndexForSlotView);
  FRIEND_TEST_ALL_PREFIXES(TabContainerTest,
                           GetChildIndexForSlotViewWithGroups);

  class RemoveTabDelegate;
  views::ViewModelT<Tab>* GetTabsViewModel();

  // Uses `bounds_animator_` to animate `view` to `target`. Use this rather than
  // calling `bounds_animator_.AnimateViewTo()` directly so animations correctly
  // track changes in rich animation enable state.
  void AnimateViewTo(
      View* view,
      const gfx::Rect& target,
      std::unique_ptr<gfx::AnimationDelegate> delegate = nullptr);

  // Animates `tab_slot_view` to `target_bounds`
  void AnimateTabSlotViewTo(TabSlotView* tab_slot_view,
                            const gfx::Rect& target_bounds);

  // Generates and sets the ideal bounds for each of the tabs. Note: Does not
  // animate the tabs to those bounds so callers can use this information for
  // other purposes - see AnimateToIdealBounds.
  void UpdateIdealBounds();

  // Teleports the tabs to their ideal bounds.
  // NOTE: this does *not* invoke UpdateIdealBounds, it uses the bounds
  // currently set in ideal_bounds.
  void SnapToIdealBounds();

  // Calculates the width that can be occupied by the tabs in the container.
  // This can differ from GetAvailableWidthForTabContainer() when in tab closing
  // mode.
  int CalculateAvailableWidthForTabs() const;

  // Invoked from `AddTab` after the newly created tab has been inserted.
  void StartInsertTabAnimation(int model_index);

  void StartRemoveTabAnimation(Tab* tab, int former_model_index);

  // Returns the group ID if the closing tab was preceded by a TabGroupHeader,
  // or std::nullopt if it was preceded by another tab or is at the start of
  // the strip without a group.
  std::optional<tab_groups::TabGroupId> GetGroupHeaderPrecedingClosingTab(
      Tab* tab,
      int former_model_index) const;

  // Computes the bounds that `tab` should animate towards as it closes.
  gfx::Rect GetTargetBoundsForClosingTab(Tab* tab,
                                         int former_model_index) const;

  // Returns the largest x-value this TabContainer should contain, based on the
  // ideal (i.e. post-animation) bounds of its contents.
  int GetIdealTrailingX() const;

  std::optional<int> GetMidAnimationTrailingX() const;

  // Update `layout_helper_` and remove the tab from `tabs_view_model_` (but
  // *not* from the View hierarchy) so it can be animated closed.
  void CloseTabInViewModel(int index);

  // Call when `tab` is going away to remove the tab from data structures.
  void OnTabRemoved(Tab* tab);

  // Updates `override_available_width_for_tabs_`, if necessary, to account for
  // the removal of the tab at `model_index`.
  void UpdateClosingModeOnRemovedTab(int model_index, bool was_active);

  // Returns whether `tab` is visible, accounting for tab group focusing and
  // collapsed groups.
  bool IsTabVisible(const Tab* tab) const;

  // Returns the number of visible tabs in `tabs_view_model_`, accounting for
  // tab group focusing and collapsed groups.
  int GetVisibleTabCount() const;

  // Returns the model index of the trailingmost visible tab in
  // `tabs_view_model_`, or -1 if no tabs are visible.
  int GetLastVisibleTabModelIndex() const;

  // Perform an animated resize-relayout of the TabContainer immediately.
  void ResizeLayoutTabs();

  // Invokes ResizeLayoutTabs() as long as we're not in a drag session. If we
  // are in a drag session this restarts the timer.
  void ResizeLayoutTabsFromTouch();

  // Restarts `resize_layout_timer_`.
  void StartResizeLayoutTabsFromTouchTimer();

  bool IsDragSessionActive() const;
  bool IsDragSessionEnding() const;

  // Ensure that the message loop observer used for event spying is added and
  // removed appropriately so we can tell when to resize layout.
  void AddMessageLoopObserver();
  void RemoveMessageLoopObserver();

  // Moves `slot_view` within children() to match `layout_helper_`'s slot
  // ordering.
  void OrderTabSlotView(TabSlotView* slot_view);

  // Returns the index in children() where `slot_view` belongs according to
  // `layout_helper_`'s slot ordering. `slot_view` must already be known to
  // `layout_helper_`, but doesn't have to be a child yet.
  size_t GetChildIndexForSlotView(const TabSlotView* slot_view) const;

  // Returns true if the specified point in TabStrip coords is within the
  // hit-test region of the specified Tab.
  bool IsPointInTab(Tab* tab, const gfx::Point& point_in_tabstrip_coords);

  // For a given point, finds a tab that is hit by the point. If the point hits
  // an area on which two tabs are overlapping, the tab is selected as follows:
  // - If one of the tabs is active, select it.
  // - Select the left one.
  // If no tabs are hit, returns null.
  Tab* FindTabHitByPoint(const gfx::Point& point);

  // Returns true if the tab is not partly or fully clipped (due to overflow),
  // and the tab couldn't become partly clipped due to changing the selected tab
  // (for example, if currently the strip has the last tab selected, and
  // changing that to the first tab would cause `tab` to be pushed over enough
  // to clip).
  bool ShouldTabBeVisible(const Tab* tab) const;

  // Returns true iff `tab` is a member of a collapsed group and the collapse
  // animation is finished.
  bool IsTabCollapsed(const Tab* tab) const;

  // -- Link Drag & Drop ------------------------------------------------------

  // Returns the bounds to render the drop at, in screen coordinates. Sets
  // `direction` to indicate which way the arrow should point.
  gfx::Rect GetDropBounds(const BrowserRootView::DropIndex& drop_index,
                          DropArrow::Direction* direction);

  // Show drop arrow with passed `tab_data_index` and `drop_before`.
  // If `tab_data_index` is negative, the arrow will disappear.
  void SetDropArrow(const std::optional<BrowserRootView::DropIndex>& index);

  // Marks the indexes and count for AX data on all tabs as stale; they are
  // refreshed by the next layout. Used by some screen readers (e.g. ChromeVox).
  void UpdateAccessibleTabIndices();

  // Refreshes the AX indexes and count of all tabs if they are stale.
  void UpdateAccessibleTabIndicesIfNeeded();

  bool IsValidModelIndex(int model_index) const;

  void MarkZOrderCacheDirty() { z_order_cache_dirty_ = true; }

  // Recalculates the zorder cache if dirty.
  // (see implementation of PaintChildren)
  void UpdateZOrderCacheIfDirty();

  std::map<tab_groups::TabGroupId, std::unique_ptr<TabGroupViews>> group_views_;

  // There is a one-to-one mapping between each of the
  // tabs in the TabStripModel and `tabs_view_model_`.
  // Because we animate tab removal there exists a
  // period of time where a tab is displayed but not
  // in the model. When this occurs the tab is removed
  // from `tabs_view_model_`, but remains in
  // `layout_helper_` (and remains a View child) until
  // the remove animation completes.
  views::ViewModelT<Tab> tabs_view_model_;

  const raw_ref<TabContainerController, DanglingUntriaged> controller_;

  const raw_ptr<TabHoverCardController, AcrossTasksDanglingUntriaged>
      hover_card_controller_;

  // May be nullptr in tests.
  const raw_ptr<TabDragPositioningDelegateBase, DanglingUntriaged>
      drag_position_delegate_;

  const raw_ref<TabSlotController> tab_slot_controller_;

  // This view is animated by `bounds_animator_` to guarantee that this
  // container's bounds change smoothly when tabs are animated into or out of
  // this container.
  const raw_ref<views::View, AcrossTasksDanglingUntriaged> overall_bounds_view_;

  // Responsible for animating tabs in response to model changes.
  views::BoundsAnimator bounds_animator_;

  const std::unique_ptr<TabStripLayoutHelper> layout_helper_;

  // MouseWatcher is used when a tab is closed to reset the layout.
  std::unique_ptr<views::MouseWatcher> mouse_watcher_;

  // Timer used when a tab is closed and we need to relayout. Only used when a
  // tab close comes from a touch device.
  base::OneShotTimer resize_layout_timer_;

  // Valid for the lifetime of a link drag over us.
  std::unique_ptr<DropArrow> drop_arrow_;

  // If this value is defined, it is used as the width to lay out tabs
  // (instead of GetAvailableWidthForTabStrip()). It is defined when closing
  // tabs with the mouse, and is used to control which tab will end up under the
  // cursor after the close animation completes.
  std::optional<int> override_available_width_for_tabs_;

  // The TabContainer enters tab closing mode when a tab is closed or a tab
  // group is collapsed with the mouse/touch. When in tab closing mode, remove
  // animations preserve current tab bounds, making tabs move more predictably
  // in case the user wants to perform more mouse-based actions.
  bool in_tab_close_ = false;

  base::RepeatingCallback<int()> available_width_callback_;

  std::vector<ZOrderableTabContainerElement> z_ordered_children_cache_;
  bool z_order_cache_dirty_ = true;

  // Whether the AX indexes and count of the tabs need to be refreshed by the
  // next layout.
  bool accessible_tab_indices_dirty_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_IMPL_H_

#include <memory>
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_container.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout_helper.h"
#include "components/tab_groups/tab_group_id.h"
#include "tab_container_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/paint_info.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"
#include "ui/views/view_targeter_delegate.h"

class TabStrip;
class TabHoverCardController;
class TabDragContextBase;

// A View that contains a sequence of Tabs for the TabStrip.
class TabContainerImpl : public TabContainer,
                         public views::ViewTargeterDelegate,
                         public views::MouseWatcherListener,
                         public views::BoundsAnimatorObserver {
  METADATA_HEADER(TabContainerImpl, TabContainer)

 public:
  TabContainerImpl(TabContainerController& controller,
                   TabHoverCardController* hover_card_controller,
                   TabDragContextBase* drag_context,
                   TabSlotController& tab_slot_controller,
                   views::View* scroll_contents_view);
  ~TabContainerImpl() override;

  // TabContainer:
  void SetAvailableWidthCallback(
      base::RepeatingCallback<int()> available_width_callback) override;

  Tab* AddTab(std::unique_ptr<Tab> tab,
              int model_index,
              TabPinned pinned) override;
  void MoveTab(int from_model_index, int to_model_index) override;
  void RemoveTab(int index, bool was_active) override;
  void SetTabPinned(int model_index, TabPinned pinned) override;
  void SetActiveTab(std::optional<size_t> prev_active_index,
                    std::optional<size_t> new_active_index) override;

  Tab* RemoveTabFromViewModel(int model_index) override;
  Tab* AddTabToViewModel(Tab* tab, int model_index, TabPinned pinned) override;
  void ReturnTabSlotView(TabSlotView* view) override;

  void ScrollTabToVisible(int model_index) override;

  void ScrollTabContainerByOffset(int offset) override;
  void OnGroupCreated(const tab_groups::TabGroupId& group) override;
  void OnGroupEditorOpened(const tab_groups::TabGroupId& group) override;
  void OnGroupMoved(const tab_groups::TabGroupId& group) override;
  void OnGroupContentsChanged(const tab_groups::TabGroupId& group) override;
  void OnGroupVisualsChanged(
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData* old_visuals,
      const tab_groups::TabGroupVisualData* new_visuals) override;
  void ToggleTabGroup(const tab_groups::TabGroupId& group,
                      bool is_collapsing,
                      ToggleTabGroupCollapsedStateOrigin origin) override;
  void OnGroupClosed(const tab_groups::TabGroupId& group) override;
  void UpdateTabGroupVisuals(tab_groups::TabGroupId group_id) override;
  void NotifyTabGroupEditorBubbleOpened() override;
  void NotifyTabGroupEditorBubbleClosed() override;

  std::optional<int> GetModelIndexOf(
      const TabSlotView* slot_view) const override;
  Tab* GetTabAtModelIndex(int index) const override;
  int GetTabCount() const override;
  std::optional<int> GetModelIndexOfFirstNonClosingTab(Tab* tab) const override;

  void UpdateHoverCard(
      Tab* tab,
      TabSlotController::HoverCardUpdateType update_type) override;

  void HandleLongTap(ui::GestureEvent* event) override;

  bool IsRectInContentArea(const gfx::Rect& rect) override;

  std::optional<ZOrderableTabContainerElement> GetLeadingElementForZOrdering()
      const override;
  std::optional<ZOrderableTabContainerElement> GetTrailingElementForZOrdering()
      const override;

  void OnTabSlotAnimationProgressed(TabSlotView* view) override;

  void OnTabCloseAnimationCompleted(Tab* tab) override;

  void InvalidateIdealBounds() override;
  void AnimateToIdealBounds() override;
  bool IsAnimating() const override;
  void CancelAnimation() override;
  void CompleteAnimationAndLayout() override;

  int GetAvailableWidthForTabContainer() const override;

  void EnterTabClosingMode(std::optional<int> override_width,
                           CloseTabSource source) override;
  void ExitTabClosingMode() override;

  void SetTabSlotVisibility() override;

  bool InTabClose() override;

  TabGroupViews* GetGroupViews(tab_groups::TabGroupId group_id) const override;
  const std::map<tab_groups::TabGroupId, std::unique_ptr<TabGroupViews>>&
  get_group_views_for_testing() const override;

  int GetActiveTabWidth() const override;
  int GetInactiveTabWidth() const override;

  gfx::Rect GetIdealBounds(int model_index) const override;
  gfx::Rect GetIdealBounds(tab_groups::TabGroupId group) const override;

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

 private:
  // Used during a drop session of a url. Tracks the position of the drop as
  // well as a window used to highlight where the drop occurs.
  class DropArrow : public views::WidgetObserver {
   public:
    DropArrow(const BrowserRootView::DropIndex& index,
              bool point_down,
              views::Widget* context);
    DropArrow(const DropArrow&) = delete;
    DropArrow& operator=(const DropArrow&) = delete;
    ~DropArrow() override;

    void set_index(const BrowserRootView::DropIndex& index) { index_ = index; }
    BrowserRootView::DropIndex index() const { return index_; }

    void SetPointDown(bool down);
    bool point_down() const { return point_down_; }

    void SetWindowBounds(const gfx::Rect& bounds);

    // views::WidgetObserver:
    void OnWidgetDestroying(views::Widget* widget) override;

   private:
    // Index of the tab to drop on.
    BrowserRootView::DropIndex index_;

    // Direction the arrow should point in. If true, the arrow is displayed
    // above the tab and points down. If false, the arrow is displayed beneath
    // the tab and points up.
    bool point_down_ = false;

    // Renders the drop indicator.
    raw_ptr<views::Widget, DanglingUntriaged> arrow_window_ = nullptr;

    raw_ptr<views::ImageView, DanglingUntriaged> arrow_view_ = nullptr;

    base::ScopedObservation<views::Widget, views::WidgetObserver>
        scoped_observation_{this};
  };

  class RemoveTabDelegate;

  views::ViewModelT<Tab>* GetTabsViewModel();

  // Private getter to retrieve the visible rect of the scroll container.
  std::optional<gfx::Rect> GetVisibleContentRect();

  // Animates and scrolls the tab container from the start_edge to the
  // target_edge. If the target_edge is beyond the tab strip it will be clamped
  // bounds of the tabstrip.
  void AnimateScrollToShowXCoordinate(const int start_edge,
                                      const int target_edge);
  // Animates |tab_slot_view| to |target_bounds|
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

  // Invoked from |AddTab| after the newly created tab has been inserted.
  void StartInsertTabAnimation(int model_index);

  void StartRemoveTabAnimation(Tab* tab, int former_model_index);

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

  // Updates |override_available_width_for_tabs_|, if necessary, to account for
  // the removal of the tab at |model_index|.
  void UpdateClosingModeOnRemovedTab(int model_index, bool was_active);

  // Perform an animated resize-relayout of the TabContainer immediately.
  void ResizeLayoutTabs();

  // Invokes ResizeLayoutTabs() as long as we're not in a drag session. If we
  // are in a drag session this restarts the timer.
  void ResizeLayoutTabsFromTouch();

  // Restarts |resize_layout_timer_|.
  void StartResizeLayoutTabsFromTouchTimer();

  bool IsDragSessionActive() const;
  bool IsDragSessionEnding() const;

  // Ensure that the message loop observer used for event spying is added and
  // removed appropriately so we can tell when to resize layout.
  void AddMessageLoopObserver();
  void RemoveMessageLoopObserver();

  // Moves |slot_view| within children() to match |layout_helper_|'s slot
  // ordering.
  void OrderTabSlotView(TabSlotView* slot_view);

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
  // changing that to the first tab would cause |tab| to be pushed over enough
  // to clip).
  bool ShouldTabBeVisible(const Tab* tab) const;

  // Returns true iff `tab` is a member of a collapsed group and the collapse
  // animation is finished.
  bool IsTabCollapsed(const Tab* tab) const;

  // -- Link Drag & Drop ------------------------------------------------------

  // Returns the bounds to render the drop at, in screen coordinates. Sets
  // |is_beneath| to indicate whether the arrow is beneath the tab, or above
  // it.
  gfx::Rect GetDropBounds(int drop_index,
                          bool drop_before,
                          bool drop_in_group,
                          bool* is_beneath);

  // Show drop arrow with passed |tab_data_index| and |drop_before|.
  // If |tab_data_index| is negative, the arrow will disappear.
  void SetDropArrow(const std::optional<BrowserRootView::DropIndex>& index);

  // Updates the indexes and count for AX data on all tabs. Used by some screen
  // readers (e.g. ChromeVox).
  void UpdateAccessibleTabIndices();

  bool IsValidModelIndex(int model_index) const;

  std::map<tab_groups::TabGroupId, std::unique_ptr<TabGroupViews>> group_views_;

  // There is a one-to-one mapping between each of the
  // tabs in the TabStripModel and |tabs_view_model_|.
  // Because we animate tab removal there exists a
  // period of time where a tab is displayed but not
  // in the model. When this occurs the tab is removed
  // from |tabs_view_model_|, but remains in
  // |layout_helper_| (and remains a View child) until
  // the remove animation completes.
  views::ViewModelT<Tab> tabs_view_model_;

  const raw_ref<TabContainerController, DanglingUntriaged> controller_;

  const raw_ptr<TabHoverCardController, AcrossTasksDanglingUntriaged>
      hover_card_controller_;

  // May be nullptr in tests.
  const raw_ptr<TabDragContextBase, DanglingUntriaged> drag_context_;

  const raw_ref<TabSlotController> tab_slot_controller_;

  // The View that is to be scrolled by |tab_scrolling_animation_|. May be
  // nullptr in tests.
  const raw_ptr<views::View> scroll_contents_view_;

  // This view is animated by `bounds_animator_` to guarantee that this
  // container's bounds change smoothly when tabs are animated into or out of
  // this container.
  const raw_ref<views::View, AcrossTasksDanglingUntriaged> overall_bounds_view_;

  // Responsible for animating tabs in response to model changes.
  views::BoundsAnimator bounds_animator_;

  // Responsible for animating the scroll of the tab container.
  std::unique_ptr<gfx::LinearAnimation> tab_scrolling_animation_;

  const std::unique_ptr<TabStripLayoutHelper> layout_helper_;

  // MouseWatcher is used when a tab is closed to reset the layout.
  std::unique_ptr<views::MouseWatcher> mouse_watcher_;

  // Timer used when a tab is closed and we need to relayout. Only used when a
  // tab close comes from a touch device.
  base::OneShotTimer resize_layout_timer_;

  // Valid for the lifetime of a link drag over us.
  std::unique_ptr<DropArrow> drop_arrow_;

  // Size we last laid out at.
  gfx::Size last_layout_size_;

  // The width available for tabs at the time of last layout.
  int last_available_width_ = 0;

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
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_IMPL_H_

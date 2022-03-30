// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_

#include <memory>
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout_helper.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/paint_info.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"
#include "ui/views/view_targeter_delegate.h"

class TabStrip;
class TabGroupHeader;
class TabHoverCardController;
class TabDragContext;

// A View that contains a sequence of Tabs for the TabStrip.
class TabContainer : public views::View,
                     public BrowserRootView::DropTarget,
                     public views::ViewTargeterDelegate,
                     public views::MouseWatcherListener,
                     public views::BoundsAnimatorObserver {
 public:
  METADATA_HEADER(TabContainer);

  TabContainer(TabStripController* controller,
               TabHoverCardController* hover_card_controller,
               TabDragContext* drag_context,
               TabSlotController* tab_slot_controller,
               views::View* scroll_contents_view);
  ~TabContainer() override;

  void SetAvailableWidthCallback(
      base::RepeatingCallback<int()> available_width_callback);

  Tab* AddTab(std::unique_ptr<Tab> tab, int model_index, TabPinned pinned);
  void MoveTab(int from_model_index, int to_model_index);
  void RemoveTab(int index, bool was_active);
  void SetTabPinned(int model_index, TabPinned pinned);

  void ScrollTabToVisible(int model_index);

  void OnGroupCreated(const tab_groups::TabGroupId& group);
  // Opens the editor bubble for the tab |group| as a result of an explicit user
  // action to create the |group|.
  void OnGroupEditorOpened(const tab_groups::TabGroupId& group);
  void OnGroupMoved(const tab_groups::TabGroupId& group);
  void OnGroupClosed(const tab_groups::TabGroupId& group);
  void UpdateTabGroupVisuals(tab_groups::TabGroupId group_id);

  int GetModelIndexOf(const TabSlotView* slot_view) const;

  views::ViewModelT<Tab>* tabs_view_model() { return &tabs_view_model_; }

  Tab* GetTabAtModelIndex(int index) const;

  int GetTabCount() const;

  void UpdateHoverCard(Tab* tab,
                       TabSlotController::HoverCardUpdateType update_type);

  void HandleLongTap(ui::GestureEvent* event);

  bool IsRectInWindowCaption(const gfx::Rect& rect);

  // Animation stuff. Will be public until fully moved down into TabContainer.

  // Called whenever a tab or group header animation has progressed.
  void OnTabSlotAnimationProgressed(TabSlotView* view);

  // Animates tabs and group views from where they are to where they should be.
  // Callers that want to do fancier things can manipulate starting bounds
  // before calling this and/or replace the animation for some tabs or group
  // views after calling this.
  void StartBasicAnimation();

  // Force recalculation of ideal bounds at the next layout. Used to cause tabs
  // to animate to their ideal bounds after somebody other than TabContainer
  // (cough TabDragController cough) moves tabs directly.
  void InvalidateIdealBounds();

  // Stops any ongoing animations. If |layout| is true and an animation is
  // ongoing this does a layout.
  void StopAnimating(bool layout);

  // Invoked from Layout if the size changes or layout is really needed.
  void CompleteAnimationAndLayout();

  // Returns the total width available for the TabContainer's use.
  int GetAvailableWidthForTabContainer() const;

  void StartResetDragAnimation(int tab_model_index);

  // See |in_tab_close_| for details on tab closing mode. |source| is the input
  // method used to enter tab closing mode, which determines how it is exited
  // due to user inactivity.
  void EnterTabClosingMode(absl::optional<int> override_width,
                           CloseTabSource source);
  void ExitTabClosingMode();

  // Sets the visibility state of all tabs and group headers (if any) based on
  // ShouldTabBeVisible().
  void SetTabSlotVisibility();

  bool in_tab_close() { return in_tab_close_; }

  // TODO (1295774): Move callers down into TabContainer so this
  // encapsulation-breaking getter can be removed.
  TabStripLayoutHelper* layout_helper() const { return layout_helper_.get(); }

  std::map<tab_groups::TabGroupId, std::unique_ptr<TabGroupViews>>&
  group_views() {
    return group_views_;
  }

  views::BoundsAnimator& bounds_animator() { return bounds_animator_; }

  // views::View
  void Layout() override;
  void PaintChildren(const views::PaintInfo& paint_info) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize() const override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;

  // BrowserRootView::DropTarget:
  BrowserRootView::DropIndex GetDropIndex(
      const ui::DropTargetEvent& event) override;
  BrowserRootView::DropTarget* GetDropTarget(
      gfx::Point loc_in_local_coords) override;
  views::View* GetViewForDrop() override;
  void HandleDragUpdate(
      const absl::optional<BrowserRootView::DropIndex>& index) override;
  void HandleDragExited() override;

  // views::ViewTargeterDelegate:
  views::View* TargetForRect(views::View* root, const gfx::Rect& rect) override;

  // MouseWatcherListener:
  void MouseMovedOutOfHost() override;

  // views::BoundsAnimatorObserver:
  void OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) override {}
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
    raw_ptr<views::Widget> arrow_window_ = nullptr;

    raw_ptr<views::ImageView> arrow_view_ = nullptr;

    base::ScopedObservation<views::Widget, views::WidgetObserver>
        scoped_observation_{this};
  };

  class RemoveTabDelegate;

  // Invoked prior to starting a new animation.
  void PrepareForAnimation();

  // Generates and sets the ideal bounds for each of the tabs as well as the new
  // tab button. Note: Does not animate the tabs to those bounds so callers can
  // use this information for other purposes - see AnimateToIdealBounds.
  void UpdateIdealBounds();

  // Animates all the views to their ideal bounds.
  // NOTE: this does *not* invoke UpdateIdealBounds, it uses the bounds
  // currently set in ideal_bounds.
  void AnimateToIdealBounds();

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

  // Remove the tab from |tabs_view_model_|, but *not* from the View hierarchy,
  // so it can be animated closed.
  void RemoveTabFromViewModel(int index);

  void OnTabCloseAnimationCompleted(Tab* tab);

  // Updates |override_available_width_for_tabs_|, if necessary, to account for
  // the removal of the tab at |model_index|.
  void UpdateClosingModeOnRemovedTab(int model_index, bool was_active);

  void MoveGroupHeader(TabGroupHeader* group_header, int first_tab_model_index);

  // Perform an animated resize-relayout of the TabContainer immediately.
  void ResizeLayoutTabs();

  // Invokes ResizeLayoutTabs() as long as we're not in a drag session. If we
  // are in a drag session this restarts the timer.
  void ResizeLayoutTabsFromTouch();

  // Restarts |resize_layout_timer_|.
  void StartResizeLayoutTabsFromTouchTimer();

  bool IsDragSessionActive() const;

  // Ensure that the message loop observer used for event spying is added and
  // removed appropriately so we can tell when to resize layout.
  void AddMessageLoopObserver();
  void RemoveMessageLoopObserver();

  // Returns the corresponding view index of a |tab| to be inserted at
  // |to_model_index|. Used to reorder the child views of the tab container
  // so that focus order stays consistent with the visual tab order.
  // |from_model_index| is where the tab currently is, if it's being moved
  // instead of added.
  int GetViewInsertionIndex(absl::optional<tab_groups::TabGroupId> group,
                            absl::optional<int> from_model_index,
                            int to_model_index) const;

  int GetViewIndexForModelIndex(int tab_model_index) const;

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
  void SetDropArrow(const absl::optional<BrowserRootView::DropIndex>& index);

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

  TabStripController* controller_;

  TabHoverCardController* hover_card_controller_;

  // May be nullptr in tests.
  TabDragContext* drag_context_;

  TabSlotController* tab_slot_controller_;

  // The View that is to be scrolled by |tab_scrolling_animation_|.
  views::View* scroll_contents_view_;

  // Responsible for animating tabs in response to model changes.
  views::BoundsAnimator bounds_animator_;

  // Responsible for animating the scroll of the tab container.
  std::unique_ptr<gfx::LinearAnimation> tab_scrolling_animation_;

  std::unique_ptr<TabStripLayoutHelper> layout_helper_;

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
  absl::optional<int> override_available_width_for_tabs_;

  // The TabContainer enters tab closing mode when a tab is closed or a tab
  // group is collapsed with the mouse/touch. When in tab closing mode, remove
  // animations preserve current tab bounds, making tabs move more predictably
  // in case the user wants to perform more mouse-based actions.
  bool in_tab_close_ = false;

  base::RepeatingCallback<int()> available_width_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_

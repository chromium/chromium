// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_animation_state.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/tabs/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/widget_observer.h"

class StackedTabStripLayout;
class Tab;
class TabHoverCardController;
class TabStripController;
class TabStripObserver;
class TabStripLayoutHelper;

namespace gfx {
class Rect;
}

namespace tab_groups {
class TabGroupId;
}

namespace ui {
class ListSelectionModel;
}

namespace views {
class ImageView;
}

// A View that represents the TabStripModel. The TabStrip has the
// following responsibilities:
//
//  - It implements the TabStripModelObserver interface, and acts as a
//    container for Tabs, and is also responsible for creating them.
//
//  - It takes part in Tab Drag & Drop with Tab, TabDragHelper and
//    DraggedTab, focusing on tasks that require reshuffling other tabs
//    in response to dragged tabs.
class TabStrip : public views::View,
                 public views::MouseWatcherListener,
                 public views::ViewObserver,
                 public views::ViewTargeterDelegate,
                 public views::WidgetObserver,
                 public views::BoundsAnimatorObserver,
                 public TabController,
                 public BrowserRootView::DropTarget {
 public:
  METADATA_HEADER(TabStrip);
  explicit TabStrip(std::unique_ptr<TabStripController> controller);
  TabStrip(const TabStrip&) = delete;
  TabStrip& operator=(const TabStrip&) = delete;
  ~TabStrip() override;

  void SetAvailableWidthCallback(
      base::RepeatingCallback<int()> available_width_callback);

  void NewTabButtonPressed(const ui::Event& event);

  // Returns the size needed for the specified views. This is invoked during
  // drag and drop to calculate offsets and positioning.
  static int GetSizeNeededForViews(const std::vector<TabSlotView*>& views);

  // Add and remove observers to changes within this TabStrip.
  void AddObserver(TabStripObserver* observer);
  void RemoveObserver(TabStripObserver* observer);

  // Called when the colors of the frame change.
  void FrameColorsChanged();

  // Sets |background_offset_| and schedules a paint.
  void SetBackgroundOffset(int background_offset);

  // Returns true if the specified rect (in TabStrip coordinates) intersects
  // the window caption area of the browser window.
  bool IsRectInWindowCaption(const gfx::Rect& rect);

  // Returns true if the specified point (in TabStrip coordinates) is in the
  // window caption area of the browser window.
  bool IsPositionInWindowCaption(const gfx::Point& point);

  // Returns false when there is a drag operation in progress so that the frame
  // doesn't close.
  bool IsTabStripCloseable() const;

  // Returns true if the tab strip is editable. Returns false if the tab strip
  // is being dragged or animated to prevent extensions from messing things up
  // while that's happening.
  bool IsTabStripEditable() const;

  // Returns whether tab dragging is in progress.
  bool CanHighlightTabs() const;

  // Returns information about tabs at given indices.
  bool IsTabCrashed(int tab_index) const;
  bool TabHasNetworkError(int tab_index) const;
  base::Optional<TabAlertState> GetTabAlertState(int tab_index) const;

  // Updates the loading animations displayed by tabs in the tabstrip to the
  // next frame. The |elapsed_time| parameter is shared between tabs and used to
  // keep the throbbers in sync.
  void UpdateLoadingAnimations(const base::TimeDelta& elapsed_time);

  // If |adjust_layout| is true the stacked layout changes based on whether the
  // user uses a mouse or a touch device with the tabstrip.
  void set_adjust_layout(bool adjust_layout) { adjust_layout_ = adjust_layout; }

  // |stacked_layout_| defines what should happen when the tabs won't fit at
  // their ideal size. When |stacked_layout_| is true the tabs are always sized
  // to their ideal size and stacked on top of each other so that only a certain
  // set of tabs are visible. This is used when the user uses a touch device.
  // When |stacked_layout_| is false the tabs shrink to accommodate the
  // available space. This is the default.
  bool stacked_layout() const { return stacked_layout_; }

  // Sets |stacked_layout_| and animates if necessary.
  void SetStackedLayout(bool stacked_layout);

  // Adds a tab at the specified index.
  void AddTabAt(int model_index, TabRendererData data, bool is_active);

  // Moves a tab.
  void MoveTab(int from_model_index, int to_model_index, TabRendererData data);

  // Removes a tab at the specified index. If the tab with |contents| is being
  // dragged then the drag is completed.
  void RemoveTabAt(content::WebContents* contents,
                   int model_index,
                   bool was_active);

  void ScrollTabToVisible(int model_index);

  // Sets the tab data at the specified model index.
  void SetTabData(int model_index, TabRendererData data);

  // Sets the tab group at the specified model index.
  void AddTabToGroup(base::Optional<tab_groups::TabGroupId> group,
                     int model_index);

  // Creates the views associated with a newly-created tab group.
  void OnGroupCreated(const tab_groups::TabGroupId& group);

  // Opens the editor bubble for the tab |group| as a result of an explicit user
  // action to create the |group|.
  void OnGroupEditorOpened(const tab_groups::TabGroupId& group);

  // Updates the group's contents and metadata when its tab membership changes.
  // This should be called when a tab is added to or removed from a group.
  void OnGroupContentsChanged(const tab_groups::TabGroupId& group);

  // Updates the group's tabs and header when its associated TabGroupVisualData
  // changes. This should be called when the result of
  // |TabStripController::GetGroupTitle(group)| or
  // |TabStripController::GetGroupColorId(group)| changes.
  void OnGroupVisualsChanged(const tab_groups::TabGroupId& group,
                             const tab_groups::TabGroupVisualData* old_visuals,
                             const tab_groups::TabGroupVisualData* new_visuals);

  // Handles animations relating to toggling the collapsed state of a group.
  void ToggleTabGroup(const tab_groups::TabGroupId& group,
                      bool is_collapsing,
                      ToggleTabGroupCollapsedStateOrigin origin);

  // Updates the ordering of the group header when the whole group is moved.
  // Needed to ensure display and focus order of the group header view.
  void OnGroupMoved(const tab_groups::TabGroupId& group);

  // Destroys the views associated with a recently deleted tab group.
  void OnGroupClosed(const tab_groups::TabGroupId& group);

  // Attempts to move the specified group to the left.
  void ShiftGroupLeft(const tab_groups::TabGroupId& group);

  // Attempts to move the specified group to the right.
  void ShiftGroupRight(const tab_groups::TabGroupId& group);

  // Returns true if the tab is not partly or fully clipped (due to overflow),
  // and the tab couldn't become partly clipped due to changing the selected tab
  // (for example, if currently the strip has the last tab selected, and
  // changing that to the first tab would cause |tab| to be pushed over enough
  // to clip).
  bool ShouldTabBeVisible(const Tab* tab) const;

  // Returns whether or not strokes should be drawn around and under the tabs.
  bool ShouldDrawStrokes() const;

  // Invoked when the selection is updated.
  void SetSelection(const ui::ListSelectionModel& new_selection);

  // Invoked when a tab needs to show UI that it needs the user's attention.
  void SetTabNeedsAttention(int model_index, bool attention);

  // Retrieves the ideal bounds for the Tab at the specified index.
  const gfx::Rect& ideal_bounds(int tab_data_index) const {
    return tabs_.ideal_bounds(tab_data_index);
  }

  // Retrieves the ideal bounds for the Tab Group Header at the specified group.
  const gfx::Rect& ideal_bounds(tab_groups::TabGroupId group) const;

  // Returns the Tab at |index|.
  // TODO(pkasting): Make const correct
  Tab* tab_at(int index) const { return tabs_.view_at(index); }

  // Returns the TabGroupHeader with ID |id|.
  TabGroupHeader* group_header(const tab_groups::TabGroupId& id) const {
    return group_views_.at(id).get()->header();
  }

  // Returns the index of the specified view in the model coordinate system, or
  // -1 if view is closing or not a tab.
  int GetModelIndexOf(const TabSlotView* view) const;

  // Gets the number of Tabs in the tab strip.
  int GetTabCount() const;

  // Cover method for TabStripController::GetCount.
  int GetModelCount() const;

  // Cover method for TabStripController::IsValidIndex.
  bool IsValidModelIndex(int model_index) const;

  TabStripController* controller() const { return controller_.get(); }

  TabDragContext* GetDragContext();

  // Returns the number of pinned tabs.
  int GetPinnedTabCount() const;

  // Returns true if Tabs in this TabStrip are currently changing size or
  // position.
  bool IsAnimating() const;

  // Stops any ongoing animations. If |layout| is true and an animation is
  // ongoing this does a layout.
  void StopAnimating(bool layout);

  // Returns the index of the focused tab, if any.
  base::Optional<int> GetFocusedTabIndex() const;

  // Returns a view for anchoring an in-product help promo. |index_hint|
  // indicates at which tab the promo should be displayed, but is not
  // binding.
  views::View* GetTabViewForPromoAnchor(int index_hint);

  // Gets the default focusable child view in the TabStrip.
  views::View* GetDefaultFocusableChild();

  // TabController:
  const ui::ListSelectionModel& GetSelectionModel() const override;
  bool SupportsMultipleSelection() override;
  bool ShouldHideCloseButtonForTab(Tab* tab) const override;
  void SelectTab(Tab* tab, const ui::Event& event) override;
  void ExtendSelectionTo(Tab* tab) override;
  void ToggleSelected(Tab* tab) override;
  void AddSelectionFromAnchorTo(Tab* tab) override;
  void CloseTab(Tab* tab, CloseTabSource source) override;
  void ShiftTabNext(Tab* tab) override;
  void ShiftTabPrevious(Tab* tab) override;
  void MoveTabFirst(Tab* tab) override;
  void MoveTabLast(Tab* tab) override;
  void ShowContextMenuForTab(Tab* tab,
                             const gfx::Point& p,
                             ui::MenuSourceType source_type) override;
  bool IsActiveTab(const Tab* tab) const override;
  bool IsTabSelected(const Tab* tab) const override;
  bool IsTabPinned(const Tab* tab) const override;
  bool IsTabFirst(const Tab* tab) const override;
  bool IsFocusInTabs() const override;
  void MaybeStartDrag(
      TabSlotView* source,
      const ui::LocatedEvent& event,
      const ui::ListSelectionModel& original_selection) override;
  void ContinueDrag(views::View* view, const ui::LocatedEvent& event) override;
  bool EndDrag(EndDragReason reason) override;
  Tab* GetTabAt(const gfx::Point& point) override;
  const Tab* GetAdjacentTab(const Tab* tab, int offset) override;
  void OnMouseEventInTab(views::View* source,
                         const ui::MouseEvent& event) override;
  void UpdateHoverCard(Tab* tab, HoverCardUpdateType update_type) override;
  bool ShowDomainInHoverCards() const override;
  bool HoverCardIsShowingForTab(Tab* tab) override;
  int GetBackgroundOffset() const override;
  int GetStrokeThickness() const override;
  bool CanPaintThrobberToLayer() const override;
  bool HasVisibleBackgroundTabShapes() const override;
  bool ShouldPaintAsActiveFrame() const override;
  SkColor GetToolbarTopSeparatorColor() const override;
  SkColor GetTabSeparatorColor() const override;
  SkColor GetTabBackgroundColor(
      TabActive active,
      BrowserFrameActiveState active_state) const override;
  SkColor GetTabForegroundColor(TabActive active,
                                SkColor background_color) const override;
  std::u16string GetAccessibleTabName(const Tab* tab) const override;
  base::Optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const override;
  gfx::Rect GetTabAnimationTargetBounds(const Tab* tab) override;
  float GetHoverOpacityForTab(float range_parameter) const override;
  float GetHoverOpacityForRadialHighlight() const override;
  std::u16string GetGroupTitle(
      const tab_groups::TabGroupId& group) const override;
  tab_groups::TabGroupColorId GetGroupColorId(
      const tab_groups::TabGroupId& group) const override;
  SkColor GetPaintedGroupColor(
      const tab_groups::TabGroupColorId& color_id) const override;

  // MouseWatcherListener:
  void MouseMovedOutOfHost() override;

  // views::View:
  void Layout() override;
  void PaintChildren(const views::PaintInfo& paint_info) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize() const override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;

  // BrowserRootView::DropTarget:
  BrowserRootView::DropIndex GetDropIndex(
      const ui::DropTargetEvent& event) override;
  views::View* GetViewForDrop() override;
  void HandleDragUpdate(
      const base::Optional<BrowserRootView::DropIndex>& index) override;
  void HandleDragExited() override;

 private:
  class RemoveTabDelegate;
  class TabDragContextImpl;

  friend class TabDragControllerTest;
  friend class TabDragContextImpl;
  friend class TabGroupEditorBubbleViewDialogBrowserTest;
  friend class TabHoverCardBubbleViewBrowserTest;
  friend class TabHoverCardBubbleViewInteractiveUiTest;
  friend class TabStripTestBase;
  friend class TabStripRegionViewTestBase;

  class TabContextMenuController : public views::ContextMenuController {
   public:
    explicit TabContextMenuController(TabStrip* parent);
    // views::ContextMenuController:
    void ShowContextMenuForViewImpl(views::View* source,
                                    const gfx::Point& point,
                                    ui::MenuSourceType source_type) override;

   private:
    TabStrip* const parent_;
  };

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
    views::Widget* arrow_window_ = nullptr;

    views::ImageView* arrow_view_ = nullptr;

    base::ScopedObservation<views::Widget, views::WidgetObserver>
        scoped_observation_{this};
  };

  void Init();

  views::ViewModelT<Tab>* tabs_view_model() { return &tabs_; }

  std::map<tab_groups::TabGroupId, TabGroupHeader*> GetGroupHeaders();

  // Invoked from |AddTabAt| after the newly created tab has been inserted.
  void StartInsertTabAnimation(int model_index, TabPinned pinned);

  // Animates the removal of the tab at |model_index|. Defers to the old
  // animation style when appropriate.
  void StartRemoveTabAnimation(int model_index, bool was_active);

  // Animates the removal of the tab at |model_index| using the old animation
  // style.
  void StartFallbackRemoveTabAnimation(int model_index, bool was_active);

  // Invoked from |MoveTab| after |tab_data_| has been updated to animate the
  // move.
  void StartMoveTabAnimation();

  // Animates all the views to their ideal bounds.
  // NOTE: this does *not* invoke UpdateIdealBounds, it uses the bounds
  // currently set in ideal_bounds.
  void AnimateToIdealBounds();

  // Teleports the tabs to their ideal bounds.
  // NOTE: this does *not* invoke UpdateIdealBounds, it uses the bounds
  // currently set in ideal_bounds.
  void SnapToIdealBounds();

  void ExitTabClosingMode();

  // Returns whether the close button should be highlighted after a remove.
  bool ShouldHighlightCloseButtonAfterRemove();

  // Returns whether the window background behind the tabstrip is transparent.
  bool TitlebarBackgroundIsTransparent() const;

  // Invoked from Layout if the size changes or layout is really needed.
  void CompleteAnimationAndLayout();

  // Sets the visibility state of all tabs and group headers (if any) based on
  // ShouldTabBeVisible().
  void SetTabSlotVisibility();

  // Updates the indexes and count for AX data on all tabs. Used by some screen
  // readers (e.g. ChromeVox).
  void UpdateAccessibleTabIndices();

  // Returns the current width of the active tab.
  int GetActiveTabWidth() const;

  // Returns the current width of inactive tabs. An individual inactive tab may
  // differ from this width slightly due to rounding.
  int GetInactiveTabWidth() const;

  // Returns the last tab in the strip that's actually visible.  This will be
  // the actual last tab unless the strip is in the overflow node_data.
  const Tab* GetLastVisibleTab() const;

  // Returns the view index (the order of ChildViews of the TabStrip) of the
  // given |tab| based on its model index when it moves. Used to reorder the
  // child views of the tabstrip so that focus order stays consistent.
  int GetViewInsertionIndex(Tab* tab,
                            base::Optional<int> from_model_index,
                            int to_model_index) const;

  // Closes the tab at |model_index|.
  void CloseTabInternal(int model_index, CloseTabSource source);

  // Removes the tab at |index| from |tabs_|.
  void RemoveTabFromViewModel(int index);

  // Cleans up the Tab from the TabStrip. This is called from the tab animation
  // code and is not a general-purpose method.
  void OnTabCloseAnimationCompleted(Tab* tab);

  // Invoked from StoppedDraggingTabs to cleanup |view|. If |view| is known
  // |is_first_view| is set to true.
  void StoppedDraggingView(TabSlotView* view, bool* is_first_view);

  // Invoked when a mouse event occurs over |source|. Potentially switches the
  // |stacked_layout_|.
  void UpdateStackedLayoutFromMouseEvent(views::View* source,
                                         const ui::MouseEvent& event);

  // Computes and stores values derived from contrast ratios.
  void UpdateContrastRatioValues();

  // Determines whether a tab can be shifted by one in the direction of |offset|
  // and moves it if possible.
  void ShiftTabRelative(Tab* tab, int offset);

  // Determines whether a group can be shifted by one in the direction of
  // |offset| and moves it if possible.
  void ShiftGroupRelative(const tab_groups::TabGroupId& group, int offset);

  // -- Tab Resize Layout -----------------------------------------------------

  // Perform an animated resize-relayout of the TabStrip immediately.
  void ResizeLayoutTabs();

  // Invokes ResizeLayoutTabs() as long as we're not in a drag session. If we
  // are in a drag session this restarts the timer.
  void ResizeLayoutTabsFromTouch();

  // Restarts |resize_layout_timer_|.
  void StartResizeLayoutTabsFromTouchTimer();

  // Ensure that the message loop observer used for event spying is added and
  // removed appropriately so we can tell when to resize layout the tab strip.
  void AddMessageLoopObserver();
  void RemoveMessageLoopObserver();

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
  void SetDropArrow(const base::Optional<BrowserRootView::DropIndex>& index);

  // Returns the image to use for indicating a drop on a tab. If is_down is
  // true, this returns an arrow pointing down.
  static gfx::ImageSkia* GetDropArrowImage(bool is_down);

  // -- Animations ------------------------------------------------------------

  // Invoked prior to starting a new animation.
  void PrepareForAnimation();

  // Generates and sets the ideal bounds for each of the tabs as well as the new
  // tab button. Note: Does not animate the tabs to those bounds so callers can
  // use this information for other purposes - see AnimateToIdealBounds.
  void UpdateIdealBounds();

  // Generates and sets the ideal bounds for the pinned tabs. Returns the index
  // to position the first non-pinned tab and sets |first_non_pinned_index| to
  // the index of the first non-pinned tab.
  int UpdateIdealBoundsForPinnedTabs(int* first_non_pinned_index);

  // Calculates the width that can be occupied by the tabs in the strip. This
  // can differ from GetAvailableWidthForTabStrip() when in tab closing mode.
  int CalculateAvailableWidthForTabs() const;

  // Returns the total width available for the TabStrip's use.
  int GetAvailableWidthForTabStrip() const;

  // Starts various types of TabStrip animations.
  void StartResizeLayoutAnimation();
  void StartPinnedTabAnimation();

  // Returns true if the specified point in TabStrip coords is within the
  // hit-test region of the specified Tab.
  bool IsPointInTab(Tab* tab, const gfx::Point& point_in_tabstrip_coords);

  // -- Touch Layout ----------------------------------------------------------

  // Returns the tab to use for event handling. This uses FindTabForEventFrom()
  // to do the actual searching.  This method should be called when
  // |touch_layout_| is set.
  Tab* FindTabForEvent(const gfx::Point& point);

  // Helper for FindTabForEvent().  Returns the tab to use for event handling
  // starting at index |start| and iterating by |delta|.
  Tab* FindTabForEventFrom(const gfx::Point& point, int start, int delta);

  // For a given point, finds a tab that is hit by the point. If the point hits
  // an area on which two tabs are overlapping, the tab is selected as follows:
  // - If one of the tabs is active, select it.
  // - Select the left one.
  // If no tabs are hit, returns null.  This method should be called when
  // |touch_layout_| is not set.
  Tab* FindTabHitByPoint(const gfx::Point& point);

  // Creates/Destroys |touch_layout_| as necessary.
  void SwapLayoutIfNecessary();

  // Returns true if |touch_layout_| is needed.
  bool NeedsTouchLayout() const;

  // Sets the value of |reset_to_shrink_on_exit_|. If true |mouse_watcher_| is
  // used to track when the mouse truly exits the tabstrip and the stacked
  // layout is reset.
  void SetResetToShrinkOnExit(bool value);

  // Called whenever a tab animation has progressed.
  void OnTabSlotAnimationProgressed(TabSlotView* view);

  // Called to update the visuals for a tab group when tabs in the group are
  // moved or resized.
  void UpdateTabGroupVisuals(tab_groups::TabGroupId tab_group_id);

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::ViewTargeterDelegate:
  views::View* TargetForRect(views::View* root, const gfx::Rect& rect) override;

  // views::ViewObserver:
  void OnViewFocused(views::View* observed_view) override;
  void OnViewBlurred(views::View* observed_view) override;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // views::BoundsAnimatorObserver:
  void OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) override {}
  void OnBoundsAnimatorDone(views::BoundsAnimator* animator) override;

  void OnTouchUiChanged();

  // Screen-reader-only announcements that depend on tab group titles.
  void AnnounceTabAddedToGroup(tab_groups::TabGroupId group_id);
  void AnnounceTabRemovedFromGroup(tab_groups::TabGroupId group_id);

  // -- Member Variables ------------------------------------------------------

  base::ObserverList<TabStripObserver>::Unchecked observers_;

  // There is a one-to-one mapping between each of the tabs in the
  // TabStripController (TabStripModel) and |tabs_|. Because we animate tab
  // removal there exists a period of time where a tab is displayed but not in
  // the model. When this occurs the tab is removed from |tabs_|, but remains
  // in |layout_helper_| until the remove animation completes.
  views::ViewModelT<Tab> tabs_;

  std::map<tab_groups::TabGroupId, std::unique_ptr<TabGroupViews>> group_views_;

  std::unique_ptr<TabStripController> controller_;

  base::RepeatingCallback<int()> available_width_callback_;

  std::unique_ptr<TabStripLayoutHelper> layout_helper_;

  // Responsible for animating tabs in response to model changes.
  views::BoundsAnimator bounds_animator_{this};

  // Responsible for animating the scroll of the tab strip.
  std::unique_ptr<gfx::LinearAnimation> tab_scrolling_animation_;

  // If this value is defined, it is used as the width to lay out tabs
  // (instead of GetAvailableWidthForTabStrip()). It is defined when closing
  // tabs with the mouse, and is used to control which tab will end up under the
  // cursor after the close animation completes.
  base::Optional<int> override_available_width_for_tabs_;

  // The background offset used by inactive tabs to match the frame image.
  int background_offset_ = 0;

  // True if PrepareForCloseAt has been invoked. When true remove animations
  // preserve current tab bounds.
  bool in_tab_close_ = false;

  // Valid for the lifetime of a drag over us.
  std::unique_ptr<DropArrow> drop_arrow_;

  // MouseWatcher is used for two things:
  // . When a tab is closed to reset the layout.
  // . When a mouse is used and the layout dynamically adjusts and is currently
  //   stacked (|stacked_layout_| is true).
  std::unique_ptr<views::MouseWatcher> mouse_watcher_;

  // Size we last layed out at.
  gfx::Size last_layout_size_;

  // The width available for tabs at the time of last layout.
  int last_available_width_ = 0;

  // See description above stacked_layout().
  bool stacked_layout_ = false;

  // Should the layout dynamically adjust?
  bool adjust_layout_ = false;

  // Only used while in touch mode.
  std::unique_ptr<StackedTabStripLayout> touch_layout_;

  // If true the |stacked_layout_| is set to false when the mouse exits the
  // tabstrip (as determined using MouseWatcher).
  bool reset_to_shrink_on_exit_ = false;

  // Location of the mouse at the time of the last move.
  gfx::Point last_mouse_move_location_;

  // Time of the last mouse move event.
  base::TimeTicks last_mouse_move_time_;

  // Used for seek time metrics from the time the mouse enters the tabstrip.
  base::Optional<base::TimeTicks> mouse_entered_tabstrip_time_;

  // Number of mouse moves.
  int mouse_move_count_ = 0;

  // Timer used when a tab is closed and we need to relayout. Only used when a
  // tab close comes from a touch device.
  base::OneShotTimer resize_layout_timer_;

  // This represents the Tabs in |tabs_| that have been selected.
  //
  // Each time tab selection should change, this class will receive a
  // SetSelection() callback with the new tab selection. That callback only
  // includes the new selection model. This keeps track of the previous
  // selection model, and is always consistent with |tabs_|. This must be
  // updated to account for tab insertions/removals/moves.
  ui::ListSelectionModel selected_tabs_;

  // When tabs are hovered, a radial highlight is shown and the tab opacity is
  // adjusted using some value between |hover_opacity_min_| and
  // |hover_opacity_max_| (depending on tab width). All these opacities depend
  // on contrast ratios and are updated when colors or active state changes,
  // so for efficiency's sake they are computed and stored once here instead
  // of with each tab. Note: these defaults will be overwritten at construction
  // except in cases where a unit test provides no controller_.
  float hover_opacity_min_ = 1.0f;
  float hover_opacity_max_ = 1.0f;
  float radial_highlight_opacity_ = 1.0f;

  SkColor separator_color_ = gfx::kPlaceholderColor;

  std::unique_ptr<TabHoverCardController> hover_card_controller_;

  const base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&TabStrip::OnTouchUiChanged,
                              base::Unretained(this)));

  std::unique_ptr<TabDragContextImpl> drag_context_;

  TabContextMenuController context_menu_controller_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_H_

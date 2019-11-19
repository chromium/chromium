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
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
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
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/material_design/material_design_controller_observer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/widget_observer.h"

class Browser;
class NewTabButton;
class StackedTabStripLayout;
class Tab;
class TabGroupUnderline;
class TabGroupId;
class TabHoverCardBubbleView;
class TabStripController;
class TabStripObserver;
class TabStripLayoutHelper;

namespace gfx {
class Rect;
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
class TabStrip : public views::AccessiblePaneView,
                 public views::ButtonListener,
                 public views::MouseWatcherListener,
                 public views::ViewObserver,
                 public views::ViewTargeterDelegate,
                 public views::WidgetObserver,
                 public TabController,
                 public BrowserRootView::DropTarget,
                 public ui::MaterialDesignControllerObserver {
 public:
  explicit TabStrip(std::unique_ptr<TabStripController> controller);
  ~TabStrip() override;

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

  // Returns the ideal bounds of the new tab button.
  gfx::Rect new_tab_button_ideal_bounds() const {
    return new_tab_button_ideal_bounds_;
  }

  // Adds a tab at the specified index.
  void AddTabAt(int model_index, TabRendererData data, bool is_active);

  // Moves a tab.
  void MoveTab(int from_model_index, int to_model_index, TabRendererData data);

  // Removes a tab at the specified index. If the tab with |contents| is being
  // dragged then the drag is completed.
  void RemoveTabAt(content::WebContents* contents,
                   int model_index,
                   bool was_active);

  // Sets the tab data at the specified model index.
  void SetTabData(int model_index, TabRendererData data);

  // Changes the group of the tab at |model_index| from |old_group| to
  // |new_group|.
  void ChangeTabGroup(int model_index,
                      base::Optional<TabGroupId> old_group,
                      base::Optional<TabGroupId> new_group);

  // Updates the group's tabs and header when its associated TabGroupVisualData
  // changes. This should be called when the result of
  // |TabStripController::GetVisualDataForGroup(group)| changes.
  void GroupVisualsChanged(TabGroupId group);

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
  // TODO(958173): The notion of ideal bounds is going away. Delete this.
  const gfx::Rect& ideal_bounds(int tab_data_index) const {
    return tabs_.ideal_bounds(tab_data_index);
  }

  // Returns the Tab at |index|.
  // TODO(pkasting): Make const correct
  Tab* tab_at(int index) const { return tabs_.view_at(index); }

  // Returns the TabGroupHeader with ID |id|.
  TabGroupHeader* group_header(TabGroupId id) {
    return group_headers_[id].get();
  }

  // Returns the NewTabButton.
  NewTabButton* new_tab_button() { return new_tab_button_; }

  // Returns the index of the specified view in the model coordinate system, or
  // -1 if view is closing or not a tab.
  int GetModelIndexOf(const TabSlotView* view) const;

  // Gets the number of Tabs in the tab strip.
  int tab_count() const { return tabs_.view_size(); }

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

  // TabController:
  const ui::ListSelectionModel& GetSelectionModel() const override;
  bool SupportsMultipleSelection() override;
  bool ShouldHideCloseButtonForTab(Tab* tab) const override;
  void SelectTab(Tab* tab, const ui::Event& event) override;
  void ExtendSelectionTo(Tab* tab) override;
  void ToggleSelected(Tab* tab) override;
  void AddSelectionFromAnchorTo(Tab* tab) override;
  void CloseTab(Tab* tab, CloseTabSource source) override;
  void MoveTabLeft(Tab* tab) override;
  void MoveTabRight(Tab* tab) override;
  void MoveTabFirst(Tab* tab) override;
  void MoveTabLast(Tab* tab) override;
  void ShowContextMenuForTab(Tab* tab,
                             const gfx::Point& p,
                             ui::MenuSourceType source_type) override;
  bool IsActiveTab(const Tab* tab) const override;
  bool IsTabSelected(const Tab* tab) const override;
  bool IsTabPinned(const Tab* tab) const override;
  bool IsFirstVisibleTab(const Tab* tab) const override;
  bool IsLastVisibleTab(const Tab* tab) const override;
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
  void UpdateHoverCard(Tab* tab) override;
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
  base::string16 GetAccessibleTabName(const Tab* tab) const override;
  base::Optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const override;
  gfx::Rect GetTabAnimationTargetBounds(const Tab* tab) override;
  float GetHoverOpacityForTab(float range_parameter) const override;
  float GetHoverOpacityForRadialHighlight() const override;
  const TabGroupVisualData* GetVisualDataForGroup(
      TabGroupId group) const override;
  void SetVisualDataForGroup(TabGroupId group,
                             TabGroupVisualData visual_data) override;
  void CloseAllTabsInGroup(TabGroupId group) override;
  void UngroupAllTabsInGroup(TabGroupId group) override;
  void AddNewTabInGroup(TabGroupId group) override;
  const Browser* GetBrowser() override;

  // MouseWatcherListener:
  void MouseMovedOutOfHost() override;

  // views::AccessiblePaneView:
  void Layout() override;
  void PaintChildren(const views::PaintInfo& paint_info) override;
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  void OnThemeChanged() override;
  views::View* GetDefaultFocusableChild() override;

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
  friend class TabStripTest;

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

    ScopedObserver<views::Widget, views::WidgetObserver> scoped_observer_{this};

    DISALLOW_COPY_AND_ASSIGN(DropArrow);
  };

  // Specifies how to handle tabs that are midway through closing when falling
  // back from |layout_helper_| to |bounds_animator_|.
  enum class ClosingTabsBehavior {
    // Keep the tabs alive, because responsibilities for destroying them lie
    // with |bounds_animator_|.
    kTransferOwnership,
    // Destroy the tabs, because responsibilities for destroying them lie with
    // |layout_helper_|.
    kDestroy
  };

  void Init();

  views::ViewModelT<Tab>* tabs_view_model() { return &tabs_; }

  std::map<TabGroupId, TabGroupHeader*> GetGroupHeaders();

  // Invoked from |AddTabAt| after the newly created tab has been inserted.
  void StartInsertTabAnimation(int model_index, TabPinned pinned);

  // Animates the removal of the tab at |model_index|. Defers to the old
  // animation style when appropriate.
  void StartRemoveTabAnimation(int model_index, bool was_active);

  // Animates the removal of the tab at |model_index| using the old animation
  // style.
  // TODO(958173): Delete this once all animations have been migrated to the
  // new animation style.
  void StartFallbackRemoveTabAnimation(int model_index, bool was_active);

  // Invoked from |MoveTab| after |tab_data_| has been updated to animate the
  // move.
  void StartMoveTabAnimation();

  // Animates all the views to their ideal bounds.
  // NOTE: this does *not* invoke UpdateIdealBounds, it uses the bounds
  // currently set in ideal_bounds.
  // TODO(958173): The notion of ideal bounds is going away. Delete this.
  void AnimateToIdealBounds(ClosingTabsBehavior closing_tabs_behavior =
                                ClosingTabsBehavior::kDestroy);

  void ExitTabClosingMode();

  // Returns whether the close button should be highlighted after a remove.
  bool ShouldHighlightCloseButtonAfterRemove();

  // Returns the spacing between the trailing edge of the tabs and the leading
  // edge of the new tab button.
  int TabToNewTabButtonSpacing() const;

  // Returns the space to reserve after the tabs to guarantee the user can grab
  // part of the window frame (to move the window with).
  int FrameGrabWidth() const;

  // Returns whether the window background behind the tabstrip is transparent.
  bool TitlebarBackgroundIsTransparent() const;

  // Invoked from Layout if the size changes or layout is really needed.
  void CompleteAnimationAndLayout();

  // Invoked to re-layout the tabs as animations progress.
  void LayoutToCurrentBounds();

  // Sets the visibility state of all tabs based on ShouldTabBeVisible().
  void SetTabVisibility();

  // Updates the indexes and count for AX data on all tabs. Used by some screen
  // readers (e.g. ChromeVox).
  void UpdateAccessibleTabIndices();

  // Returns the current width of the active tab.
  int GetActiveTabWidth() const;

  // Returns the current width of inactive tabs. An individual inactive tab may
  // differ from this width slightly due to rounding.
  int GetInactiveTabWidth() const;

  // Returns the width of the area that contains tabs not including the new tab
  // button.
  int GetTabAreaWidth() const;

  // Returns the X coordinate the new tab button should be placed at.  Requires
  // |tabs_| to have correct ideal bounds.
  int GetNewTabButtonIdealX() const;

  // Returns the last tab in the strip that's actually visible.  This will be
  // the actual last tab unless the strip is in the overflow node_data.
  const Tab* GetLastVisibleTab() const;

  // Removes the tab at |index| from |tabs_|.
  void RemoveTabFromViewModel(int index);

  // Cleans up the Tab from the TabStrip. This is called from the tab animation
  // code and is not a general-purpose method.
  void OnTabCloseAnimationCompleted(Tab* tab);

  // Cleans up the TabGroupHeader for |group| from the TabStrip. This is called
  // from the tab animation code and is not a general-purpose method.
  void OnGroupCloseAnimationCompleted(TabGroupId group);

  // Invoked from StoppedDraggingTabs to cleanup |view|. If |view| is known
  // |is_first_view| is set to true.
  void StoppedDraggingView(TabSlotView* view, bool* is_first_view);

  // Invoked when a mouse event occurs over |source|. Potentially switches the
  // |stacked_layout_|.
  void UpdateStackedLayoutFromMouseEvent(views::View* source,
                                         const ui::MouseEvent& event);

  // Computes and stores values derived from contrast ratios.
  void UpdateContrastRatioValues();

  // Determines whether a tab can be moved by |offset| positions and moves it if
  // possible.
  void MoveTabRelative(Tab* tab, int offset);

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
  gfx::Rect GetDropBounds(int drop_index, bool drop_before, bool* is_beneath);

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
  // tab button.
  // TODO(958173): The notion of ideal bounds is going away. Delete this.
  void UpdateIdealBounds();

  // Generates and sets the ideal bounds for the pinned tabs. Returns the index
  // to position the first non-pinned tab and sets |first_non_pinned_index| to
  // the index of the first non-pinned tab.
  // TODO(958173): The notion of ideal bounds is going away. Delete this.
  int UpdateIdealBoundsForPinnedTabs(int* first_non_pinned_index);

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

  // Updates the border padding for |new_tab_button_|.  This should be called
  // whenever any input of the computation of the border's sizing changes.
  void UpdateNewTabButtonBorder();

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  const views::View* GetViewByID(int id) const override;
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
  void OnViewIsDeleting(views::View* observed_view) override;
  void OnViewFocused(views::View* observed_view) override;
  void OnViewBlurred(views::View* observed_view) override;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // ui::MaterialDesignControllerObserver:
  void OnTouchUiChanged() override;

  // -- Member Variables ------------------------------------------------------

  base::ObserverList<TabStripObserver>::Unchecked observers_;

  // There is a one-to-one mapping between each of the tabs in the
  // TabStripController (TabStripModel) and |tabs_|. Because we animate tab
  // removal there exists a period of time where a tab is displayed but not in
  // the model. When this occurs the tab is removed from |tabs_|, but remains
  // in |layout_helper_| until the remove animation completes.
  views::ViewModelT<Tab> tabs_;

  // Map associating each group to its TabGroupHeader instance.
  std::map<TabGroupId, std::unique_ptr<TabGroupHeader>> group_headers_;

  // Map associating each group to its TabGroupUnderline instance.
  std::map<TabGroupId, std::unique_ptr<TabGroupUnderline>> group_underlines_;

  // The view tracker is used to keep track of if the hover card has been
  // destroyed by its widget.
  TabHoverCardBubbleView* hover_card_ = nullptr;
  ScopedObserver<views::View, views::ViewObserver> hover_card_observer_{this};
  std::unique_ptr<ui::EventHandler> hover_card_event_sniffer_;

  std::unique_ptr<TabStripController> controller_;

  std::unique_ptr<TabStripLayoutHelper> layout_helper_;

  // Responsible for animating tabs in response to model changes.
  // Deprecated; https://crbug.com/958173 tracks migrating animations from
  // |bounds_animator_| to |TabStripLayoutHelper::animator_|.
  views::BoundsAnimator bounds_animator_{this};

  // The "New Tab" button.
  NewTabButton* new_tab_button_ = nullptr;

  // Ideal bounds of the new tab button.
  gfx::Rect new_tab_button_ideal_bounds_;

  // If this value is defined, it is used as the width to lay out tabs
  // (instead of GetTabAreaWidth()). It is defined when closing tabs with the
  // mouse, and is used to control which tab will end up under the cursor
  // after the close animation completes.
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

  ScopedObserver<ui::MaterialDesignController,
                 ui::MaterialDesignControllerObserver>
      md_observer_{this};

  std::unique_ptr<TabDragContextImpl> drag_context_;

  TabContextMenuController context_menu_controller_{this};

  DISALLOW_COPY_AND_ASSIGN(TabStrip);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_H_

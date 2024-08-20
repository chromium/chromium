// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_H_

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_container.h"
#include "chrome/browser/ui/views/tabs/tab_container_controller.h"
#include "chrome/browser/ui/views/tabs/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/widget_observer.h"

class Tab;
class TabHoverCardController;
class TabStripController;
class TabStripObserver;
class TabStyle;

namespace gfx {
class Rect;
}

namespace tab_groups {
class TabGroupId;
}

namespace ui {
class ListSelectionModel;
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
                 public views::ViewObserver,
                 public views::WidgetObserver,
                 public TabContainerController,
                 public TabSlotController,
                 public BrowserRootView::DropTarget {
  METADATA_HEADER(TabStrip, views::View)

 public:
  explicit TabStrip(std::unique_ptr<TabStripController> controller);
  TabStrip(const TabStrip&) = delete;
  TabStrip& operator=(const TabStrip&) = delete;
  ~TabStrip() override;

  void SetAvailableWidthCallback(
      base::RepeatingCallback<int()> available_width_callback);

  void NewTabButtonPressed(const ui::Event& event);

  // Returns the size needed for the specified views. This is invoked during
  // drag and drop to calculate offsets and positioning.
  static int GetSizeNeededForViews(
      const std::vector<raw_ptr<TabSlotView, VectorExperimental>>& views);

  // Sets the observer to be notified of changes within this TabStrip.
  void SetTabStripObserver(TabStripObserver* observer);

  // Sets |background_offset_| and schedules a paint.
  void SetBackgroundOffset(int background_offset);

  // Scroll the tabstrip towards the trailing tabs by an offset
  void ScrollTowardsTrailingTabs(int offset);

  // Scroll the tabstrip towards the leading tabs by an offset
  void ScrollTowardsLeadingTabs(int offset);

  // Returns true if the specified rect (in TabStrip coordinates) intersects
  // the window caption area of the browser window.
  bool IsRectInWindowCaption(const gfx::Rect& rect);

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
  std::optional<TabAlertState> GetTabAlertState(int tab_index) const;

  // Updates the loading animations displayed by tabs in the tabstrip to the
  // next frame. The |elapsed_time| parameter is shared between tabs and used to
  // keep the throbbers in sync.
  void UpdateLoadingAnimations(const base::TimeDelta& elapsed_time);

  // Adds a tab at the specified index.
  void AddTabAt(int model_index, TabRendererData data);

  // Moves a tab.
  void MoveTab(int from_model_index, int to_model_index, TabRendererData data);

  // Removes a tab at the specified index. If the tab with |contents| is being
  // dragged then the drag is completed.
  void RemoveTabAt(content::WebContents* contents,
                   int model_index,
                   bool was_active);

  void OnTabWillBeRemoved(content::WebContents* contents, int model_index);

  // Sets the tab data at the specified model index.
  void SetTabData(int model_index, TabRendererData data);

  // Sets the tab group at the specified model index.
  void AddTabToGroup(std::optional<tab_groups::TabGroupId> group,
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

  // Returns whether or not strokes should be drawn around and under the tabs.
  bool ShouldDrawStrokes() const;

  // Invoked when the selection is updated.
  void SetSelection(const ui::ListSelectionModel& new_selection);

  // Invoked when a tab needs to show UI that it needs the user's attention.
  void SetTabNeedsAttention(int model_index, bool attention);

  // Returns the TabGroupHeader with ID |id|.
  TabGroupHeader* group_header(const tab_groups::TabGroupId& id) const {
    return tab_container_->GetGroupViews(id)->header();
  }

  // Returns the index of the specified view in the model coordinate system, or
  // std::nullopt if view is closing not a tab, or is not in this tabstrip.
  // TODO(tbergquist): This should return an optional<size_t>.
  std::optional<int> GetModelIndexOf(const TabSlotView* view) const;

  // Gets the number of Tabs in the tab strip.
  int GetTabCount() const;

  // Cover method for TabStripController::GetCount.
  int GetModelCount() const;

  // Returns the number of pinned tabs.
  int GetModelPinnedTabCount() const;

  TabStripController* controller() const { return controller_.get(); }

  TabDragContext* GetDragContext();

  // Returns true if Tabs in this TabStrip are currently changing size or
  // position.
  bool IsAnimating() const;

  // Stops any ongoing animations. If |layout| is true and an animation is
  // ongoing this does a layout.
  void StopAnimating(bool layout);

  // Returns the index of the focused tab, if any.
  std::optional<int> GetFocusedTabIndex() const;

  // Returns a view for anchoring an in-product help promo. |index_hint|
  // indicates at which tab the promo should be displayed, but is not
  // binding.
  views::View* GetTabViewForPromoAnchor(int index_hint);

  // Gets the default focusable child view in the TabStrip.
  views::View* GetDefaultFocusableChild();

  // TabContainerController:
  bool IsValidModelIndex(int index) const override;
  std::optional<int> GetActiveIndex() const override;
  int NumPinnedTabsInModel() const override;
  void OnDropIndexUpdate(std::optional<int> index, bool drop_before) override;
  std::optional<int> GetFirstTabInGroup(
      const tab_groups::TabGroupId& group) const override;
  gfx::Range ListTabsInGroup(
      const tab_groups::TabGroupId& group) const override;
  bool CanExtendDragHandle() const override;
  const views::View* GetTabClosingModeMouseWatcherHostView() const override;
  bool IsAnimatingInTabStrip() const override;
  void UpdateAnimationTarget(
      TabSlotView* tab_slot_view,
      gfx::Rect target_bounds_in_tab_container_coords) override;

  // TabContainerController AND TabSlotController:
  bool IsGroupCollapsed(const tab_groups::TabGroupId& group) const override;

  // TabSlotController:
  const ui::ListSelectionModel& GetSelectionModel() const override;
  Tab* tab_at(int index) const override;
  void SelectTab(Tab* tab, const ui::Event& event) override;
  void ExtendSelectionTo(Tab* tab) override;
  void ToggleSelected(Tab* tab) override;
  void AddSelectionFromAnchorTo(Tab* tab) override;
  void CloseTab(Tab* tab, CloseTabSource source) override;
  void ToggleTabAudioMute(Tab* tab) override;
  void ShiftTabNext(Tab* tab) override;
  void ShiftTabPrevious(Tab* tab) override;
  void MoveTabFirst(Tab* tab) override;
  void MoveTabLast(Tab* tab) override;
  void ToggleTabGroupCollapsedState(
      const tab_groups::TabGroupId group,
      ToggleTabGroupCollapsedStateOrigin origin =
          ToggleTabGroupCollapsedStateOrigin::kMenuAction) override;
  void NotifyTabGroupEditorBubbleOpened() override;
  void NotifyTabGroupEditorBubbleClosed() override;
  void ShowContextMenuForTab(Tab* tab,
                             const gfx::Point& p,
                             ui::MenuSourceType source_type) override;
  bool IsActiveTab(const Tab* tab) const override;
  bool IsTabSelected(const Tab* tab) const override;
  bool IsTabPinned(const Tab* tab) const override;
  bool IsTabFirst(const Tab* tab) const override;
  bool IsFocusInTabs() const override;
  bool ShouldCompactLeadingEdge() const override;

  void MaybeStartDrag(
      TabSlotView* source,
      const ui::LocatedEvent& event,
      const ui::ListSelectionModel& original_selection) override;
  [[nodiscard]] Liveness ContinueDrag(views::View* view,
                                      const ui::LocatedEvent& event) override;
  bool EndDrag(EndDragReason reason) override;
  Tab* GetTabAt(const gfx::Point& point) override;
  const Tab* GetAdjacentTab(const Tab* tab, int offset) override;
  void OnMouseEventInTab(views::View* source,
                         const ui::MouseEvent& event) override;
  void UpdateHoverCard(Tab* tab, HoverCardUpdateType update_type) override;
  bool HoverCardIsShowingForTab(Tab* tab) override;
  int GetBackgroundOffset() const override;
  int GetStrokeThickness() const override;
  bool CanPaintThrobberToLayer() const override;
  bool HasVisibleBackgroundTabShapes() const override;
  SkColor GetTabSeparatorColor() const override;
  SkColor GetTabForegroundColor(TabActive active) const override;
  std::u16string GetAccessibleTabName(const Tab* tab) const override;
  std::optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const override;
  float GetHoverOpacityForTab(float range_parameter) const override;
  float GetHoverOpacityForRadialHighlight() const override;
  std::u16string GetGroupTitle(
      const tab_groups::TabGroupId& group) const override;
  std::u16string GetGroupContentString(
      const tab_groups::TabGroupId& group) const override;
  tab_groups::TabGroupColorId GetGroupColorId(
      const tab_groups::TabGroupId& group) const override;
  SkColor GetPaintedGroupColor(
      const tab_groups::TabGroupColorId& color_id) const override;
  void ShiftGroupLeft(const tab_groups::TabGroupId& group) override;
  void ShiftGroupRight(const tab_groups::TabGroupId& group) override;
  const Browser* GetBrowser() const override;
  int GetInactiveTabWidth() const override;
  bool IsFrameCondensed() const override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool IsLockedForOnTask() override;
#endif

  // views::View:
  views::SizeBounds GetAvailableSize(const View* child) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // BrowserRootView::DropTarget:
  // These methods handle link drag & drop.
  // TODO(https://crbug.com/40828528): Use the standard views::View drag and
  // drop methods instead.
  std::optional<BrowserRootView::DropIndex> GetDropIndex(
      const ui::DropTargetEvent& event) override;
  BrowserRootView::DropTarget* GetDropTarget(
      gfx::Point loc_in_local_coords) override;
  views::View* GetViewForDrop() override;

  TabHoverCardController* hover_card_controller_for_testing() {
    return hover_card_controller_.get();
  }

 private:
  class TabDragContextImpl;

  friend class TabDragControllerTest;
  friend class TabDragContextImpl;
  friend class TabGroupEditorBubbleViewDialogBrowserTest;
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
    const raw_ptr<TabStrip> parent_;
  };

  void Init();

  std::map<tab_groups::TabGroupId, TabGroupHeader*> GetGroupHeaders();

  // Returns whether the close button should be highlighted after a remove.
  bool ShouldHighlightCloseButtonAfterRemove();

  // Returns whether the window background behind the tabstrip is transparent.
  bool TitlebarBackgroundIsTransparent() const;

  // Returns the current width of the active tab.
  int GetActiveTabWidth() const;

  // Returns the last tab in the strip that's actually visible.  This will be
  // the actual last tab unless the strip is in the overflow node_data.
  const Tab* GetLastVisibleTab() const;

  // Closes the tab at |model_index|.
  void CloseTabInternal(int model_index, CloseTabSource source);

  // Computes and stores values derived from contrast ratios.
  void UpdateContrastRatioValues();

  // Determines whether a tab can be shifted by one in the direction of |offset|
  // and moves it if possible.
  void ShiftTabRelative(Tab* tab, int offset);

  // Determines whether a group can be shifted by one in the direction of
  // |offset| and moves it if possible.
  void ShiftGroupRelative(const tab_groups::TabGroupId& group, int offset);

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnThemeChanged() override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::ViewObserver:
  void OnViewFocused(views::View* observed_view) override;
  void OnViewBlurred(views::View* observed_view) override;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  void OnTouchUiChanged();

  // Screen-reader-only announcements that depend on tab group titles.
  void AnnounceTabAddedToGroup(tab_groups::TabGroupId group_id);
  void AnnounceTabRemovedFromGroup(tab_groups::TabGroupId group_id);

  // For metrics on the best size for tab scrolling, log if the different
  // sizes would trigger tab scrolling
  void LogTabWidthsForTabScrolling();

  // -- Member Variables ------------------------------------------------------

  raw_ptr<TabStripObserver> observer_;

  std::unique_ptr<TabStripController> controller_;

  std::unique_ptr<TabHoverCardController> hover_card_controller_;

  raw_ref<TabDragContextImpl, AcrossTasksDanglingUntriaged> drag_context_;

  // The View parent for the tabs and the various group views.
  raw_ref<TabContainer, AcrossTasksDanglingUntriaged> tab_container_;

  // The background offset used by inactive tabs to match the frame image.
  int background_offset_ = 0;

  // Location of the mouse at the time of the last move.
  gfx::Point last_mouse_move_location_;

  // Used to track the time needed to create a new tab from the new tab button.
  std::optional<base::TimeTicks> new_tab_button_pressed_start_time_;

  // Used for seek time metrics from the time the mouse enters the tabstrip.
  std::optional<base::TimeTicks> mouse_entered_tabstrip_time_;

  // Used to track if the time from mouse entered to tab switch has been
  // reported.
  bool has_reported_time_mouse_entered_to_switch_ = false;

  // Used to track if the tab dragging metrics have been reported.
  bool has_reported_tab_drag_metrics_ = false;

  // Used to track the time of last tab dragging.
  std::optional<base::TimeTicks> last_tab_drag_time_;

  // Used to count the number of tab dragging in the last 30 minutes and 5
  // minutes.
  int tab_drag_count_30min_ = 0;
  int tab_drag_count_5min_ = 0;
  std::unique_ptr<base::RepeatingTimer> tab_drag_count_timer_30min_;
  std::unique_ptr<base::RepeatingTimer> tab_drag_count_timer_5min_;

  const raw_ptr<const TabStyle> style_;

  // Number of mouse moves.
  int mouse_move_count_ = 0;

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

  base::CallbackListSubscription paint_as_active_subscription_;

  const base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&TabStrip::OnTouchUiChanged,
                              base::Unretained(this)));

  TabContextMenuController context_menu_controller_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_H_

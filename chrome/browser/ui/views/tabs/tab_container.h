// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_

#include <memory>
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout_helper.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/paint_info.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

class TabStrip;
class TabGroupHeader;
class TabHoverCardController;
class TabDragContext;

// A View that contains a sequence of Tabs for the TabStrip.
class TabContainer : public views::View, public views::ViewTargeterDelegate {
 public:
  METADATA_HEADER(TabContainer);

  TabContainer(TabStripController* controller,
               TabHoverCardController* hover_card_controller,
               TabDragContext* drag_context);
  ~TabContainer() override;

  void SetAvailableWidthCallback(
      base::RepeatingCallback<int()> available_width_callback);

  Tab* AddTab(std::unique_ptr<Tab> tab, int model_index, TabPinned pinned);
  void MoveTab(Tab* tab, int from_model_index, int to_model_index);

  // Remove the tab from |tabs_view_model_|, but *not* from the View hierarchy,
  // so it can be animated closed.
  void RemoveTabFromViewModel(int index);

  void OnGroupCreated(const tab_groups::TabGroupId& group, TabStrip* tab_strip);

  // Opens the editor bubble for the tab |group| as a result of an explicit user
  // action to create the |group|.
  void OnGroupEditorOpened(const tab_groups::TabGroupId& group);

  void OnGroupMoved(const tab_groups::TabGroupId& group);

  void MoveGroupHeader(TabGroupHeader* group_header, int first_tab_model_index);

  void UpdateTabGroupVisuals(tab_groups::TabGroupId group_id);

  int GetModelIndexOf(const TabSlotView* slot_view) const;

  views::ViewModelT<Tab>* tabs_view_model() { return &tabs_view_model_; }

  Tab* GetTabAtModelIndex(int index) const;

  int GetTabCount() const;

  void UpdateHoverCard(Tab* tab,
                       TabController::HoverCardUpdateType update_type);

  // Updates the indexes and count for AX data on all tabs. Used by some screen
  // readers (e.g. ChromeVox).
  void UpdateAccessibleTabIndices();

  void HandleLongTap(ui::GestureEvent* event);

  bool IsRectInWindowCaption(const gfx::Rect& rect);

  // Animation stuff. Will be public until fully moved down into TabContainer.

  // Called whenever a tab or group header animation has progressed.
  void OnTabSlotAnimationProgressed(TabSlotView* view);

  // Animates all the views to their ideal bounds.
  // NOTE: this does *not* invoke UpdateIdealBounds, it uses the bounds
  // currently set in ideal_bounds.
  void AnimateToIdealBounds();

  // Calculates the width that can be occupied by the tabs in the container.
  // This can differ from GetAvailableWidthForTabContainer() when in tab closing
  // mode.
  int CalculateAvailableWidthForTabs() const;

  // Returns the total width available for the TabContainer's use.
  int GetAvailableWidthForTabContainer() const;

  // Teleports the tabs to their ideal bounds.
  // NOTE: this does *not* invoke UpdateIdealBounds, it uses the bounds
  // currently set in ideal_bounds.
  void SnapToIdealBounds();

  void AnimateTabClosed(Tab* tab, int former_model_index);
  void StartResetDragAnimation(int tab_model_index);

  void EnterTabClosingMode(absl::optional<int> override_width);
  void ExitTabClosingMode();

  // Sets the visibility state of all tabs and group headers (if any) based on
  // ShouldTabBeVisible().
  void SetTabSlotVisibility();

  bool in_tab_close() { return in_tab_close_; }
  absl::optional<int> override_available_width_for_tabs() {
    return override_available_width_for_tabs_;
  }

  void OnTabWillBeRemovedAt(int model_index, bool was_active);

  // TODO (1295774): Move callers down into TabContainer so this
  // encapsulation-breaking getter can be removed.
  TabStripLayoutHelper* layout_helper() const { return layout_helper_.get(); }

  std::map<tab_groups::TabGroupId, std::unique_ptr<TabGroupViews>>&
  group_views() {
    return group_views_;
  }

  views::BoundsAnimator& bounds_animator() { return bounds_animator_; }

  // views::View
  void PaintChildren(const views::PaintInfo& paint_info) override;
  gfx::Size GetMinimumSize() const override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;

  // views::ViewTargeterDelegate:
  views::View* TargetForRect(views::View* root, const gfx::Rect& rect) override;

 private:
  class RemoveTabDelegate;

  void OnTabCloseAnimationCompleted(Tab* tab);

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

  TabDragContext* drag_context_;

  // Responsible for animating tabs in response to model changes.
  views::BoundsAnimator bounds_animator_;

  std::unique_ptr<TabStripLayoutHelper> layout_helper_;

  // If this value is defined, it is used as the width to lay out tabs
  // (instead of GetAvailableWidthForTabStrip()). It is defined when closing
  // tabs with the mouse, and is used to control which tab will end up under the
  // cursor after the close animation completes.
  absl::optional<int> override_available_width_for_tabs_;

  // True if EnterTabClosingMode has been invoked. Currently, this happens when
  // a tab is closed with the mouse/touch and when collapsing a tab group. When
  // true, remove animations preserve current tab bounds, making tabs move more
  // predictably in case the user wants to perform more mouse-based actions.
  bool in_tab_close_ = false;

  base::RepeatingCallback<int()> available_width_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_

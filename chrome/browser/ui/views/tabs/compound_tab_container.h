// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMPOUND_TAB_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMPOUND_TAB_CONTAINER_H_

#include <memory>
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/tab_container.h"
#include "chrome/browser/ui/views/tabs/tab_container_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/view_targeter_delegate.h"

class TabHoverCardController;
class TabDragContextBase;

// Composes two TabContainers into one, keeping the pinned tabs in one container
// and the unpinned tabs in the other.
// Indices in the public and private API are all in overall indices, unless
// specifically noted otherwise as being relative to a specific container.
class CompoundTabContainer : public TabContainer,
                             public views::ViewTargeterDelegate {
  METADATA_HEADER(CompoundTabContainer, TabContainer)

 public:
  CompoundTabContainer(TabContainerController& controller,
                       TabHoverCardController* hover_card_controller,
                       TabDragContextBase* drag_context,
                       TabSlotController& tab_slot_controller,
                       views::View* scroll_contents_view);
  ~CompoundTabContainer() override;

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
  void OnGroupClosed(const tab_groups::TabGroupId& group) override;
  void ToggleTabGroup(const tab_groups::TabGroupId& group,
                      bool is_collapsing,
                      ToggleTabGroupCollapsedStateOrigin origin) override;
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
  gfx::Size GetMinimumSize() const override;
  views::SizeBounds GetAvailableSize(const View* child) const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  void Layout(PassKey) override;
  void PaintChildren(const views::PaintInfo& paint_info) override;
  void ChildPreferredSizeChanged(views::View* child) override;

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

  // Notifies this CompoundTabContainer that `tab_slot_view` must be animated to
  // `target_bounds`. `pinned` indicates whether these bounds are relative to
  // `pinned_tab_container_` or `unpinned_tab_container_`.
  void UpdateAnimationTarget(TabSlotView* tab_slot_view,
                             gfx::Rect target_bounds,
                             TabPinned pinned);

 private:
  int NumPinnedTabs() const;

  // Returns true iff `index` is a valid index in the joint viewmodel index
  // space of the two TabContainers, i.e. if there's a Tab view that corresponds
  // to `index`. These index spaces are different when the model has added or
  // removed a tab but we haven't finished processing that change ourselves.
  bool IsValidViewModelIndex(int index) const;

  // Moves the tab at `from_model_index` from whichever TabContainer currently
  // holds it into the other TabContainer, inserting it into that container at
  // the index that corresponds to `to_model_index`.
  void TransferTabBetweenContainers(int from_model_index, int to_model_index);

  // Converts `ideal_bounds` from `unpinned_tab_container_`'s coordinate space
  // into local coordinate space. References ideal bounds instead of current
  // bounds to correctly account for any ongoing animations in the pinned tab
  // container.
  gfx::Rect ConvertUnpinnedContainerIdealBoundsToLocal(
      gfx::Rect ideal_bounds) const;

  // Animates `tab` to `ideal_bounds` using `bounds_animator_`. Retargets an
  // existing animation if one is already running.
  void AnimateTabTo(Tab* tab, gfx::Rect ideal_bounds);

  // Returns the child TabContainer that should contain `view`. NB this can be
  // different from `view->parent()` e.g. while `view` is being dragged.
  TabContainer& GetTabContainerFor(TabSlotView* view) const;

  // Returns the child TabContainer that should handle text drag and drop events
  // at `point_in_local_coords`.
  TabContainer* GetTabContainerForDrop(gfx::Point point_in_local_coords) const;

  // Returns the child TabContainer that contains `point_in_local_coords`, or
  // nullptr if neither contain it. If both contain it, chooses based on where
  // `point_in_local_coords` is within the overlap area.
  TabContainer* GetTabContainerAt(gfx::Point point_in_local_coords) const;

  // Returns the x position that `unpinned_tab_container_` should be at after
  // any running animations finish.
  int GetUnpinnedContainerIdealLeadingX() const;

  int GetAvailableWidthForUnpinnedTabContainer() const;

  // Computes the size of this compound container assuming the pinned and
  // unpinned containers have the given sizes.
  gfx::Size GetCombinedSizeForTabContainerSizes(gfx::Size pinned_size,
                                                gfx::Size unpinned_size) const;

  // Private getter to retrieve the visible rect of the scroll container.
  std::optional<gfx::Rect> GetVisibleContentRect() const;

  // Animates and scrolls the tab container from the start_edge to the
  // target_edge. If the target_edge is beyond the tab strip it will be clamped
  // bounds of the tabstrip.
  void AnimateScrollToShowXCoordinate(const int start_edge,
                                      const int target_edge);

  const raw_ref<TabContainerController> controller_;

  // Adapts `pinned_tab_container_`'s interactions with the model to account for
  // it only holding pinned tabs.
  const std::unique_ptr<TabContainerController>
      pinned_tab_container_controller_;
  // The TabContainer that holds the pinned tabs.
  const raw_ref<TabContainer, DanglingUntriaged> pinned_tab_container_;

  // Adapts `unpinned_tab_container_`'s interactions with the model to account
  // for it only holding unpinned tabs.
  const std::unique_ptr<TabContainerController>
      unpinned_tab_container_controller_;
  // The TabContainer that holds the unpinned tabs.
  const raw_ref<TabContainer, DanglingUntriaged> unpinned_tab_container_;

  base::RepeatingCallback<int()> available_width_callback_;

  const raw_ptr<TabHoverCardController, DanglingUntriaged>
      hover_card_controller_;

  // The View that is to be scrolled by |tab_scrolling_animation_|. May be
  // nullptr in tests.
  const raw_ptr<views::View> scroll_contents_view_;

  // Responsible for animating the scroll of the tab container.
  std::unique_ptr<gfx::LinearAnimation> tab_scrolling_animation_;

  // Animates tabs between pinned and unpinned states.
  views::BoundsAnimator bounds_animator_;

  // The sub-container that handled the last drag/drop update, if any. Used to
  // ensure HandleDragExited is called when necessary.
  raw_ptr<TabContainer, DanglingUntriaged> current_text_drop_target_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMPOUND_TAB_CONTAINER_H_

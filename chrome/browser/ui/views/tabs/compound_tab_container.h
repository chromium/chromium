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

class TabHoverCardController;
class TabDragContextBase;

// Composes two TabContainers into one, keeping the pinned tabs in one container
// and the unpinned tabs in the other.
// Indices in the public and private API are all in overall indices, unless
// specifically noted otherwise as being relative to a specific container.
class CompoundTabContainer : public TabContainer {
 public:
  METADATA_HEADER(CompoundTabContainer);

  CompoundTabContainer(raw_ref<TabContainerController> controller,
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
  void SetActiveTab(absl::optional<size_t> prev_active_index,
                    absl::optional<size_t> new_active_index) override;
  std::unique_ptr<Tab> TransferTabOut(int model_index) override;
  void StoppedDraggingView(TabSlotView* view) override;
  void ScrollTabToVisible(int model_index) override;
  void ScrollTabContainerByOffset(int offset) override;
  void OnGroupCreated(const tab_groups::TabGroupId& group) override;
  void OnGroupEditorOpened(const tab_groups::TabGroupId& group) override;
  void OnGroupMoved(const tab_groups::TabGroupId& group) override;
  void OnGroupContentsChanged(const tab_groups::TabGroupId& group) override;
  void OnGroupClosed(const tab_groups::TabGroupId& group) override;
  void UpdateTabGroupVisuals(tab_groups::TabGroupId group_id) override;
  void NotifyTabGroupEditorBubbleOpened() override;
  void NotifyTabGroupEditorBubbleClosed() override;
  int GetModelIndexOf(const TabSlotView* slot_view) const override;
  Tab* GetTabAtModelIndex(int index) const override;
  int GetTabCount() const override;
  int GetModelIndexOfFirstNonClosingTab(Tab* tab) const override;
  void UpdateHoverCard(
      Tab* tab,
      TabSlotController::HoverCardUpdateType update_type) override;
  void HandleLongTap(ui::GestureEvent* event) override;
  bool IsRectInWindowCaption(const gfx::Rect& rect) override;
  void OnTabSlotAnimationProgressed(TabSlotView* view) override;
  void OnTabCloseAnimationCompleted(Tab* tab) override;
  void StartBasicAnimation() override;
  void InvalidateIdealBounds() override;
  bool IsAnimating() const override;
  void CancelAnimation() override;
  void CompleteAnimationAndLayout() override;
  int GetAvailableWidthForTabContainer() const override;
  void EnterTabClosingMode(absl::optional<int> override_width,
                           CloseTabSource source) override;
  void ExitTabClosingMode() override;
  void SetTabSlotVisibility() override;
  bool InTabClose() override;
  std::map<tab_groups::TabGroupId, std::unique_ptr<TabGroupViews>>&
  GetGroupViews() override;
  int GetActiveTabWidth() const override;
  int GetInactiveTabWidth() const override;
  gfx::Rect GetIdealBounds(int model_index) const override;
  gfx::Rect GetIdealBounds(tab_groups::TabGroupId group) const override;

  // views::View
  void Layout() override;
  void PaintChildren(const views::PaintInfo& paint_info) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize() const override;

  // BrowserRootView::DropTarget:
  BrowserRootView::DropIndex GetDropIndex(
      const ui::DropTargetEvent& event) override;
  BrowserRootView::DropTarget* GetDropTarget(
      gfx::Point loc_in_local_coords) override;
  views::View* GetViewForDrop() override;
  void HandleDragUpdate(
      const absl::optional<BrowserRootView::DropIndex>& index) override;
  void HandleDragExited() override;

 private:
  int NumPinnedTabs() const;

  // Moves the tab at `from_model_index` from whichever TabContainer currently
  // holds it into the other TabContainer, inserting it into that container at
  // the index that corresponds to `to_model_index`.
  void TransferTabBetweenContainers(int from_model_index, int to_model_index);

  TabContainer* GetTabContainerAt(gfx::Point point_in_local_coords);

  int GetAvailableWidthForUnpinnedTabContainer(
      base::RepeatingCallback<int()> available_width_callback);

  const raw_ref<TabContainerController> controller_;

  // Adapts `pinned_tab_container_`'s interactions with the model to account for
  // it only holding pinned tabs.
  const std::unique_ptr<TabContainerController>
      pinned_tab_container_controller_;
  // The TabContainer that holds the pinned tabs.
  const raw_ref<TabContainer> pinned_tab_container_;

  // Adapts `unpinned_tab_container_`'s interactions with the model to account
  // for it only holding unpinned tabs.
  const std::unique_ptr<TabContainerController>
      unpinned_tab_container_controller_;
  // The TabContainer that holds the unpinned tabs.
  const raw_ref<TabContainer> unpinned_tab_container_;

  base::RepeatingCallback<int()> available_width_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMPOUND_TAB_CONTAINER_H_

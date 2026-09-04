// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/common/dragged_tabs_container.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_z_order_manager.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class TabCollectionNode;
class TabGroupHeaderView;
class TabGroupLineView;

namespace tabs {
class TabGroupDataObserver;
}

// The view class for the tab group container. It hosts the group header,
// underline and all the tabs within the group, and serves as the drag target
// for tab dragging. Layout is managed by TabGroupViewLayout.
class TabGroupView : public TabCollectionZOrderManager,
                     public TabGroupHeaderView::Delegate,
                     public DraggedTabsContainer,
                     public TabCollectionAnimatingLayoutManager::Delegate {
  METADATA_HEADER(TabGroupView, TabCollectionZOrderManager)

 public:
  static constexpr int kTabLeadingPadding = 10;

  explicit TabGroupView(TabCollectionNode* collection_node);
  TabGroupView(const TabGroupView&) = delete;
  TabGroupView& operator=(const TabGroupView&) = delete;
  ~TabGroupView() override;

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnThemeChanged() override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // TabGroupHeaderView::Delegate:
  void ToggleCollapsedState(ToggleTabGroupCollapsedStateOrigin origin) override;
  std::unique_ptr<views::Widget> ShowGroupEditorBubble(
      bool stop_context_menu_propagation) override;
  std::u16string GetGroupContentString() const override;
  bool IsValid() const override;
  void InitHeaderDrag(const ui::LocatedEvent& event) override;
  bool ContinueHeaderDrag(const ui::LocatedEvent& event) override;
  void CancelHeaderDrag() override;
  const TabGroup& GetTabGroup() const override;
  const tabs::TabGroupData& GetTabGroupData() const override;
  void UpdateHoverCard(int update_type) const override;
  void HideHoverCard(int update_type) const override;
  bool IsFocusInTabStrip() override;
  std::unique_ptr<ExpandOnHoverLock> AcquireExpandOnHoverLock() override;
  void ShiftGroupUp() override;
  void ShiftGroupDown() override;
  bool IsGroupFocused() const override;
  HorizontalTabClosingHelper* GetTabClosingHelper() const override;

  // TabCollectionAnimatingLayoutManager::Delegate:
  bool IsDragging() const override;
  bool IsViewDragging(const views::View& child_view) const override;
  bool ShouldAnimateOpacityForAddAndRemove(
      const views::View& child_view) const override;
  bool ShouldSnapToTarget(const views::View& child_view) const override;
  std::optional<views::SizeBound> GetAvailableMainAxisSpaceOverride()
      const override;
  void OnAnimationEnded() override;

  bool IsCollapsed() const;

  std::optional<BrowserRootView::DropIndex> GetLinkDropIndex(
      const gfx::Point& point_in_local_coords);

  const TabCollectionNode* collection_node() const { return collection_node_; }
  TabStripOrientation orientation() const { return orientation_; }
  const tab_groups::TabGroupVisualData& tab_group_visual_data() const {
    return tab_group_visual_data_;
  }

  TabGroupHeaderView* group_header() { return group_header_; }
  const TabGroupHeaderView* group_header() const { return group_header_; }

  TabGroupLineView* group_line() { return group_line_; }
  const TabGroupLineView* group_line() const { return group_line_; }

  // Sets the main-axis space allocated for this group during unpinned tab
  // container layout passes.
  void SetAvailableSpace(views::SizeBound space);
  views::SizeBound available_space() const { return available_space_; }

  // Returns the minimum width for the group in horizontal orientation where all
  // child tabs have the same size. Below this width, an active tab has a larger
  // size than inactive tabs before the container scrolls.
  int GetCrossoverWidth() const;

 private:
  friend class TabGroupViewLayout;

  // DraggedTabsContainer:
  views::ScrollView* GetScrollViewForContainer() const override;
  void UpdateTargetLayoutForDrag(
      const std::vector<const views::View*>& views_to_snap) override;
  const views::ProposedLayout& GetLayoutForDrag() const override;
  const TabCollectionNode* GetCollectionNodeFromView(
      const views::View& view) const override;

  void AttachChildView(std::unique_ptr<views::View> child_view,
                       const gfx::Rect& previous_bounds_in_screen);
  std::unique_ptr<views::View> DetachChildView(views::View* child_view);

  bool is_collapsed() const { return is_collapsed_; }

  void SetIsCollapsed(bool is_collapsed);

  void ResetCollectionNode();
  void OnDataChanged();
  void UpdateChildVisibilityForCollapseState(bool collapsed);

  TabStripOrientation orientation_ = TabStripOrientation::kHorizontal;
  raw_ptr<TabCollectionNode> collection_node_ = nullptr;
  tab_groups::TabGroupVisualData tab_group_visual_data_;
  const raw_ptr<TabGroupHeaderView> group_header_ = nullptr;
  const raw_ptr<TabGroupLineView> group_line_ = nullptr;

  // The available main-axis space allocated by the parent unpinned container.
  views::SizeBound available_space_;

  // Tracked separately for layout purposes so child/underline visibility
  // updates occur only when collapse/expand animations complete, rather than
  // reacting immediately to visual data updates during animation.
  bool is_collapsed_ = false;

  const raw_ref<TabCollectionAnimatingLayoutManager> layout_manager_;

  std::unique_ptr<tabs::TabGroupDataObserver> tab_group_data_observer_;
  base::CallbackListSubscription tab_group_data_changed_subscription_;
  base::CallbackListSubscription node_destroyed_subscription_;
  base::CallbackListSubscription paint_as_active_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_VIEW_H_

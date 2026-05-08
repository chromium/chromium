// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_GROUP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_GROUP_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_dragged_tabs_container.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

class TabCollectionNode;
class VerticalTabGroupHeaderView;

namespace tabs {
class TabGroupDataObserver;
}

// The view class for vertical tab group container. It manages layout
// of the group header, underline and all the tabs within the group. It also
// handles serves as the drag target for tab dragging.
class VerticalTabGroupView
    : public views::View,
      public views::LayoutDelegate,
      public VerticalTabGroupHeaderView::Delegate,
      public VerticalDraggedTabsContainer,
      public TabCollectionAnimatingLayoutManager::Delegate {
  METADATA_HEADER(VerticalTabGroupView, views::View)

 public:
  static constexpr int kTabLeadingPadding = 10;

  explicit VerticalTabGroupView(TabCollectionNode* collection_node);
  VerticalTabGroupView(const VerticalTabGroupView&) = delete;
  VerticalTabGroupView& operator=(const VerticalTabGroupView&) = delete;
  ~VerticalTabGroupView() override;

  // views::View:
  void OnThemeChanged() override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  // VerticalTabGroupHeaderView::Delegate:
  void ToggleCollapsedState(ToggleTabGroupCollapsedStateOrigin origin) override;
  views::Widget* ShowGroupEditorBubble(
      bool stop_context_menu_propagation) override;
  std::u16string GetGroupContentString() const override;
  bool IsValid() const override;
  void InitHeaderDrag(const ui::LocatedEvent& event) override;
  bool ContinueHeaderDrag(const ui::LocatedEvent& event) override;
  void CancelHeaderDrag() override;
  const TabGroup& GetTabGroup() const override;
  void UpdateHoverCard(int update_type) const override;
  void HideHoverCard(int update_type) const override;
  bool IsFocusInTabStrip() override;
  std::unique_ptr<ExpandOnHoverLock> AcquireExpandOnHoverLock() override;
  void ShiftGroupUp() override;
  void ShiftGroupDown() override;

  // TabCollectionAnimatingLayoutManager::Delegate:
  bool IsDragging() const override;
  bool IsViewDragging(const views::View& child_view) const override;
  bool ShouldAnimateOpacityForAddAndRemove(
      const views::View& child_view) const override;
  bool ShouldSnapToTarget(const views::View& child_view) const override;
  void OnAnimationEnded() override;

  bool IsCollapsed() const;

  std::optional<BrowserRootView::DropIndex> GetLinkDropIndex(
      const gfx::Point& point_in_local_coords);

  const TabCollectionNode* collection_node() const { return collection_node_; }

  VerticalTabGroupHeaderView* group_header() { return group_header_; }
  const VerticalTabGroupHeaderView* group_header() const {
    return group_header_;
  }

 private:
  // VerticalDraggedTabsContainer:
  views::ScrollView* GetScrollViewForContainer() const override;
  void UpdateTargetLayoutForDrag(
      const std::vector<const views::View*>& views_to_snap) override;
  const views::ProposedLayout& GetLayoutForDrag() const override;
  const TabCollectionNode* GetCollectionNodeFromView(
      const views::View& view) const override;

  void AttachChildView(std::unique_ptr<views::View> child_view,
                       const gfx::Rect& previous_bounds_in_screen);
  std::unique_ptr<views::View> DetachChildView(views::View* child_view);

  void ResetCollectionNode();
  void OnDataChanged();
  void UpdateChildVisibilityForCollapseState(bool collapsed);

  raw_ptr<TabCollectionNode> collection_node_ = nullptr;

  base::CallbackListSubscription node_destroyed_subscription_;

  tab_groups::TabGroupVisualData tab_group_visual_data_;
  const raw_ptr<VerticalTabGroupHeaderView> group_header_ = nullptr;
  const raw_ptr<views::View> group_line_ = nullptr;

  const raw_ref<TabCollectionAnimatingLayoutManager> layout_manager_;

  std::unique_ptr<tabs::TabGroupDataObserver> tab_group_data_observer_;
  base::CallbackListSubscription tab_group_data_changed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_GROUP_VIEW_H_

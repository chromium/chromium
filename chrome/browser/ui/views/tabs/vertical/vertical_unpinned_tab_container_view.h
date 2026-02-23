// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_UNPINNED_TAB_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_UNPINNED_TAB_CONTAINER_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_dragged_tabs_container.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

class TabCollectionNode;
class VerticalTabGroupView;

// The view class that represents the unpinned tab region for the
// vertical tab strip. It manages the layout of the all the unpinned tabs and
// serves as the drag target for unpinned tabs which aren't grouped.
class VerticalUnpinnedTabContainerView
    : public views::View,
      public views::LayoutDelegate,
      public VerticalDraggedTabsContainer,
      public TabCollectionAnimatingLayoutManager::Delegate {
  METADATA_HEADER(VerticalUnpinnedTabContainerView, views::View)

 public:
  explicit VerticalUnpinnedTabContainerView(TabCollectionNode* collection_node);
  VerticalUnpinnedTabContainerView(const VerticalUnpinnedTabContainerView&) =
      delete;
  VerticalUnpinnedTabContainerView& operator=(
      const VerticalUnpinnedTabContainerView&) = delete;
  ~VerticalUnpinnedTabContainerView() override;

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  // views::View:
  gfx::Size GetMinimumSize() const override;

  // TabCollectionAnimatingLayoutManager::Delegate:
  bool IsViewDragging(const views::View& child_view) const override;
  bool ShouldSnapToTarget(const views::View& child_view) const override;

  // VerticalDraggedTabsContainer:
  VerticalDraggedTabsContainer& GetTabDragTarget(
      const gfx::Point& point_in_screen) override;

  std::optional<BrowserRootView::DropIndex> GetLinkDropIndex(
      const gfx::Point& point_in_local_coords);

 private:
  // VerticalDraggedTabsContainer:
  views::ScrollView* GetScrollViewForContainer() const override;
  void UpdateTargetLayoutForDrag(
      const std::vector<const views::View*>& views_to_snap) override;
  const views::ProposedLayout& GetLayoutForDrag() const override;
  void HandleTabDragInContainer(const gfx::Rect& dragged_tab_bounds) override;

  // Returns whether a drag that is currently being handled by the given
  // `group_view` should continue being handled by it.
  bool ShouldDragRemainInGroup(const VerticalTabGroupView& group_view,
                               const gfx::Rect& proposed_group_bounds,
                               const gfx::Point& point_in_screen) const;

  void ResetCollectionNode();
  bool IsTabStripCollapsed() const;

  raw_ptr<TabCollectionNode> collection_node_;
  const raw_ref<TabCollectionAnimatingLayoutManager> layout_manager_;

  base::CallbackListSubscription node_destroyed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_UNPINNED_TAB_CONTAINER_VIEW_H_

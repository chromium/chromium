// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_UNPINNED_TAB_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_UNPINNED_TAB_CONTAINER_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/common/dragged_tabs_container.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_animating_layout_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class TabCollectionNode;
class TabGroupView;
class UnpinnedTabContainerViewLayout;

// The view class that represents the unpinned tab region for the
// tab strip. It hosts all the unpinned tabs and serves as the drag target
// for unpinned tabs which aren't grouped. Layout is managed by
// UnpinnedTabContainerViewLayout.
class UnpinnedTabContainerView
    : public views::View,
      public DraggedTabsContainer,
      public TabCollectionAnimatingLayoutManager::Delegate {
  METADATA_HEADER(UnpinnedTabContainerView, views::View)
  friend class UnpinnedTabContainerViewLayout;

 public:
  explicit UnpinnedTabContainerView(TabCollectionNode* collection_node);
  UnpinnedTabContainerView(const UnpinnedTabContainerView&) = delete;
  UnpinnedTabContainerView& operator=(const UnpinnedTabContainerView&) = delete;
  ~UnpinnedTabContainerView() override;

  // TabCollectionAnimatingLayoutManager::Delegate:
  bool IsDragging() const override;
  bool IsViewDragging(const views::View& child_view) const override;
  bool ShouldSnapToTarget(const views::View& child_view) const override;
  bool ShouldAnimateOpacityForAddAndRemove(
      const views::View& child_view) const override;

  // DraggedTabsContainer:
  DraggedTabsContainer& GetTabDragTarget(
      const gfx::Point& point_in_screen) override;

  std::optional<BrowserRootView::DropIndex> GetLinkDropIndex(
      const gfx::Point& point_in_local_coords);

 private:
  // DraggedTabsContainer:
  views::ScrollView* GetScrollViewForContainer() const override;
  void UpdateTargetLayoutForDrag(
      const std::vector<const views::View*>& views_to_snap) override;
  const views::ProposedLayout& GetLayoutForDrag() const override;
  const TabCollectionNode* GetCollectionNodeFromView(
      const views::View& view) const override;

  // Returns whether a drag that is currently being handled by the given
  // `group_view` should continue being handled by it.
  bool ShouldDragRemainInGroup(const TabGroupView& group_view,
                               const gfx::Rect& proposed_group_bounds,
                               const gfx::Point& point_in_screen) const;

  void ResetCollectionNode();

  raw_ptr<TabCollectionNode> collection_node_;
  const raw_ref<TabCollectionAnimatingLayoutManager> layout_manager_;

  base::CallbackListSubscription node_destroyed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_UNPINNED_TAB_CONTAINER_VIEW_H_

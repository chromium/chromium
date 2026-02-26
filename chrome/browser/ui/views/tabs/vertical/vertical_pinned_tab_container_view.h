// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_PINNED_TAB_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_PINNED_TAB_CONTAINER_VIEW_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_dragged_tabs_container.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

class TabCollectionNode;

// The view class that represents the pinned tab region for the
// vertical tab strip. It manages the layout of all the pinned tabs and serves
// as the drag target for pinned tabs.
class VerticalPinnedTabContainerView
    : public views::View,
      public views::LayoutDelegate,
      public VerticalDraggedTabsContainer,
      public TabCollectionAnimatingLayoutManager::Delegate {
  METADATA_HEADER(VerticalPinnedTabContainerView, views::View)

 public:
  explicit VerticalPinnedTabContainerView(TabCollectionNode* collection_node);
  VerticalPinnedTabContainerView(const VerticalPinnedTabContainerView&) =
      delete;
  VerticalPinnedTabContainerView& operator=(
      const VerticalPinnedTabContainerView&) = delete;
  ~VerticalPinnedTabContainerView() override;

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  // views::View:
  gfx::Size GetMinimumSize() const override;

  // TabCollectionAnimatingLayoutManager::Delegate:
  bool IsViewDragging(const views::View& child_view) const override;

  std::optional<BrowserRootView::DropIndex> GetLinkDropIndex(
      const gfx::Point& loc_in_container);

 private:
  // VerticalDraggedTabsContainer:
  views::ScrollView* GetScrollViewForContainer() const override;
  void UpdateTargetLayoutForDrag(
      const std::vector<const views::View*>& views_to_snap) override;
  const views::ProposedLayout& GetLayoutForDrag() const override;
  void HandleTabDragInContainer(const gfx::Rect& dragged_tab_bounds) override;

  // While collapsed, only the y-coordinate is used to determine the drop
  // index, similar to the unpinned container.
  std::optional<BrowserRootView::DropIndex> GetLinkDropIndexForCollapsed(
      const gfx::Point& loc_in_container);
  // While expanded, the x-coordinate is used to determine which row the drag
  // lands on, and the y-coordinate is used to determine where within the row
  // the drop index should be.
  std::optional<BrowserRootView::DropIndex> GetLinkDropIndexForExpanded(
      const gfx::Point& loc_in_container);

  bool IsTabStripCollapsed() const;
  void ResetCollectionNode();

  raw_ptr<TabCollectionNode> collection_node_;
  const raw_ref<TabCollectionAnimatingLayoutManager> layout_manager_;

  base::CallbackListSubscription node_destroyed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_PINNED_TAB_CONTAINER_VIEW_H_

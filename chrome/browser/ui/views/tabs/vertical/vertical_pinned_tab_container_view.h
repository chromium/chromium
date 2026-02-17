// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_PINNED_TAB_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_PINNED_TAB_CONTAINER_VIEW_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_dragged_tabs_container.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

class TabCollectionNode;

// The view class that represents the pinned tab region for the
// vertical tab strip. It manages the layout of all the pinned tabs and serves
// as the drag target for pinned tabs.
class VerticalPinnedTabContainerView : public views::View,
                                       public views::LayoutDelegate,
                                       public VerticalDraggedTabsContainer {
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

 private:
  // VerticalDraggedTabsContainer:
  views::ScrollView* GetScrollViewForContainer() const override;
  void UpdateLayoutForDrag() override;
  const views::ProposedLayout& GetLayoutForDrag() const override;
  void HandleTabDragInContainer(const gfx::Rect& dragged_tab_bounds) override;

  bool IsTabStripCollapsed() const;
  void ResetCollectionNode();

  raw_ptr<TabCollectionNode> collection_node_;
  const raw_ref<TabCollectionAnimatingLayoutManager> layout_manager_;

  base::CallbackListSubscription node_destroyed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_PINNED_TAB_CONTAINER_VIEW_H_

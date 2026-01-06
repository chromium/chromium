// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_UNPINNED_TAB_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_UNPINNED_TAB_CONTAINER_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_target.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

class TabCollectionNode;

// Container for the vertical tabstrip's unpinned tabs.
class VerticalUnpinnedTabContainerView : public views::View,
                                         public views::LayoutDelegate,
                                         public TabDragTarget {
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

  // TabDragTarget
  TabDragContext* OnTabDragUpdated(TabDragTarget::DragController& controller,
                                   const gfx::Point& point_in_screen) override;
  void OnTabDragEntered() override {}
  void OnTabDragExited() override;
  void OnTabDragEnded() override;
  bool CanDropTab() override;
  void HandleTabDrop(TabDragTarget::DragController& controller) override {}
  base::CallbackListSubscription RegisterWillDestroyCallback(
      base::OnceClosure callback) override;

 private:
  void ResetCollectionNode();

  // Updates state related to dragging tabs, to be used when this container
  // starts handling a drag.
  void InitializeDragState(TabDragTarget::DragController& controller);

  // Clears drag state and removes the transformations that were being used for
  // the drag.
  void ResetDragState();

  raw_ptr<TabCollectionNode> collection_node_;
  const raw_ref<TabCollectionAnimatingLayoutManager> layout_manager_;

  base::CallbackListSubscription node_destroyed_subscription_;

  std::set<raw_ptr<views::View>> dragging_views_;
  gfx::Point last_drag_point_;

  base::OnceClosureList on_will_destroy_callback_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_UNPINNED_TAB_CONTAINER_VIEW_H_

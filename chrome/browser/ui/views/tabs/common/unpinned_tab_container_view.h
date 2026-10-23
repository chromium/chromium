// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_UNPINNED_TAB_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_UNPINNED_TAB_CONTAINER_VIEW_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/common/dragged_tabs_container.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_z_order_manager.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/view.h"

class TabCollectionNode;
class TabGroupView;

// The view class that represents the unpinned tab region for the
// tab strip. It hosts all the unpinned tabs and serves as the drag target
// for unpinned tabs which aren't grouped.
class UnpinnedTabContainerView
    : public TabCollectionZOrderManager,
      public DraggedTabsContainer,
      public TabCollectionAnimatingLayoutManager::Delegate {
  METADATA_HEADER(UnpinnedTabContainerView, TabCollectionZOrderManager)

 public:
  explicit UnpinnedTabContainerView(TabCollectionNode* collection_node);
  UnpinnedTabContainerView(const UnpinnedTabContainerView&) = delete;
  UnpinnedTabContainerView& operator=(const UnpinnedTabContainerView&) = delete;
  ~UnpinnedTabContainerView() override;

  class LayoutManager : public views::LayoutManagerBase {
   public:
    virtual int GetUnconstrainedPreferredWidth() const;

   protected:
    // views::LayoutManagerBase:
    void OnInstalled(views::View* host) override;

    // Casts host_view() to an UnpinnedTabContainerView const ref, using
    // static_cast. Avoids views::AsViewClass as it incurs overhead when
    // checking metadata.
    const class UnpinnedTabContainerView& UnpinnedTabContainerView() const;

    bool IsChildVisibleInContainer(
        std::optional<tab_groups::TabGroupId> focused_group_id,
        const views::View* child) const;
    std::optional<tab_groups::TabGroupId> GetFocusedGroupId() const;
    std::optional<tab_groups::TabGroupId> GetGroupIdForChild(
        const views::View* child) const;
  };

  // TabCollectionAnimatingLayoutManager::Delegate:
  bool IsDragging() const override;
  bool IsViewDragging(const views::View& child_view) const override;
  bool ShouldSnapToTarget(const views::View& child_view) const override;
  bool ShouldAnimateOpacityForAddAndRemove(
      const views::View& child_view) const override;
  bool ShouldAnimateLayout() override;

  // DraggedTabsContainer:
  DraggedTabsContainer& GetTabDragTarget(
      const gfx::Point& point_in_screen) override;

  std::optional<BrowserRootView::DropIndex> GetLinkDropIndex(
      const gfx::Point& point_in_local_coords);

  // Returns the unconstrained target preferred size that this container will
  // occupy once current animations complete.
  gfx::Size GetTargetPreferredSize() const;

  // Returns the total preferred width of all visible children at standard size,
  // ignoring any active tab closing mode override.
  int GetUnconstrainedPreferredWidth() const;

 private:
  friend class UnpinnedTabContainerViewVerticalLayout;
  friend class UnpinnedTabContainerViewHorizontalLayout;

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
  void OnCollapseStateChanged(tabs::VerticalTabStripCollapseState state);

  const TabStripOrientation orientation_;
  raw_ptr<TabCollectionNode> collection_node_ = nullptr;

  const raw_ref<TabCollectionAnimatingLayoutManager> layout_manager_;

  base::CallbackListSubscription node_destroyed_subscription_;
  base::CallbackListSubscription collapsed_state_changed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_UNPINNED_TAB_CONTAINER_VIEW_H_

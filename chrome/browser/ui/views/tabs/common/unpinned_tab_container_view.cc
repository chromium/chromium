// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/common/dragged_tabs_container.h"
#include "chrome/browser/ui/views/tabs/common/split_tab_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view_layout.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/view_utils.h"

namespace {

// The following are percentages used to determine the amount that dragged
// tabs must be peeking into/out of a tab group view in order for the handling
// drag container to change.
//
// If the group is handling the drag, then the dragged tabs should
// exit the group if 10% of the header's height is peeking out of the
// group.
//
// If this (the unpinned tabs container) is handling the drag, then
// the dragged tabs should enter the group if 40% of the header's height
// is peeking into the group.
//
// The latter percentage should be greater than the former
// to make it easy to place dragged tabs between two groups.
constexpr float kMinHeaderHeightPctForGroupExit = 0.1;
constexpr float kMinHeaderHeightPctForGroupEntry = 0.4;
static_assert(kMinHeaderHeightPctForGroupExit <
              kMinHeaderHeightPctForGroupEntry);

class UnpinnedTabContainerViewTargeter : public views::ViewTargeterDelegate {
 public:
  explicit UnpinnedTabContainerViewTargeter(views::View* view) : view_(view) {}
  ~UnpinnedTabContainerViewTargeter() override = default;

  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    gfx::Rect content_bounds;
    for (const views::View* child : view_->children()) {
      if (child->GetVisible()) {
        content_bounds.Union(child->GetMirroredBounds());
      }
    }
    return content_bounds.Intersects(rect);
  }

 private:
  raw_ptr<views::View> view_;
};

}  // namespace

UnpinnedTabContainerView::UnpinnedTabContainerView(
    TabCollectionNode* collection_node)
    : DraggedTabsContainer(
          static_cast<views::View&>(*this),
          collection_node,
          collection_node->orientation() == TabStripOrientation::kHorizontal
              ? DragAxes::kBoth
              : DragAxes::kVerticalOnly,
          collection_node->orientation() == TabStripOrientation::kHorizontal
              // TODO(crbug.com/523327760): Update to DragLayout::kHorizontal
              // once created.
              ? DragLayout::kSquash
              : DragLayout::kVertical),
      collection_node_(collection_node),
      layout_manager_(*SetLayoutManager(std::make_unique<
                                        TabCollectionAnimatingLayoutManager>(
          std::make_unique<UnpinnedTabContainerViewLayout>(
              collection_node->orientation()),
          /*delegate=*/*this,
          /*animation_axis=*/
          collection_node->orientation() == TabStripOrientation::kHorizontal
              ? TabCollectionAnimatingLayoutManager::AnimationAxis::kHorizontal
              : TabCollectionAnimatingLayoutManager::AnimationAxis::kVertical,
          /*animate_host_size=*/true))) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(
      std::make_unique<UnpinnedTabContainerViewTargeter>(this)));

  collection_node->set_remove_child_from_node(base::BindRepeating(
      &TabCollectionAnimatingLayoutManager::AnimateAndDestroyChildView,
      base::Unretained(&layout_manager_.get())));
  collection_node->set_attach_child_to_node(base::BindRepeating(
      &TabCollectionAnimatingLayoutManager::AnimateAndReparentView,
      base::Unretained(&layout_manager_.get())));

  node_destroyed_subscription_ = collection_node_->RegisterWillDestroyCallback(
      base::BindOnce(&UnpinnedTabContainerView::ResetCollectionNode,
                     base::Unretained(this)));
}

UnpinnedTabContainerView::~UnpinnedTabContainerView() = default;


std::optional<BrowserRootView::DropIndex>
UnpinnedTabContainerView::GetLinkDropIndex(const gfx::Point& loc_in_container) {
  if (!collection_node_) {
    return std::nullopt;
  }
  for (auto& child_node : collection_node_->children()) {
    auto* view = child_node->view();
    CHECK(view);
    if (loc_in_container.y() >= view->bounds().bottom()) {
      continue;
    }

    if (child_node->type() == TabCollectionNode::Type::GROUP) {
      auto* group_view = views::AsViewClass<TabGroupView>(view);
      if (group_view->IsCollapsed()) {
        gfx::Point loc_in_group = views::View::ConvertPointToTarget(
            this, group_view, loc_in_container);
        const bool is_leading =
            loc_in_group.y() <
            group_view->group_header()->bounds().CenterPoint().y();
        return GetDragHandler().GetLinkDropIndexForNode(
            *group_view->collection_node(),
            is_leading ? DragPositionHint::kBefore : DragPositionHint::kAfter);
      }
      // Recursive call into the group view.
      gfx::Point loc_in_group =
          views::View::ConvertPointToTarget(this, group_view, loc_in_container);
      return group_view->GetLinkDropIndex(loc_in_group);
    }

    gfx::Point loc_in_child =
        views::View::ConvertPointToTarget(this, view, loc_in_container);

    // If the drag is over the margins from the edges of the tab, then
    // consider this drag as a before/after rather than over.
    constexpr double kDragOverMargins = 0.2;
    std::optional<DragPositionHint> hint;
    if (loc_in_child.y() < view->height() * kDragOverMargins) {
      hint = DragPositionHint::kBefore;
    } else if (loc_in_child.y() > view->height() * (1 - kDragOverMargins)) {
      hint = DragPositionHint::kAfter;
    } else if (child_node->type() == TabCollectionNode::Type::SPLIT) {
      // If landing in the middle of the split, let the split view decide which
      // tab to replace.
      auto* split_view = views::AsViewClass<SplitTabView>(view);
      gfx::Point loc_in_split =
          views::View::ConvertPointToTarget(this, split_view, loc_in_container);
      return split_view->GetLinkDropIndex(loc_in_split);
    } else {
      hint = std::nullopt;
    }
    return GetDragHandler().GetLinkDropIndexForNode(*child_node, hint);
  }

  // Fallback to the end of the container.
  return GetDragHandler().GetLinkDropIndexForNode(*collection_node_,
                                                  std::nullopt);
}

bool UnpinnedTabContainerView::IsDragging() const {
  if (!collection_node_ || !collection_node_->GetController()) {
    return false;
  }
  return GetDragHandler().IsDragging();
}

bool UnpinnedTabContainerView::IsViewDragging(
    const views::View& child_view) const {
  if (!collection_node_ || !collection_node_->GetController()) {
    return false;
  }
  return GetDragHandler().IsViewDragging(child_view);
}

bool UnpinnedTabContainerView::ShouldAnimateOpacityForAddAndRemove(
    const views::View& child_view) const {
  // Only animate opacity for tab views.
  return views::IsViewClass<TabView>(&child_view);
}

bool UnpinnedTabContainerView::ShouldSnapToTarget(
    const views::View& child_view) const {
  return views::IsViewClass<SplitTabView>(&child_view);
}

void UnpinnedTabContainerView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

views::ScrollView* UnpinnedTabContainerView::GetScrollViewForContainer() const {
  return views::ScrollView::GetScrollViewForContents(
      const_cast<UnpinnedTabContainerView*>(this));
}

void UnpinnedTabContainerView::UpdateTargetLayoutForDrag(
    const std::vector<const views::View*>& views_to_snap) {
  layout_manager_->ResetViewsToTargetLayout(views_to_snap);
}
const views::ProposedLayout& UnpinnedTabContainerView::GetLayoutForDrag()
    const {
  return layout_manager_->target_layout();
}

DraggedTabsContainer& UnpinnedTabContainerView::GetTabDragTarget(
    const gfx::Point& point_in_screen) {
  if (GetDragHandler().IsDraggingGroups()) {
    // Don't consider other tab group views as a drag target if we're dragging
    // a group header already.
    return *this;
  }

  gfx::Point point_in_container =
      views::View::ConvertPointFromScreen(this, point_in_screen);
  std::optional<gfx::Rect> dragging_view_bounds =
      IsHandlingDrag() ? std::make_optional(
                             GetDraggingViewsBoundsAtPoint(point_in_container))
                       : std::nullopt;
  const views::ProposedLayout& target_layout = layout_manager_->target_layout();
  for (const views::ChildLayout& layout : target_layout.child_layouts) {
    if (!layout.visible || IsViewDragging(*layout.child_view)) {
      continue;
    }
    auto* group_view = views::AsViewClass<TabGroupView>(layout.child_view);
    if (!group_view || group_view->IsCollapsed()) {
      continue;
    }

    if (group_view->IsHandlingDrag()) {
      if (ShouldDragRemainInGroup(*group_view, layout.bounds,
                                  point_in_screen)) {
        return *group_view;
      } else {
        return *this;
      }
    }

    if (dragging_view_bounds) {
      const auto required_overlap_amount =
          group_view->group_header()->height() *
          kMinHeaderHeightPctForGroupEntry;
      if (HasMinimumOverlap(*dragging_view_bounds, layout.bounds, std::nullopt,
                            required_overlap_amount)) {
        return *group_view;
      }
    } else if (layout.bounds.y() <= point_in_container.y() &&
               layout.bounds.bottom() >= point_in_container.y()) {
      // If neither the group or this container are handling a drag and the drag
      // point falls in the group (e.g. when starting the drag), then use the
      // group.
      return *group_view;
    }
  }
  return *this;
}

bool UnpinnedTabContainerView::ShouldDragRemainInGroup(
    const TabGroupView& group_view,
    const gfx::Rect& proposed_group_bounds,
    const gfx::Point& point_in_screen) const {
  gfx::Point point_in_group =
      views::View::ConvertPointFromScreen(&group_view, point_in_screen);
  auto dragging_view_bounds_in_group =
      group_view.GetDraggingViewsBoundsAtPoint(point_in_group);
  auto dragging_view_bounds_from_group = views::View::ConvertRectToTarget(
      &group_view, this, dragging_view_bounds_in_group);

  // Note, it's possible the size of the group has not been updated to reflect
  // that a set of dragged tabs were added, meaning it's possible the height of
  // the dragged tabs is greater than the height of the group.
  // For this case, the tabs should remain in the group, even if the required
  // amount is peeking out, until the group has a chance to update its sizing.
  const auto required_overlap_amount =
      dragging_view_bounds_from_group.height() -
      (group_view.group_header()->height() * kMinHeaderHeightPctForGroupExit);

  return HasMinimumOverlap(dragging_view_bounds_from_group,
                           proposed_group_bounds, std::nullopt,
                           required_overlap_amount);
}

const TabCollectionNode* UnpinnedTabContainerView::GetCollectionNodeFromView(
    const views::View& view) const {
  if (auto* tab_view = views::AsViewClass<TabView>(&view)) {
    return tab_view->collection_node();
  } else if (auto* group_view = views::AsViewClass<TabGroupView>(&view)) {
    return group_view->collection_node();
  } else if (auto* split_tab_view = views::AsViewClass<SplitTabView>(&view)) {
    return split_tab_view->collection_node();
  }
  return nullptr;
}

BEGIN_METADATA(UnpinnedTabContainerView)
END_METADATA

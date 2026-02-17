// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_dragged_tabs_container.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/view_utils.h"

namespace {
constexpr int kTabVerticalPadding = 2;

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

class VerticalUnpinnedTabContainerViewTargeter
    : public views::ViewTargeterDelegate {
 public:
  explicit VerticalUnpinnedTabContainerViewTargeter(views::View* view)
      : view_(view) {}
  ~VerticalUnpinnedTabContainerViewTargeter() override = default;

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

VerticalUnpinnedTabContainerView::VerticalUnpinnedTabContainerView(
    TabCollectionNode* collection_node)
    : VerticalDraggedTabsContainer(static_cast<views::View&>(*this),
                                   collection_node,
                                   DragAxes::kVerticalOnly,
                                   DragLayout::kVertical),
      collection_node_(collection_node),
      layout_manager_(*SetLayoutManager(
          std::make_unique<TabCollectionAnimatingLayoutManager>(
              std::make_unique<views::DelegatingLayoutManager>(this),
              /*delegate=*/this,
              /*animation_axis=*/
              TabCollectionAnimatingLayoutManager::AnimationAxis::kVertical,
              /*animate_host_size=*/true))) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(
      std::make_unique<VerticalUnpinnedTabContainerViewTargeter>(this)));

  collection_node->set_remove_child_from_node(base::BindRepeating(
      &TabCollectionAnimatingLayoutManager::AnimateAndDestroyChildView,
      base::Unretained(&layout_manager_.get())));
  collection_node->set_attach_child_to_node(base::BindRepeating(
      &TabCollectionAnimatingLayoutManager::AnimateAndReparentView,
      base::Unretained(&layout_manager_.get())));

  node_destroyed_subscription_ = collection_node_->RegisterWillDestroyCallback(
      base::BindOnce(&VerticalUnpinnedTabContainerView::ResetCollectionNode,
                     base::Unretained(this)));
}

VerticalUnpinnedTabContainerView::~VerticalUnpinnedTabContainerView() = default;

views::ProposedLayout VerticalUnpinnedTabContainerView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  int width = 0;
  int height = 0;
  bool is_collapsed = IsTabStripCollapsed();

  const int horizontal_padding = GetLayoutConstant(
      is_collapsed ? LayoutConstant::kVerticalTabStripCollapsedPadding
                   : LayoutConstant::kVerticalTabStripUncollapsedPadding);
  const auto children = collection_node_->GetDirectChildren();

  // Layout children in order. Children will have their preferred height and
  // fill available width.
  for (auto* child : children) {
    // The leading inset should not be applied for tab groups when the tab strip
    // is collapsed since the group color line is drawn in that space.
    int x = views::AsViewClass<VerticalTabGroupView>(child) && is_collapsed
                ? 0
                : horizontal_padding;
    views::SizeBounds child_size_bounds =
        views::SizeBounds(size_bounds.width().is_bounded()
                              ? (size_bounds.width() - (x + horizontal_padding))
                              : size_bounds.width(),
                          {});
    gfx::Rect bounds = gfx::Rect(child->GetPreferredSize(child_size_bounds));
    bounds.set_x(x);

    auto drag_data = GetVisualDataForDraggedView(*child);
    CHECK(!drag_data || !drag_data->should_hide);
    bounds.set_y(drag_data ? drag_data->offset.y() : height);

    // If width is bounded, child views should respect the width constraints and
    // take up the available width excluding trailing horizontal padding.
    if (size_bounds.width().is_bounded()) {
      bounds.set_width(size_bounds.width().value() - bounds.x() -
                       horizontal_padding);
    }
    layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    height += bounds.height() + kTabVerticalPadding;
    width = std::max(width, bounds.width() + bounds.x());
  }
  // Remove excess padding if needed.
  if (!children.empty()) {
    height -= kTabVerticalPadding;
  }
  layouts.host_size = gfx::Size(width, height);
  return layouts;
}

gfx::Size VerticalUnpinnedTabContainerView::GetMinimumSize() const {
  // The minimum size should be enough to show a tab and a half, if needed.
  const int num_children = collection_node_->GetDirectChildren().size();
  const int min_height =
      base::ClampCeil(GetLayoutConstant(LayoutConstant::kVerticalTabHeight) *
                      std::min(1.5f, static_cast<float>(num_children))) +
      (num_children > 1 ? kTabVerticalPadding : 0);
  return gfx::Size(GetLayoutConstant(LayoutConstant::kVerticalTabMinWidth),
                   min_height);
}

bool VerticalUnpinnedTabContainerView::IsViewDragging(
    const views::View& child_view) const {
  if (!collection_node_ || !collection_node_->GetController()) {
    return false;
  }
  return GetDragHandler().IsViewDragging(child_view);
}

bool VerticalUnpinnedTabContainerView::ShouldSnapToTarget(
    const views::View& child_view) const {
  return views::IsViewClass<VerticalSplitTabView>(&child_view);
}

void VerticalUnpinnedTabContainerView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

bool VerticalUnpinnedTabContainerView::IsTabStripCollapsed() const {
  const auto* controller =
      collection_node_ ? collection_node_->GetController() : nullptr;
  return controller && controller->IsCollapsed();
}

views::ScrollView* VerticalUnpinnedTabContainerView::GetScrollViewForContainer()
    const {
  return views::ScrollView::GetScrollViewForContents(
      const_cast<VerticalUnpinnedTabContainerView*>(this));
}

void VerticalUnpinnedTabContainerView::UpdateLayoutForDrag() {
  layout_manager_->ResetToTargetLayout();
}

const views::ProposedLayout&
VerticalUnpinnedTabContainerView::GetLayoutForDrag() const {
  return layout_manager_->target_layout();
}

void VerticalUnpinnedTabContainerView::HandleTabDragInContainer(
    const gfx::Rect& dragged_tab_bounds) {
  const views::ProposedLayout& target_layout = layout_manager_->target_layout();
  views::View* view_at_point =
      GetViewForDragBounds(target_layout, dragged_tab_bounds);
  const TabCollectionNode* node = nullptr;
  if (auto* tab_view = views::AsViewClass<VerticalTabView>(view_at_point)) {
    node = tab_view->collection_node();
  } else if (auto* group_view =
                 views::AsViewClass<VerticalTabGroupView>(view_at_point)) {
    // Groups themselves are a drag target except when they are collapsed or
    // if we are dragging groups, which are the only cases we handle here.
    if (group_view->IsCollapsed()) {
      node = group_view->collection_node();
    } else if (GetDragHandler().IsDraggingGroups()) {
      node = group_view->collection_node();
    }
  } else if (auto* split_tab_view =
                 views::AsViewClass<VerticalSplitTabView>(view_at_point)) {
    node = split_tab_view->collection_node();
  }
  if (node) {
    GetDragHandler().HandleDraggedTabsOverNode(*node, std::nullopt);
  }
}

VerticalDraggedTabsContainer&
VerticalUnpinnedTabContainerView::GetTabDragTarget(
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
    auto* group_view =
        views::AsViewClass<VerticalTabGroupView>(layout.child_view);
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

bool VerticalUnpinnedTabContainerView::ShouldDragRemainInGroup(
    const VerticalTabGroupView& group_view,
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

BEGIN_METADATA(VerticalUnpinnedTabContainerView)
END_METADATA

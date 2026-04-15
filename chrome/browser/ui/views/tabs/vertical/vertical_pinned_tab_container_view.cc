// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_dragged_tabs_container.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {
constexpr int kTabPadding = 4;
}  // namespace

VerticalPinnedTabContainerView::VerticalPinnedTabContainerView(
    TabCollectionNode* collection_node)
    : VerticalDraggedTabsContainer(static_cast<views::View&>(*this),
                                   collection_node,
                                   DragAxes::kBoth,
                                   DragLayout::kSquash),
      collection_node_(collection_node),
      layout_manager_(*SetLayoutManager(std::make_unique<
                                        TabCollectionAnimatingLayoutManager>(
          std::make_unique<views::DelegatingLayoutManager>(this),
          *this,
          TabCollectionAnimatingLayoutManager::AnimationAxis::kHorizontal))) {
  collection_node->set_remove_child_from_node(base::BindRepeating(
      &TabCollectionAnimatingLayoutManager::AnimateAndDestroyChildView,
      base::Unretained(base::to_address(layout_manager_))));
  collection_node->set_attach_child_to_node(base::BindRepeating(
      &TabCollectionAnimatingLayoutManager::AnimateAndReparentView,
      base::Unretained(&layout_manager_.get())));

  node_destroyed_subscription_ = collection_node_->RegisterWillDestroyCallback(
      base::BindOnce(&VerticalPinnedTabContainerView::ResetCollectionNode,
                     base::Unretained(this)));
}

VerticalPinnedTabContainerView::~VerticalPinnedTabContainerView() = default;

views::ProposedLayout VerticalPinnedTabContainerView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  int total_width = 0;
  int total_height = 0;

  const std::vector<views::View*> children =
      collection_node_ ? collection_node_->GetDirectChildren()
                       : std::vector<views::View*>();

  int x = 0;
  int y = 0;
  int children_on_row = children.size();

  if (children_on_row == 0) {
    layouts.host_size = gfx::Size(0, 0);
    return layouts;
  }

  // Child width will be uniform and match the largest child's width.
  bool contains_split = false;
  for (const auto& i : collection_node_->children()) {
    if (i->type() == TabCollectionNode::Type::SPLIT) {
      contains_split = true;
    }
  }
  int child_width = GetLayoutConstant(LayoutConstant::kVerticalTabMinWidth) *
                    (contains_split ? 2 : 1);

  // If the width is bounded, calculate how many children can fit on a row.
  // Since all children are allocated the same width this will be the same for
  // every row.
  if (size_bounds.width().is_bounded() && size_bounds.width().value() > 0) {
    auto collapse_state = GetTabStripCollapseState();

    // Apply horizontal padding immediately at start of collapse animation by
    // including collapsing state.
    const int region_horizontal_padding = GetLayoutConstant(
        collapse_state != tabs::VerticalTabStripCollapseState::kExpanded
            ? LayoutConstant::kVerticalTabStripCollapsedHorizontalPadding
            : LayoutConstant::kVerticalTabStripUncollapsedPadding);
    int available_width =
        size_bounds.width().value() - region_horizontal_padding;

    // When we are in collapsed state, only one child should be shown per row.
    // During collapse animation and other cases, fit as many as possible.
    children_on_row =
        tabs::IsVerticalTabsExpandOnHoverFeatureEnabled() &&
                collapse_state ==
                    tabs::VerticalTabStripCollapseState::kCollapsed
            ? 1
            : std::min(
                  children_on_row,
                  static_cast<int>(std::floor((available_width - child_width) /
                                              (child_width + kTabPadding)) +
                                   1));

    // Allocate extra space to the tabs.
    available_width -=
        (children_on_row * child_width) + (kTabPadding * (children_on_row - 1));
    child_width += std::floor(available_width / children_on_row);
  }

  int row_index = 0;
  for (auto* child : children) {
    gfx::Rect bounds =
        gfx::Rect(child->GetPreferredSize(views::SizeBounds(child_width, {})));
    bounds.set_width(child_width);

    auto drag_data = GetVisualDataForDraggedView(*child);
    const bool should_show_child = !(drag_data && drag_data->should_hide);
    if (!should_show_child) {
      layouts.child_layouts.emplace_back(child, false, bounds);
      continue;
    }

    bounds.set_y(drag_data ? drag_data->offset.y() : y);
    int child_x = drag_data ? drag_data->offset.x() : x;
    if (drag_data && base::i18n::IsRTL()) {
      child_x = size_bounds.width().value() - child_x - child_width;
    }
    bounds.set_x(child_x);

    if (row_index != 0) {
      bounds.set_x(bounds.x() + kTabPadding);
    }

    if (!drag_data || !drag_data->should_float) {
      if (row_index != 0) {
        x += kTabPadding;
      }
      x += bounds.width();
      total_width = std::max(total_width, x);
      total_height = std::max(total_height, (y + bounds.height()));

      row_index++;
      if (row_index >= children_on_row) {
        y = total_height + kTabPadding;
        row_index = 0;
        x = 0;
      }
    }

    layouts.child_layouts.emplace_back(child, true, bounds);
  }

  // Make sure we snap to bounded width if defined. This is necessary as the
  // `child_width` calculation above rounds width down and this can result in
  // off-by-one width calculations when the number of children on a row changes.
  // Changes in host width can be interpreted as a resize and animations may
  // otherwise snap to target.
  layouts.host_size =
      gfx::Size(size_bounds.width().value_or(total_width), total_height);
  return layouts;
}

gfx::Size VerticalPinnedTabContainerView::GetMinimumSize() const {
  if (!collection_node_) {
    return gfx::Size();
  }

  // The minimum size should be enough to show a row and a half, if needed.
  auto collapse_state = GetTabStripCollapseState();
  const int num_children = collection_node_->GetDirectChildren().size();
  const float min_rows = std::min(
      (collapse_state != tabs::VerticalTabStripCollapseState::kExpanded ? 1.5f
                                                                        : 1.0f),
      static_cast<float>(num_children));
  const int min_height =
      base::ClampCeil(GetLayoutConstant(LayoutConstant::kVerticalTabHeight) *
                      min_rows) +
      (num_children > 1 ? kTabPadding : 0);
  return gfx::Size(GetLayoutConstant(LayoutConstant::kVerticalTabMinWidth),
                   min_height);
}

bool VerticalPinnedTabContainerView::IsDragging() const {
  if (!collection_node_ || !collection_node_->GetController()) {
    return false;
  }
  return GetDragHandler().IsDragging();
}

bool VerticalPinnedTabContainerView::IsViewDragging(
    const views::View& child_view) const {
  if (!collection_node_ || !collection_node_->GetController()) {
    return false;
  }
  return GetDragHandler().IsViewDragging(child_view);
}

bool VerticalPinnedTabContainerView::ShouldAnimateOpacityForAddAndRemove(
    const views::View& child_view) const {
  // Only animate opacity for tab views.
  return views::IsViewClass<VerticalTabView>(&child_view);
}

bool VerticalPinnedTabContainerView::ShouldSnapToTarget(
    const views::View& child_view) const {
  return views::IsViewClass<VerticalSplitTabView>(&child_view);
}

std::optional<BrowserRootView::DropIndex>
VerticalPinnedTabContainerView::GetLinkDropIndex(
    const gfx::Point& loc_in_container) {
  if (!collection_node_ || collection_node_->children().size() == 0) {
    return std::nullopt;
  }

  auto collapse_state = GetTabStripCollapseState();
  if (collapse_state != tabs::VerticalTabStripCollapseState::kExpanded) {
    if (auto index = GetLinkDropIndexForCollapsed(loc_in_container)) {
      return index;
    }
  } else if (auto index = GetLinkDropIndexForExpanded(loc_in_container)) {
    return index;
  }

  // Fallback to the end of the pinned container.
  return GetDragHandler().GetLinkDropIndexForNode(
      *collection_node_->children().back(), DragPositionHint::kAfter);
}

std::optional<BrowserRootView::DropIndex>
VerticalPinnedTabContainerView::GetLinkDropIndexForCollapsed(
    const gfx::Point& loc_in_container) {
  // While collapsed, simply use the y-coordinate, similar to the unpinned
  // container.
  for (auto& child_node : collection_node_->children()) {
    auto* view = child_node->view();
    CHECK(view);
    if (loc_in_container.y() >= view->bounds().bottom()) {
      continue;
    }

    gfx::Point loc_in_child =
        views::View::ConvertPointToTarget(this, view, loc_in_container);
    constexpr double kDragOverMargins = 0.2;
    std::optional<DragPositionHint> hint;

    // Determine whether the drag is above, below, or above the tab/tab split.
    if (loc_in_child.y() < view->height() * kDragOverMargins) {
      hint = DragPositionHint::kBefore;
    } else if (loc_in_child.y() > view->height() * (1 - kDragOverMargins)) {
      hint = DragPositionHint::kAfter;
    } else if (child_node->type() == TabCollectionNode::Type::SPLIT) {
      // If landing in the middle of the split, let the split view decide
      // which tab to replace.
      auto* split_view = views::AsViewClass<VerticalSplitTabView>(view);
      gfx::Point loc_in_split =
          views::View::ConvertPointToTarget(this, split_view, loc_in_container);
      return split_view->GetLinkDropIndex(loc_in_split);
    } else {
      hint = std::nullopt;
    }
    return GetDragHandler().GetLinkDropIndexForNode(*child_node, hint);
  }

  return std::nullopt;
}

std::optional<BrowserRootView::DropIndex>
VerticalPinnedTabContainerView::GetLinkDropIndexForExpanded(
    const gfx::Point& loc_in_container) {
  const int logical_x = GetMirroredXInView(loc_in_container.x());
  for (auto& child_node : collection_node_->children()) {
    auto* view = child_node->view();
    CHECK(view);
    // Check if the current point is within the vertical bounds of the row
    // this child belongs to, including half the padding between rows.
    if (loc_in_container.y() >= view->bounds().bottom() + kTabPadding / 2) {
      continue;
    }

    // If the point is to the right of the child, then let the next child
    // be the candidate.
    // The full padding amount is used here, rather than half as done above,
    // so that this correctly accounts for the last tab in the row.
    // The x-based calculation uses a margin to determine if the link should
    // be inserted before/after, so the cutoff point between tabs in a row
    // doesn't have to be exact.
    if (logical_x >= view->bounds().right() + kTabPadding) {
      continue;
    }

    gfx::Point loc_in_child =
        views::View::ConvertPointToTarget(this, view, loc_in_container);

    constexpr double kDragOverMargins = 0.2;
    std::optional<DragPositionHint> hint;

    const int logical_x_in_child = view->GetMirroredXInView(loc_in_child.x());

    // Determine whether the drag is to the right, left, or above the tab/tab
    // split.
    if (logical_x_in_child < view->width() * kDragOverMargins) {
      hint = DragPositionHint::kBefore;
    } else if (logical_x_in_child > view->width() * (1 - kDragOverMargins)) {
      hint = DragPositionHint::kAfter;
    } else if (child_node->type() == TabCollectionNode::Type::SPLIT) {
      // If landing in the middle of the split, let the split view decide
      // which tab to replace.
      auto* split_view = views::AsViewClass<VerticalSplitTabView>(view);
      gfx::Point loc_in_split =
          views::View::ConvertPointToTarget(this, split_view, loc_in_container);
      return split_view->GetLinkDropIndex(loc_in_split);
    } else {
      hint = std::nullopt;
    }
    return GetDragHandler().GetLinkDropIndexForNode(*child_node, hint);
  }

  return std::nullopt;
}

void VerticalPinnedTabContainerView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

views::ScrollView* VerticalPinnedTabContainerView::GetScrollViewForContainer()
    const {
  return views::ScrollView::GetScrollViewForContents(
      const_cast<VerticalPinnedTabContainerView*>(this));
}

void VerticalPinnedTabContainerView::UpdateTargetLayoutForDrag(
    const std::vector<const views::View*>& views_to_snap) {
  layout_manager_->ResetViewsToTargetLayout(views_to_snap);
}

const views::ProposedLayout& VerticalPinnedTabContainerView::GetLayoutForDrag()
    const {
  return layout_manager_->target_layout();
}

const TabCollectionNode*
VerticalPinnedTabContainerView::GetCollectionNodeFromView(
    const views::View& view) const {
  if (auto* tab_view = views::AsViewClass<VerticalTabView>(&view)) {
    return tab_view->collection_node();
  } else if (auto* split_tab_view =
                 views::AsViewClass<VerticalSplitTabView>(&view)) {
    return split_tab_view->collection_node();
  }
  return nullptr;
}

BEGIN_METADATA(VerticalPinnedTabContainerView)
END_METADATA

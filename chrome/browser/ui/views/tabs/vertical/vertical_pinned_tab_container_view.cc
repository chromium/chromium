// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/layout_constants.h"
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
          this,
          TabCollectionAnimatingLayoutManager::AnimationAxis::kHorizontal))) {
  collection_node->set_remove_child_from_node(base::BindRepeating(
      &TabCollectionAnimatingLayoutManager::AnimateAndDestroyChildView,
      base::Unretained(base::to_address(layout_manager_))));

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

  const auto children = collection_node_->GetDirectChildren();

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
    bool is_collapsed = IsTabStripCollapsed();
    const int region_horizontal_padding = GetLayoutConstant(
        is_collapsed ? LayoutConstant::kVerticalTabStripCollapsedPadding
                     : LayoutConstant::kVerticalTabStripUncollapsedPadding);
    int available_width =
        size_bounds.width().value() - region_horizontal_padding;

    children_on_row =
        std::min(children_on_row,
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
  layouts.host_size = gfx::Size(total_width, total_height);
  return layouts;
}

gfx::Size VerticalPinnedTabContainerView::GetMinimumSize() const {
  // The minimum size should be enough to show a row and a half, if needed.
  const int num_children = collection_node_->GetDirectChildren().size();
  const float min_rows = std::min((IsTabStripCollapsed() ? 1.5f : 1.0f),
                                  static_cast<float>(num_children));
  const int min_height =
      base::ClampCeil(GetLayoutConstant(LayoutConstant::kVerticalTabHeight) *
                      min_rows) +
      (num_children > 1 ? kTabPadding : 0);
  return gfx::Size(GetLayoutConstant(LayoutConstant::kVerticalTabMinWidth),
                   min_height);
}

bool VerticalPinnedTabContainerView::IsViewDragging(
    const views::View& child_view) const {
  if (!collection_node_ || !collection_node_->GetController()) {
    return false;
  }
  return GetDragHandler().IsViewDragging(child_view);
}

std::optional<BrowserRootView::DropIndex>
VerticalPinnedTabContainerView::GetLinkDropIndex(
    const gfx::Point& loc_in_container) {
  if (!collection_node_) {
    return std::nullopt;
  }

  if (IsTabStripCollapsed()) {
    if (auto index = GetLinkDropIndexForCollapsed(loc_in_container)) {
      return index;
    }
  } else if (auto index = GetLinkDropIndexForExpanded(loc_in_container)) {
    return index;
  }

  // Fallback to the end of the container.
  return GetDragHandler().GetLinkDropIndexForNode(*collection_node_,
                                                  std::nullopt);
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
      auto* split_view = static_cast<VerticalSplitTabView*>(view);
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
    if (loc_in_container.x() >= view->bounds().right() + kTabPadding) {
      continue;
    }

    gfx::Point loc_in_child =
        views::View::ConvertPointToTarget(this, view, loc_in_container);

    constexpr double kDragOverMargins = 0.2;
    std::optional<DragPositionHint> hint;

    // Determine whether the drag is to the right, left, or above the tab/tab
    // split.
    if (loc_in_child.x() < view->width() * kDragOverMargins) {
      hint = DragPositionHint::kBefore;
    } else if (loc_in_child.x() > view->width() * (1 - kDragOverMargins)) {
      hint = DragPositionHint::kAfter;
    } else if (child_node->type() == TabCollectionNode::Type::SPLIT) {
      // If landing in the middle of the split, let the split view decide
      // which tab to replace.
      auto* split_view = static_cast<VerticalSplitTabView*>(view);
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

bool VerticalPinnedTabContainerView::IsTabStripCollapsed() const {
  const auto* controller =
      collection_node_ ? collection_node_->GetController() : nullptr;
  return controller && controller->IsCollapsed();
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

void VerticalPinnedTabContainerView::HandleTabDragInContainer(
    const gfx::Rect& dragged_tab_bounds) {
  const views::ProposedLayout& target_layout = layout_manager_->target_layout();
  views::View* view_at_point =
      GetViewForDragBounds(target_layout, dragged_tab_bounds);
  const TabCollectionNode* node = nullptr;
  if (auto* tab_view = views::AsViewClass<VerticalTabView>(view_at_point)) {
    node = tab_view->collection_node();
  } else if (auto* split_tab_view =
                 views::AsViewClass<VerticalSplitTabView>(view_at_point)) {
    node = split_tab_view->collection_node();
  }
  if (node) {
    GetDragHandler().HandleDraggedTabsOverNode(*node, std::nullopt);
    // Synchronously force a layout here to update the target layout. Since all
    // the calculations are based off on target layout, we need to ensure it is
    // updated where there are model change.
    DeprecatedLayoutImmediately();
  }
}

BEGIN_METADATA(VerticalPinnedTabContainerView)
END_METADATA

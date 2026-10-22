// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/pinned_tab_container_view_vertical_layout.h"

#include <algorithm>
#include <cmath>

#include "base/numerics/safe_conversions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/common/pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"

PinnedTabContainerViewVerticalLayout::PinnedTabContainerViewVerticalLayout() =
    default;
PinnedTabContainerViewVerticalLayout::~PinnedTabContainerViewVerticalLayout() =
    default;

gfx::Size PinnedTabContainerViewVerticalLayout::GetMinimumSize(
    const views::View* host) const {
  if (!PinnedTabContainerView().collection_node_ ||
      PinnedTabContainerView().collection_node_->GetDirectChildren().empty()) {
    return gfx::Size();
  }

  // The minimum size should be enough to show a row and a half, if needed.
  auto collapse_state = PinnedTabContainerView().GetTabStripCollapseState();
  const int num_children =
      PinnedTabContainerView().collection_node_->GetDirectChildren().size();
  const float min_rows = std::min(
      (collapse_state != tabs::VerticalTabStripCollapseState::kExpanded ? 1.5f
                                                                        : 1.0f),
      static_cast<float>(num_children));
  const int min_height =
      base::ClampCeil(GetLayoutConstant(LayoutConstant::kVerticalTabHeight) *
                      min_rows) +
      (num_children > 1 ? PinnedTabContainerView::kTabPadding : 0);
  return gfx::Size(GetLayoutConstant(LayoutConstant::kVerticalTabMinWidth),
                   min_height);
}

views::ProposedLayout
PinnedTabContainerViewVerticalLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  const auto children =
      PinnedTabContainerView().collection_node_
          ? PinnedTabContainerView().collection_node_->GetDirectChildren()
          : TabCollectionNode::ChildViews();

  int total_width = 0;
  int total_height = 0;

  int x = 0;
  int y = 0;
  int children_on_row = children.size();

  if (children_on_row == 0) {
    layouts.host_size = gfx::Size(0, 0);
    return layouts;
  }

  // Child width will be uniform and match the largest child's width.
  bool contains_split = false;
  for (const auto& i : PinnedTabContainerView().collection_node_->children()) {
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
    auto collapse_state = PinnedTabContainerView().GetTabStripCollapseState();

    // Apply horizontal padding immediately at start of collapse animation by
    // including collapsing state.
    int available_width =
        size_bounds.width().value() -
        GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding);

    // When we are in collapsed state, only one child should be shown per row.
    // During collapse animation and other cases, fit as many as possible.
    children_on_row =
        collapse_state == tabs::VerticalTabStripCollapseState::kCollapsed
            ? 1
            : std::min(children_on_row,
                       static_cast<int>(
                           std::floor((available_width - child_width) /
                                      (child_width +
                                       PinnedTabContainerView::kTabPadding)) +
                           1));

    // Allocate extra space to the tabs.
    available_width -=
        (children_on_row * child_width) +
        (PinnedTabContainerView::kTabPadding * (children_on_row - 1));
    child_width += std::floor(available_width / children_on_row);
  }

  int row_index = 0;
  for (auto* child : children) {
    gfx::Rect bounds =
        gfx::Rect(child->GetPreferredSize(views::SizeBounds(child_width, {})));
    bounds.set_width(child_width);

    auto drag_data =
        PinnedTabContainerView().GetVisualDataForDraggedView(*child);
    const bool should_show_child = !(drag_data && drag_data->should_hide);
    if (!should_show_child) {
      layouts.child_layouts.emplace_back(child, false, gfx::Rect());
      continue;
    }

    bounds.set_y(drag_data ? drag_data->offset.y() : y);
    bounds.set_x(drag_data ? drag_data->offset.x() : x);

    if (row_index != 0) {
      bounds.set_x(bounds.x() + PinnedTabContainerView::kTabPadding);
    }

    if (!drag_data || !drag_data->should_float) {
      if (row_index != 0) {
        x += PinnedTabContainerView::kTabPadding;
      }
      x += bounds.width();
      total_width = std::max(total_width, x);
      total_height = std::max(total_height, (y + bounds.height()));

      row_index++;
      if (row_index >= children_on_row) {
        y = total_height + PinnedTabContainerView::kTabPadding;
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

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view_vertical_layout.h"

#include <algorithm>
#include <optional>

#include "base/numerics/safe_conversions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {
constexpr int kTabVerticalPadding = 2;
}  // namespace

UnpinnedTabContainerViewVerticalLayout::
    UnpinnedTabContainerViewVerticalLayout() = default;
UnpinnedTabContainerViewVerticalLayout::
    ~UnpinnedTabContainerViewVerticalLayout() = default;

gfx::Size UnpinnedTabContainerViewVerticalLayout::GetMinimumSize(
    const views::View* host) const {
  if (!UnpinnedTabContainerView().collection_node_) {
    return gfx::Size();
  }

  std::optional<tab_groups::TabGroupId> focused_group_id = GetFocusedGroupId();

  int num_children = 0;
  for (const auto* child :
       UnpinnedTabContainerView().collection_node_->GetDirectChildren()) {
    bool should_show = true;
    if (focused_group_id.has_value()) {
      std::optional<tab_groups::TabGroupId> group_id =
          GetGroupIdForChild(child);
      if (group_id != focused_group_id.value()) {
        should_show = false;
      }
    }
    if (should_show) {
      num_children++;
    }
  }

  const int min_height =
      base::ClampCeil(GetLayoutConstant(LayoutConstant::kVerticalTabHeight) *
                      std::min(1.5f, static_cast<float>(num_children))) +
      (num_children > 1 ? kTabVerticalPadding : 0);
  return gfx::Size(GetLayoutConstant(LayoutConstant::kVerticalTabMinWidth),
                   min_height);
}

views::ProposedLayout
UnpinnedTabContainerViewVerticalLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!UnpinnedTabContainerView().collection_node_) {
    return layouts;
  }

  std::optional<tab_groups::TabGroupId> focused_group_id = GetFocusedGroupId();

  const auto children =
      UnpinnedTabContainerView().collection_node_->GetDirectChildren();

  int width = 0;
  int height = 0;
  int dragged_view_bottom = 0;
  auto collapse_state = UnpinnedTabContainerView().GetTabStripCollapseState();

  const int horizontal_padding =
      GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding);

  for (auto* child : children) {
    int x =
        views::AsViewClass<TabGroupView>(child) &&
                collapse_state != tabs::VerticalTabStripCollapseState::kExpanded
            ? 0
            : horizontal_padding;
    views::SizeBound child_height_bound = size_bounds.height();
    // In focused mode, pass the scroll viewport height constraint to the child
    // focused group if the incoming size bounds height is unbounded, so that
    // TabGroupView can clamp dragged view bounds during a drag operation.
    if (focused_group_id.has_value() && !child_height_bound.is_bounded()) {
      if (const auto* scroll_view =
              UnpinnedTabContainerView().GetScrollViewForContainer()) {
        child_height_bound = scroll_view->height();
      }
    }
    views::SizeBounds child_size_bounds =
        views::SizeBounds(size_bounds.width().is_bounded()
                              ? (size_bounds.width() - (x + horizontal_padding))
                              : size_bounds.width(),
                          child_height_bound);
    gfx::Rect bounds = gfx::Rect(child->GetPreferredSize(child_size_bounds));
    bounds.set_x(x);

    auto drag_data =
        UnpinnedTabContainerView().GetVisualDataForDraggedView(*child);
    bool should_show_child = IsChildVisibleInContainer(focused_group_id, child);

    if (!should_show_child) {
      layouts.child_layouts.emplace_back(
          child, false,
          gfx::Rect(x, drag_data ? drag_data->offset.y() : height,
                    bounds.width(), 0));
      continue;
    }

    bounds.set_y(drag_data ? drag_data->offset.y() : height);

    if (size_bounds.width().is_bounded()) {
      bounds.set_width(size_bounds.width().value() - bounds.x() -
                       horizontal_padding);
    }
    layouts.child_layouts.emplace_back(child, true, bounds);
    height += bounds.height() + kTabVerticalPadding;
    width = std::max(width, bounds.width() + bounds.x());
  }

  if (height > 0) {
    height -= kTabVerticalPadding;
  }

  if (UnpinnedTabContainerView().IsHandlingDrag()) {
    dragged_view_bottom =
        UnpinnedTabContainerView().GetDraggingViewsBounds().bottom();
    if (size_bounds.height().is_bounded()) {
      dragged_view_bottom =
          std::min(dragged_view_bottom, size_bounds.height().value());
    }
  }
  layouts.host_size = gfx::Size(width, std::max(height, dragged_view_bottom));
  return layouts;
}

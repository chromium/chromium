// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view_layout.h"

#include <algorithm>
#include <vector>

#include "base/numerics/safe_conversions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_utils.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "components/tabs/public/tab_group.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {
constexpr int kTabVerticalPadding = 2;
}  // namespace

UnpinnedTabContainerViewLayout::UnpinnedTabContainerViewLayout(
    TabStripOrientation orientation)
    : orientation_(orientation) {}
UnpinnedTabContainerViewLayout::~UnpinnedTabContainerViewLayout() = default;

views::ProposedLayout UnpinnedTabContainerViewLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  const UnpinnedTabContainerView* tab_container_view =
      views::AsViewClass<UnpinnedTabContainerView>(host_view());
  if (!tab_container_view) {
    return views::ProposedLayout();
  }

  if (orientation_ == TabStripOrientation::kHorizontal) {
    return CalculateHorizontalLayout(tab_container_view, size_bounds);
  }
  return CalculateVerticalLayout(tab_container_view, size_bounds);
}

gfx::Size UnpinnedTabContainerViewLayout::GetMinimumSize(
    const views::View* host) const {
  const UnpinnedTabContainerView* tab_container_view =
      views::AsViewClass<UnpinnedTabContainerView>(host);
  if (!tab_container_view) {
    return gfx::Size();
  }

  if (orientation_ == TabStripOrientation::kHorizontal) {
    return CalculateHorizontalMinimumSize(tab_container_view);
  }
  return CalculateVerticalMinimumSize(tab_container_view);
}

views::ProposedLayout UnpinnedTabContainerViewLayout::CalculateHorizontalLayout(
    const UnpinnedTabContainerView* tab_container_view,
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!tab_container_view->collection_node_) {
    return layouts;
  }

  std::optional<tab_groups::TabGroupId> focused_group_id =
      GetFocusedGroupId(tab_container_view);

  const std::vector<views::View*> children =
      tab_container_view->collection_node_->GetDirectChildren();
  if (children.empty()) {
    return layouts;
  }

  std::vector<int> child_preferred_widths;
  std::vector<int> child_min_widths;
  int total_preferred_width = 0;
  int total_min_width = 0;

  for (views::View* child : children) {
    int child_pref_width =
        child->GetPreferredSize(views::SizeBounds({}, size_bounds.height()))
            .width();
    int child_min_width = child->GetMinimumSize().width();

    child_preferred_widths.push_back(child_pref_width);
    child_min_widths.push_back(child_min_width);
    total_preferred_width += child_pref_width;
    total_min_width += child_min_width;
  }

  int available_width = total_preferred_width;
  if (size_bounds.width().is_bounded()) {
    available_width = size_bounds.width().value();
  }

  int computed_width = total_preferred_width;
  if (available_width > 0) {
    computed_width =
        std::clamp(available_width, total_min_width, total_preferred_width);
  }

  std::vector<int> allocated_widths = CalculateProportionalChildWidths(
      computed_width, child_preferred_widths, child_min_widths,
      total_preferred_width, total_min_width);

  int x = 0;
  const int container_height = size_bounds.height().value_or(
      GetLayoutConstant(LayoutConstant::kTabHeight));

  for (size_t i = 0; i < children.size(); ++i) {
    views::View* child = children[i];
    int child_width = allocated_widths[i];

    auto drag_data = tab_container_view->GetVisualDataForDraggedView(*child);
    bool should_show_child = !(drag_data && drag_data->should_hide);

    if (should_show_child && focused_group_id.has_value()) {
      std::optional<tab_groups::TabGroupId> group_id =
          GetGroupIdForChild(child);
      if (group_id != focused_group_id.value()) {
        should_show_child = false;
      }
    }

    if (!should_show_child) {
      layouts.child_layouts.emplace_back(
          child, false,
          gfx::Rect(drag_data ? drag_data->offset.x() : x, 0, 0,
                    container_height));
      continue;
    }

    gfx::Rect bounds(x, 0, child_width, container_height);
    bounds.set_x(drag_data ? drag_data->offset.x() : x);

    layouts.child_layouts.emplace_back(child, true, bounds);
    x += bounds.width();
  }

  layouts.host_size = gfx::Size(x, container_height);
  return layouts;
}

views::ProposedLayout UnpinnedTabContainerViewLayout::CalculateVerticalLayout(
    const UnpinnedTabContainerView* tab_container_view,
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!tab_container_view->collection_node_) {
    return layouts;
  }

  std::optional<tab_groups::TabGroupId> focused_group_id =
      GetFocusedGroupId(tab_container_view);

  const std::vector<views::View*> children =
      tab_container_view->collection_node_->GetDirectChildren();

  int width = 0;
  int height = 0;
  int dragged_view_bottom = 0;
  auto collapse_state = tab_container_view->GetTabStripCollapseState();

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
              tab_container_view->GetScrollViewForContainer()) {
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

    auto drag_data = tab_container_view->GetVisualDataForDraggedView(*child);
    bool should_show_child = !(drag_data && drag_data->should_hide);

    if (should_show_child && focused_group_id.has_value()) {
      std::optional<tab_groups::TabGroupId> group_id =
          GetGroupIdForChild(child);
      if (group_id != focused_group_id.value()) {
        should_show_child = false;
      }
    }

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

  if (tab_container_view->IsHandlingDrag()) {
    dragged_view_bottom = tab_container_view->GetDraggingViewsBounds().bottom();
    if (size_bounds.height().is_bounded()) {
      dragged_view_bottom =
          std::min(dragged_view_bottom, size_bounds.height().value());
    }
  }
  layouts.host_size = gfx::Size(width, std::max(height, dragged_view_bottom));
  return layouts;
}

gfx::Size UnpinnedTabContainerViewLayout::CalculateHorizontalMinimumSize(
    const UnpinnedTabContainerView* tab_container_view) const {
  int min_width = 0;
  if (tab_container_view->collection_node_) {
    std::optional<tab_groups::TabGroupId> focused_group_id =
        GetFocusedGroupId(tab_container_view);

    for (const auto* child :
         tab_container_view->collection_node_->GetDirectChildren()) {
      bool should_show = true;
      if (focused_group_id.has_value()) {
        std::optional<tab_groups::TabGroupId> group_id =
            GetGroupIdForChild(child);
        if (group_id != focused_group_id.value()) {
          should_show = false;
        }
      }
      if (should_show) {
        min_width += child->GetMinimumSize().width();
      }
    }
  }
  return gfx::Size(min_width, GetLayoutConstant(LayoutConstant::kTabHeight));
}

gfx::Size UnpinnedTabContainerViewLayout::CalculateVerticalMinimumSize(
    const UnpinnedTabContainerView* tab_container_view) const {
  if (!tab_container_view->collection_node_) {
    return gfx::Size();
  }

  std::optional<tab_groups::TabGroupId> focused_group_id =
      GetFocusedGroupId(tab_container_view);

  int num_children = 0;
  for (const auto* child :
       tab_container_view->collection_node_->GetDirectChildren()) {
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

std::optional<tab_groups::TabGroupId>
UnpinnedTabContainerViewLayout::GetFocusedGroupId(
    const UnpinnedTabContainerView* tab_container_view) const {
  if (!tab_container_view->collection_node_ ||
      !tab_container_view->collection_node_->GetController()) {
    return std::nullopt;
  }
  return tab_container_view->collection_node_->GetController()
      ->GetFocusedGroup();
}

std::optional<tab_groups::TabGroupId>
UnpinnedTabContainerViewLayout::GetGroupIdForChild(
    const views::View* child) const {
  if (auto* group_view = views::AsViewClass<TabGroupView>(child)) {
    return group_view->GetTabGroup().id();
  }
  return std::nullopt;
}

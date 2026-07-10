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
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
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

  int x = 0;
  const int container_height = size_bounds.height().value_or(
      GetLayoutConstant(LayoutConstant::kTabHeight));

  for (auto* child :
       tab_container_view->collection_node_->GetDirectChildren()) {
    int child_width =
        child->GetPreferredSize(views::SizeBounds({}, size_bounds.height()))
            .width();

    gfx::Rect bounds(x, 0, child_width, container_height);
    auto drag_data = tab_container_view->GetVisualDataForDraggedView(*child);
    bounds.set_x(drag_data ? drag_data->offset.x() : x);

    layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
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
    views::SizeBounds child_size_bounds =
        views::SizeBounds(size_bounds.width().is_bounded()
                              ? (size_bounds.width() - (x + horizontal_padding))
                              : size_bounds.width(),
                          {});
    gfx::Rect bounds = gfx::Rect(child->GetPreferredSize(child_size_bounds));
    bounds.set_x(x);

    auto drag_data = tab_container_view->GetVisualDataForDraggedView(*child);
    CHECK(!drag_data || !drag_data->should_hide);
    bounds.set_y(drag_data ? drag_data->offset.y() : height);

    if (size_bounds.width().is_bounded()) {
      bounds.set_width(size_bounds.width().value() - bounds.x() -
                       horizontal_padding);
    }
    layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    height += bounds.height() + kTabVerticalPadding;
    width = std::max(width, bounds.width() + bounds.x());
  }
  if (!children.empty()) {
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
    for (const auto* child :
         tab_container_view->collection_node_->GetDirectChildren()) {
      min_width += child->GetMinimumSize().width();
    }
  }
  return gfx::Size(min_width, GetLayoutConstant(LayoutConstant::kTabHeight));
}

gfx::Size UnpinnedTabContainerViewLayout::CalculateVerticalMinimumSize(
    const UnpinnedTabContainerView* tab_container_view) const {
  if (!tab_container_view->collection_node_) {
    return gfx::Size();
  }

  const int num_children =
      tab_container_view->collection_node_->GetDirectChildren().size();
  const int min_height =
      base::ClampCeil(GetLayoutConstant(LayoutConstant::kVerticalTabHeight) *
                      std::min(1.5f, static_cast<float>(num_children))) +
      (num_children > 1 ? kTabVerticalPadding : 0);
  return gfx::Size(GetLayoutConstant(LayoutConstant::kVerticalTabMinWidth),
                   min_height);
}

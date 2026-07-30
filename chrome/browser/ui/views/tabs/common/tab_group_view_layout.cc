// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_group_view_layout.h"

#include <algorithm>
#include <vector>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/common/split_tab_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_utils.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "components/tabs/public/tab_group.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {
constexpr int kTabVerticalPadding = 2;
constexpr int kGroupLineWidth = 2;
constexpr int kGroupLineCollapsedLeadingPadding = 6;
constexpr int kGroupHeaderHeight = 26;
constexpr int kGroupHeaderVerticalMargin = 4;
}  // namespace

TabGroupViewLayout::TabGroupViewLayout(TabStripOrientation orientation)
    : orientation_(orientation) {}
TabGroupViewLayout::~TabGroupViewLayout() = default;

views::ProposedLayout TabGroupViewLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  const TabGroupView* tab_group_view =
      views::AsViewClass<TabGroupView>(host_view());
  if (!tab_group_view) {
    return views::ProposedLayout();
  }

  if (orientation_ == TabStripOrientation::kHorizontal) {
    return CalculateHorizontalLayout(tab_group_view, size_bounds);
  }
  return CalculateVerticalLayout(tab_group_view, size_bounds);
}

gfx::Size TabGroupViewLayout::GetMinimumSize(const views::View* host) const {
  const TabGroupView* tab_group_view = views::AsViewClass<TabGroupView>(host);
  if (!tab_group_view) {
    return gfx::Size();
  }

  if (orientation_ == TabStripOrientation::kHorizontal) {
    return CalculateHorizontalMinimumSize(tab_group_view);
  }
  return CalculateVerticalMinimumSize(tab_group_view);
}

views::ProposedLayout TabGroupViewLayout::CalculateVerticalLayout(
    const TabGroupView* tab_group_view,
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  int width = 0;
  int height = kGroupHeaderVerticalMargin;
  auto tab_strip_collapse_state = tab_group_view->GetTabStripCollapseState();

  gfx::Rect header_bounds;
  gfx::Rect group_line_bounds;
  group_line_bounds.set_width(kGroupLineWidth);

  // If the tab strip is collapsed then the group line should appear on the
  // leading side of all grouped tabs and the header.
  if (tab_strip_collapse_state !=
      tabs::VerticalTabStripCollapseState::kExpanded) {
    group_line_bounds.set_x(kGroupLineCollapsedLeadingPadding);
    group_line_bounds.set_y(height);
    header_bounds.set_x(
        GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding));
  }

  header_bounds.set_y(height);
  header_bounds.set_height(kGroupHeaderHeight);
  // If width is bounded, the group header should respect the width constraints
  // and take up the available width excluding trailing horizontal padding.
  if (size_bounds.width().is_bounded()) {
    header_bounds.set_width(size_bounds.width().value() - header_bounds.x());
  }
  layouts.child_layouts.emplace_back(
      tab_group_view->group_header_.get(),
      tab_group_view->group_header_->GetVisible(), header_bounds);
  height +=
      header_bounds.height() + kGroupHeaderVerticalMargin + kTabVerticalPadding;
  width = std::max(width, header_bounds.width());

  // If the tab strip is not collapsed then the group line is below and left
  // aligned with the header.
  if (tab_strip_collapse_state ==
      tabs::VerticalTabStripCollapseState::kExpanded) {
    group_line_bounds.set_x(
        (TabGroupView::kTabLeadingPadding - kGroupLineWidth) / 2);
    group_line_bounds.set_y(height);
  }

  const std::vector<views::View*> children =
      tab_group_view->collection_node_
          ? tab_group_view->collection_node_->GetDirectChildren()
          : std::vector<views::View*>();

  // Layout children in order. Children will have their preferred height and
  // fill available width.
  for (auto* child : children) {
    gfx::Rect bounds = gfx::Rect(child->GetPreferredSize(size_bounds));

    auto drag_data = tab_group_view->GetVisualDataForDraggedView(*child);
    CHECK(!drag_data || !drag_data->should_hide);
    bounds.set_y(drag_data ? drag_data->offset.y() : height);

    // If the tab strip is not collapsed then the groups tabs should be inset.
    bounds.set_x(tab_strip_collapse_state !=
                         tabs::VerticalTabStripCollapseState::kExpanded
                     ? GetLayoutConstant(
                           LayoutConstant::kVerticalTabStripHorizontalPadding)
                     : TabGroupView::kTabLeadingPadding);
    // If width is bounded, child views should respect the width constraints
    // and take up the available width excluding trailing horizontal padding.
    if (size_bounds.width().is_bounded()) {
      bounds.set_width(size_bounds.width().value() - bounds.x());
    }
    layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    height += bounds.height() + kTabVerticalPadding;
    width = std::max(width, bounds.width());
  }
  // Remove excess padding.
  height -= kTabVerticalPadding;

  if (!children.empty()) {
    group_line_bounds.set_height(height - group_line_bounds.y());
  }
  layouts.child_layouts.emplace_back(tab_group_view->group_line_.get(),
                                     tab_group_view->group_line_->GetVisible(),
                                     group_line_bounds);

  // Add extra padding below the group if not collapsed.
  const bool is_group_collapsed = tab_group_view->IsCollapsed();
  if (!is_group_collapsed) {
    height += kTabVerticalPadding;
  }

  // If the group is focused and we are handling a drag, we need to account for
  // the dragged views' bottom bound.
  int dragged_view_bottom = 0;
  if (tab_group_view->IsGroupFocused() && tab_group_view->IsHandlingDrag()) {
    dragged_view_bottom = tab_group_view->GetDraggingViewsBounds().bottom();
    if (size_bounds.height().is_bounded()) {
      dragged_view_bottom =
          std::min(dragged_view_bottom, size_bounds.height().value());
    }
  }

  layouts.host_size = gfx::Size(
      width, is_group_collapsed
                 ? header_bounds.height() + (2 * kGroupHeaderVerticalMargin)
                 : std::max(height, dragged_view_bottom));
  return layouts;
}

views::ProposedLayout TabGroupViewLayout::CalculateHorizontalLayout(
    const TabGroupView* tab_group_view,
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!tab_group_view->collection_node_) {
    return layouts;
  }

  const int container_height = size_bounds.height().value_or(
      GetLayoutConstant(LayoutConstant::kTabHeight));

  // Place the group header.
  int header_width = 0;
  if (tab_group_view->group_header_) {
    header_width =
        tab_group_view->group_header_
            ->GetPreferredSize(views::SizeBounds({}, container_height))
            .width();
    gfx::Rect header_bounds(0, 0, header_width, container_height);
    layouts.child_layouts.emplace_back(
        tab_group_view->group_header_.get(),
        tab_group_view->group_header_->GetVisible(), header_bounds);
  }

  // TODO(crbug.com/523328052): Update group line bounds and visibility for
  // horizontal orientation.
  if (tab_group_view->group_line_) {
    layouts.child_layouts.emplace_back(tab_group_view->group_line_.get(), false,
                                       gfx::Rect());
  }

  const std::vector<views::View*> children =
      tab_group_view->collection_node_->GetDirectChildren();

  int x = header_width;

  // Layout children in order following the group header.
  std::vector<int> child_preferred_widths;
  std::vector<int> child_min_widths;
  int total_preferred_width = 0;
  int total_min_width = 0;

  for (views::View* child : children) {
    int child_pref_width =
        child->GetPreferredSize(views::SizeBounds({}, container_height))
            .width();
    int child_min_width = child->GetMinimumSize().width();

    child_preferred_widths.push_back(child_pref_width);
    child_min_widths.push_back(child_min_width);
    total_preferred_width += child_pref_width;
    total_min_width += child_min_width;
  }

  int available_width =
      size_bounds.width().value_or(total_preferred_width + header_width);
  int width_for_children = std::max(0, available_width - header_width);

  std::vector<int> allocated_widths = CalculateProportionalChildWidths(
      width_for_children, child_preferred_widths, child_min_widths,
      total_preferred_width, total_min_width);

  for (size_t i = 0; i < children.size(); ++i) {
    views::View* child = children[i];
    int child_width = allocated_widths[i];

    gfx::Rect bounds(x, 0, child_width, container_height);
    auto drag_data = tab_group_view->GetVisualDataForDraggedView(*child);
    CHECK(!drag_data || !drag_data->should_hide);
    bounds.set_x(drag_data ? drag_data->offset.x() : x);

    layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    if (child->GetVisible()) {
      x += bounds.width();
    }
  }

  // If collapsed, the group only takes up the width of the header.
  layouts.host_size = gfx::Size(
      tab_group_view->IsCollapsed() ? header_width : x, container_height);
  return layouts;
}

gfx::Size TabGroupViewLayout::CalculateHorizontalMinimumSize(
    const TabGroupView* tab_group_view) const {
  int min_width = 0;
  if (tab_group_view->group_header_) {
    min_width += tab_group_view->group_header_->GetPreferredSize().width();
  }
  if (!tab_group_view->IsCollapsed()) {
    for (const auto* child :
         tab_group_view->collection_node_->GetDirectChildren()) {
      min_width += child->GetMinimumSize().width();
    }
  }
  return gfx::Size(min_width, GetLayoutConstant(LayoutConstant::kTabHeight));
}

gfx::Size TabGroupViewLayout::CalculateVerticalMinimumSize(
    const TabGroupView* tab_group_view) const {
  return CalculateVerticalLayout(tab_group_view, views::SizeBounds(0, 0))
      .host_size;
}

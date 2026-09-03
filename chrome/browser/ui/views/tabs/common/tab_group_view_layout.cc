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
#include "chrome/browser/ui/views/tabs/common/tab_group_line_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_layout_utils.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_style.h"
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

  const auto children =
      tab_group_view->collection_node_
          ? tab_group_view->collection_node_->GetDirectChildren()
          : TabCollectionNode::ChildViews();

  const bool is_focused = tab_group_view->IsGroupFocused();

  // Layout children in order. Children will have their preferred height and
  // fill available width.
  for (auto* child : children) {
    gfx::Rect bounds = gfx::Rect(child->GetPreferredSize(size_bounds));

    auto drag_data = tab_group_view->GetVisualDataForDraggedView(*child);
    CHECK(!drag_data || !drag_data->should_hide);
    bounds.set_y(drag_data ? drag_data->offset.y() : height);

    // If the tab strip is not collapsed and not focused then the groups tabs
    // should be inset.
    int child_x = 0;
    if (tab_strip_collapse_state !=
        tabs::VerticalTabStripCollapseState::kExpanded) {
      child_x =
          GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding);
    } else if (!is_focused) {
      child_x = TabGroupView::kTabLeadingPadding;
    }
    bounds.set_x(child_x);
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
  const bool show_group_line =
      !tab_group_view->IsGroupFocused() && !tab_group_view->is_collapsed();
  if (tab_group_view->group_line_) {
    layouts.child_layouts.emplace_back(tab_group_view->group_line_.get(),
                                       show_group_line, group_line_bounds);
  }

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

  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  const int header_overlap = TabGroupStyle::GetTabGroupOverlapAdjustment();
  const int container_height = TabStyle::Get()->GetStandardHeight();

  // Place the group header.
  int header_width = 0;
  if (tab_group_view->group_header_) {
    const int header_height =
        GetLayoutConstant(LayoutConstant::kTabHeight) -
        GetLayoutConstant(LayoutConstant::kTabStripPadding) -
        GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap);
    const int header_y = GetLayoutConstant(LayoutConstant::kTabStripPadding);
    header_width = tab_group_view->group_header_
                       ->GetPreferredSize(views::SizeBounds({}, header_height))
                       .width();
    gfx::Rect header_bounds(0, header_y, header_width, header_height);
    layouts.child_layouts.emplace_back(
        tab_group_view->group_header_.get(),
        tab_group_view->group_header_->GetVisible(), header_bounds);
  }

  TabStripCollectionLayoutInfo collection = CollectVisibleChildLayoutInfo(
      tab_group_view->collection_node_->GetDirectChildren(), container_height,
      base::BindRepeating(
          [](const TabGroupViewLayout* layout, const TabGroupView* group_view,
             const views::View* child) {
            if (!layout->CanBeVisible(child)) {
              return false;
            }
            auto drag_data = group_view->GetVisualDataForDraggedView(*child);
            return !(drag_data && drag_data->should_hide);
          },
          this, tab_group_view));

  const size_t num_children = collection.visible_children.size();
  const int header_space = (header_width > 0 && num_children > 0)
                               ? header_width - header_overlap
                               : header_width;
  const int overlap_total = collection.overlap_total;

  int x = header_space;

  if (num_children > 0) {
    const int net_preferred_width =
        std::max(0, collection.total_preferred_width - overlap_total);

    int available_width =
        size_bounds.width().value_or(net_preferred_width + header_space);
    int width_for_children = std::max(0, available_width - header_space);

    const int available_for_children_allocation =
        width_for_children + overlap_total;

    std::vector<int> allocated_widths = CalculateProportionalChildWidths(
        available_for_children_allocation, collection.preferred_widths,
        collection.crossover_widths, collection.min_widths,
        collection.total_preferred_width, collection.total_crossover_width,
        collection.total_min_width);

    for (size_t i = 0; i < collection.visible_children.size(); ++i) {
      const auto& info = collection.visible_children[i];
      int child_width = allocated_widths[i];

      auto drag_data = tab_group_view->GetVisualDataForDraggedView(*info.view);
      int child_x = drag_data ? drag_data->offset.x() : x;
      gfx::Rect bounds(child_x, 0, child_width, container_height);
      layouts.child_layouts.emplace_back(info.view.get(), true, bounds);

      x += child_width - tab_overlap;
    }
  }

  const int total_group_width =
      num_children > 0 ? (x + tab_overlap) : header_width;

  if (tab_group_view->group_line_) {
    const bool show_group_line =
        !tab_group_view->IsGroupFocused() && !tab_group_view->IsCollapsed();
    gfx::Rect group_line_bounds(0, 0, total_group_width, container_height);
    layouts.child_layouts.emplace_back(tab_group_view->group_line_.get(),
                                       show_group_line, group_line_bounds);
  }

  // If collapsed, the group only takes up the width of the header.
  layouts.host_size = gfx::Size(
      tab_group_view->IsCollapsed() ? header_width : total_group_width,
      container_height);
  return layouts;
}

int TabGroupViewLayout::CalculateHorizontalCrossoverWidth(
    const TabGroupView* tab_group_view) const {
  if (!tab_group_view || !tab_group_view->collection_node_) {
    return 0;
  }
  int crossover_width = 0;
  size_t count = 0;
  const bool has_header = tab_group_view->group_header_ &&
                          tab_group_view->group_header_->GetVisible();
  if (has_header) {
    crossover_width +=
        tab_group_view->group_header_->GetPreferredSize().width();
  }
  if (!tab_group_view->IsCollapsed()) {
    for (const auto* child :
         tab_group_view->collection_node_->GetDirectChildren()) {
      if (!CanBeVisible(child)) {
        continue;
      }
      auto drag_data = tab_group_view->GetVisualDataForDraggedView(*child);
      if (drag_data && drag_data->should_hide) {
        continue;
      }
      crossover_width += GetChildCrossoverWidth(child);
      count++;
    }
  }
  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  const int header_overlap = TabGroupStyle::GetTabGroupOverlapAdjustment();
  if (count > 0 && has_header) {
    crossover_width =
        std::max(0, crossover_width - header_overlap -
                        static_cast<int>(count - 1) * tab_overlap);
  } else if (count > 1) {
    crossover_width = std::max(
        0, crossover_width - static_cast<int>(count - 1) * tab_overlap);
  }
  return crossover_width;
}

gfx::Size TabGroupViewLayout::CalculateHorizontalMinimumSize(
    const TabGroupView* tab_group_view) const {
  if (!tab_group_view || !tab_group_view->collection_node_) {
    return gfx::Size();
  }
  int min_width = 0;
  size_t count = 0;
  const bool has_header = tab_group_view->group_header_ &&
                          tab_group_view->group_header_->GetVisible();
  if (has_header) {
    min_width += tab_group_view->group_header_->GetPreferredSize().width();
  }
  if (!tab_group_view->IsCollapsed()) {
    for (const auto* child :
         tab_group_view->collection_node_->GetDirectChildren()) {
      if (!CanBeVisible(child)) {
        continue;
      }
      auto drag_data = tab_group_view->GetVisualDataForDraggedView(*child);
      if (drag_data && drag_data->should_hide) {
        continue;
      }
      min_width += child->GetMinimumSize().width();
      count++;
    }
  }
  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  const int header_overlap = TabGroupStyle::GetTabGroupOverlapAdjustment();
  if (count > 0 && has_header) {
    min_width = std::max(0, min_width - header_overlap -
                                static_cast<int>(count - 1) * tab_overlap);
  } else if (count > 1) {
    min_width =
        std::max(0, min_width - static_cast<int>(count - 1) * tab_overlap);
  }
  return gfx::Size(min_width, TabStyle::Get()->GetStandardHeight());
}

gfx::Size TabGroupViewLayout::CalculateVerticalMinimumSize(
    const TabGroupView* tab_group_view) const {
  return CalculateVerticalLayout(tab_group_view, views::SizeBounds(0, 0))
      .host_size;
}

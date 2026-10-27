// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_group_view_horizontal_layout.h"

#include <algorithm>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_line_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_style.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_unfocus_button.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_layout_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"

namespace {
constexpr int kFocusedHeadersGap = 2;
}  // namespace

TabGroupViewHorizontalLayout::TabGroupViewHorizontalLayout() = default;
TabGroupViewHorizontalLayout::~TabGroupViewHorizontalLayout() = default;

int TabGroupViewHorizontalLayout::GetCrossoverWidth() const {
  if (!TabGroupView().collection_node_) {
    return 0;
  }
  int crossover_width = 0;
  size_t count = 0;
  const bool has_header = TabGroupView().group_header_ &&
                          TabGroupView().group_header_->GetVisible();
  if (has_header) {
    crossover_width += TabGroupView().group_header_->GetPreferredSize().width();
    if (TabGroupView().IsGroupFocused() && TabGroupView().unfocus_header_) {
      crossover_width +=
          TabGroupView().unfocus_header_->GetPreferredSize().width() +
          kFocusedHeadersGap;
    }
  }
  if (!TabGroupView().IsCollapsed()) {
    for (const auto* child :
         TabGroupView().collection_node_->GetDirectChildren()) {
      if (!CanBeVisible(child)) {
        continue;
      }
      auto drag_data = TabGroupView().GetVisualDataForDraggedView(*child);
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

gfx::Size TabGroupViewHorizontalLayout::GetMinimumSize(
    const views::View* host) const {
  if (!TabGroupView().collection_node_) {
    return gfx::Size();
  }
  int min_width = 0;
  size_t count = 0;
  const bool has_header = TabGroupView().group_header_ &&
                          TabGroupView().group_header_->GetVisible();
  if (has_header) {
    min_width += TabGroupView().group_header_->GetPreferredSize().width();
    if (TabGroupView().IsGroupFocused() && TabGroupView().unfocus_header_) {
      min_width += TabGroupView().unfocus_header_->GetPreferredSize().width() +
                   kFocusedHeadersGap;
    }
  }
  if (!TabGroupView().IsCollapsed()) {
    for (const auto* child :
         TabGroupView().collection_node_->GetDirectChildren()) {
      if (!CanBeVisible(child)) {
        continue;
      }
      auto drag_data = TabGroupView().GetVisualDataForDraggedView(*child);
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

views::ProposedLayout TabGroupViewHorizontalLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!TabGroupView().collection_node_) {
    return layouts;
  }

  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  const int header_overlap = TabGroupStyle::GetTabGroupOverlapAdjustment();
  const int container_height = TabStyle::Get()->GetStandardHeight();

  // Place the group header.
  int header_width = 0;
  if (TabGroupView().group_header_) {
    const bool is_focused = TabGroupView().IsGroupFocused();
    const int header_height = is_focused ? TabGroupStyle::GetFocusedChipHeight()
                                         : TabGroupStyle::GetEmptyChipSize();
    const int header_y = TabGroupStyle::GetTitleChipOffset(header_height).y();
    int x_offset = 0;
    if (TabGroupView().unfocus_header_) {
      if (is_focused) {
        const int unfocus_width =
            TabGroupView()
                .unfocus_header_
                ->GetPreferredSize(views::SizeBounds({}, header_height))
                .width();
        gfx::Rect unfocus_bounds(0, header_y, unfocus_width, header_height);
        layouts.child_layouts.emplace_back(TabGroupView().unfocus_header_.get(),
                                           true, unfocus_bounds);
        x_offset = unfocus_width + kFocusedHeadersGap;
      } else {
        layouts.child_layouts.emplace_back(TabGroupView().unfocus_header_.get(),
                                           false, gfx::Rect());
      }
    }
    const int main_header_width =
        TabGroupView()
            .group_header_
            ->GetPreferredSize(views::SizeBounds({}, header_height))
            .width();
    gfx::Rect header_bounds(x_offset, header_y, main_header_width,
                            header_height);
    layouts.child_layouts.emplace_back(
        TabGroupView().group_header_.get(),
        TabGroupView().group_header_->GetVisible(), header_bounds);
    header_width = x_offset + main_header_width;
  }

  TabStripCollectionLayoutInfo collection = CollectVisibleChildLayoutInfo(
      TabGroupView().collection_node_->GetDirectChildren(), container_height,
      base::BindRepeating(
          [](const TabGroupViewHorizontalLayout* layout,
             const views::View* child) {
            if (!layout->CanBeVisible(child)) {
              return false;
            }
            auto drag_data =
                layout->TabGroupView().GetVisualDataForDraggedView(*child);
            return !(drag_data && drag_data->should_hide);
          },
          this));

  const size_t num_children = collection.visible_children.size();
  const bool is_collapsed = TabGroupView().IsCollapsed();
  const int header_space =
      (header_width > 0 && num_children > 0 && !is_collapsed)
          ? header_width - header_overlap
          : header_width;
  const int overlap_total = collection.overlap_total;

  int x = header_space;

  if (num_children > 0) {
    if (is_collapsed) {
      for (const auto& info : collection.visible_children) {
        auto drag_data = TabGroupView().GetVisualDataForDraggedView(*info.view);
        int child_x = drag_data ? drag_data->offset.x() : header_width;
        layouts.child_layouts.emplace_back(
            info.view.get(), true, gfx::Rect(child_x, 0, 0, container_height));
      }
    } else {
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

        auto drag_data = TabGroupView().GetVisualDataForDraggedView(*info.view);
        int child_x = drag_data ? drag_data->offset.x() : x;
        gfx::Rect bounds(child_x, 0, child_width, container_height);
        layouts.child_layouts.emplace_back(info.view.get(), true, bounds);

        x += child_width - tab_overlap;
      }
    }
  }

  // If collapsed, the group only takes up the width of the header.
  const int total_group_width =
      (num_children > 0 && !is_collapsed) ? (x + tab_overlap) : header_width;

  if (TabGroupView().group_line_) {
    const bool show_group_line =
        !TabGroupView().IsGroupFocused() && !TabGroupView().is_collapsed();
    gfx::Rect group_line_bounds(0, 0, total_group_width, container_height);
    layouts.child_layouts.emplace_back(TabGroupView().group_line_.get(),
                                       show_group_line, group_line_bounds);
  }

  layouts.host_size = gfx::Size(total_group_width, container_height);
  return layouts;
}

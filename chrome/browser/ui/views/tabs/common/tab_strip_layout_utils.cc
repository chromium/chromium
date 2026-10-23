// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_strip_layout_utils.h"

#include <algorithm>

#include "base/check_op.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/common/split_tab_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_style.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

std::vector<int> CalculateProportionalChildWidths(
    int available_width,
    const std::vector<int>& child_preferred_widths,
    const std::vector<int>& child_crossover_widths,
    const std::vector<int>& child_min_widths,
    int total_preferred_width,
    int total_crossover_width,
    int total_min_width) {
  CHECK_EQ(child_preferred_widths.size(), child_crossover_widths.size());
  CHECK_EQ(child_preferred_widths.size(), child_min_widths.size());

  const size_t num_children = child_preferred_widths.size();
  std::vector<int> allocated_widths(num_children, 0);
  if (num_children == 0) {
    return allocated_widths;
  }

  // If the children's preferred widths fit in the available space, let them
  // take their preferred widths.
  if (available_width >= total_preferred_width) {
    return child_preferred_widths;
  }

  // If available space is below total minimum width, clamp to minimum widths.
  if (available_width <= total_min_width) {
    return child_min_widths;
  }

  // When available width is at least the crossover width, all tabs (active and
  // inactive) are the same size and shrink at the same rate. When available
  // width is below total crossover width, active tabs stay at their minimum
  // width (to keep the close button visible) while inactive tabs shrink towards
  // their minimum inactive width.
  const bool above_crossover = available_width >= total_crossover_width;
  const std::vector<int>& base_widths =
      above_crossover ? child_crossover_widths : child_min_widths;
  const std::vector<int>& target_widths =
      above_crossover ? child_preferred_widths : child_crossover_widths;
  const int total_base_width =
      above_crossover ? total_crossover_width : total_min_width;
  const int total_target_width =
      above_crossover ? total_preferred_width : total_crossover_width;

  const int total_capacity = total_target_width - total_base_width;
  if (total_capacity <= 0) {
    return target_widths;
  }

  int single_tab_capacity = 0;
  if (above_crossover) {
    single_tab_capacity =
        total_crossover_width == total_min_width
            ? TabStyle::Get()->GetStandardWidth(/*is_split=*/false) -
                  TabStyle::Get()->GetMinimumInactiveWidth()
            : TabStyle::Get()->GetStandardWidth(/*is_split=*/false) -
                  TabStyle::Get()->GetMinimumActiveWidth(/*is_split=*/false);
  } else {
    single_tab_capacity =
        TabStyle::Get()->GetMinimumActiveWidth(/*is_split=*/false) -
        TabStyle::Get()->GetMinimumInactiveWidth();
  }

  // Round down each tab's proportional width above `base_widths`.
  const int available_extra = available_width - total_base_width;
  std::vector<int> extra_space_slots(num_children, 0);
  int total_allocated = 0;

  for (size_t i = 0; i < num_children; ++i) {
    const int child_capacity = target_widths[i] - base_widths[i];
    if (child_capacity <= 0) {
      allocated_widths[i] = base_widths[i];
    } else if (single_tab_capacity > 0 &&
               child_capacity % single_tab_capacity == 0) {
      const int num_tabs = child_capacity / single_tab_capacity;
      const int per_tab_extra =
          (static_cast<int64_t>(available_extra) * single_tab_capacity) /
          total_capacity;
      allocated_widths[i] = base_widths[i] + num_tabs * per_tab_extra;
      extra_space_slots[i] = num_tabs;
    } else {
      const int child_extra =
          (static_cast<int64_t>(available_extra) * child_capacity) /
          total_capacity;
      allocated_widths[i] = base_widths[i] + child_extra;
      extra_space_slots[i] = 1;
    }
    total_allocated += allocated_widths[i];
  }

  // Because proportional tab widths are rounded down, the fractional part of
  // tab widths go unused. Retroactively round up tab widths from left to right
  // to use up that width.
  int remaining_extra_space = available_width - total_allocated;
  for (size_t i = 0; i < num_children && remaining_extra_space > 0; ++i) {
    if (extra_space_slots[i] > 0) {
      const int extra = std::min(remaining_extra_space, extra_space_slots[i]);
      allocated_widths[i] += extra;
      remaining_extra_space -= extra;
    }
  }

  return allocated_widths;
}

std::vector<int> CalculateProportionalChildWidths(
    int available_width,
    const std::vector<int>& child_preferred_widths,
    const std::vector<int>& child_min_widths,
    int total_preferred_width,
    int total_min_width) {
  return CalculateProportionalChildWidths(
      available_width, child_preferred_widths, child_min_widths,
      child_min_widths, total_preferred_width, total_min_width,
      total_min_width);
}

int GetChildCrossoverWidth(const views::View* child) {
  if (!child) {
    return 0;
  }
  if (const auto* tab_view = views::AsViewClass<TabView>(child)) {
    if (tab_view->pinned()) {
      return tab_view->tab_styling()->tab_style()->GetPinnedWidth(
          tab_view->split());
    }
    return tab_view->tab_styling()->tab_style()->GetMinimumActiveWidth(
        tab_view->split());
  }
  if (const auto* split_view = views::AsViewClass<SplitTabView>(child)) {
    return split_view->GetCrossoverWidth();
  }
  if (const auto* group_view = views::AsViewClass<TabGroupView>(child)) {
    return group_view->GetCrossoverWidth();
  }
  return child->GetMinimumSize().width();
}

int GetChildOverlap(const views::View* prev_child,
                    const views::View* next_child) {
  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  const int header_overlap = TabGroupStyle::GetTabGroupOverlapAdjustment();

  const auto* prev_group = views::AsViewClass<TabGroupView>(prev_child);
  const auto* next_group = views::AsViewClass<TabGroupView>(next_child);
  // When the previous group is collapsed and the next child is a tab group,
  // both adjacent elements are header chips. A negative overlap is returned to
  // provide padding between them.
  if (prev_group && prev_group->IsCollapsed() && next_group) {
    return -TabGroupStyle::GetPaddingBetweenCollapsedHeaders();
  }
  if (next_group) {
    return header_overlap;
  }
  if (prev_group && prev_group->IsCollapsed()) {
    return header_overlap;
  }
  return tab_overlap;
}

int GetLeadingChildPadding(const views::View* child) {
  if (child && views::IsViewClass<TabGroupView>(child)) {
    return TabGroupStyle::GetLeadingGroupHeaderPadding();
  }
  return 0;
}

TabStripCollectionLayoutInfo CollectVisibleChildLayoutInfo(
    TabCollectionNode::ChildViews children,
    int container_height,
    ChildVisibilityCallback is_child_visible) {
  TabStripCollectionLayoutInfo collection;
  for (views::View* child : children) {
    if (!is_child_visible.Run(child)) {
      continue;
    }

    int pref_width =
        child->GetPreferredSize(views::SizeBounds({}, container_height))
            .width();
    int crossover_width = GetChildCrossoverWidth(child);
    int min_width = child->GetMinimumSize().width();

    collection.visible_children.push_back({
        .view = child,
        .pref_width = pref_width,
        .crossover_width = crossover_width,
        .min_width = min_width,
    });

    if (collection.visible_children.size() == 1) {
      collection.leading_padding = GetLeadingChildPadding(child);
    } else {
      const size_t idx = collection.visible_children.size() - 1;
      collection.overlap_total +=
          GetChildOverlap(collection.visible_children[idx - 1].view,
                          collection.visible_children[idx].view);
    }

    collection.preferred_widths.push_back(pref_width);
    collection.crossover_widths.push_back(crossover_width);
    collection.min_widths.push_back(min_width);
    collection.total_preferred_width += pref_width;
    collection.total_crossover_width += crossover_width;
    collection.total_min_width += min_width;
  }
  return collection;
}

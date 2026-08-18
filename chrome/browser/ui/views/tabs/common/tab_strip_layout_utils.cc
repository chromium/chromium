// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_strip_layout_utils.h"

#include <algorithm>

#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_style.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

std::vector<int> CalculateProportionalChildWidths(
    int available_width,
    const std::vector<int>& child_preferred_widths,
    const std::vector<int>& child_min_widths,
    int total_preferred_width,
    int total_min_width) {
  const size_t num_children = child_preferred_widths.size();
  std::vector<int> allocated_widths(num_children, 0);
  if (num_children == 0) {
    return allocated_widths;
  }

  // If the children's preferred widths fit in the available space, let them
  // take their preferred widths.
  if (available_width >= total_preferred_width) {
    allocated_widths = child_preferred_widths;
    // Children must have at least their minimum widths if available space is
    // constrained.
  } else if (available_width <= total_min_width) {
    allocated_widths = child_min_widths;
    // Otherwise proportionally shrink children between their preferred and
    // minimum sizes.
  } else {
    int accumulated_allocated = 0;
    int accumulated_shrink_capacity = 0;
    int accumulated_preferred = 0;
    const int total_shrink_capacity = total_preferred_width - total_min_width;
    const int total_shrink_needed = total_preferred_width - available_width;

    for (size_t i = 0; i < num_children; ++i) {
      accumulated_shrink_capacity +=
          child_preferred_widths[i] - child_min_widths[i];
      accumulated_preferred += child_preferred_widths[i];

      int target_cumulative_shrink =
          (static_cast<int64_t>(total_shrink_needed) *
               accumulated_shrink_capacity +
           total_shrink_capacity / 2) /
          total_shrink_capacity;

      int target_cumulative_allocated =
          accumulated_preferred - target_cumulative_shrink;
      int child_width = target_cumulative_allocated - accumulated_allocated;

      allocated_widths[i] = child_width;
      accumulated_allocated += child_width;
    }
  }

  return allocated_widths;
}

int GetChildOverlap(const views::View* prev_child,
                    const views::View* next_child) {
  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  const int header_overlap = TabGroupStyle::GetTabGroupOverlapAdjustment();

  const auto* next_group = views::AsViewClass<TabGroupView>(next_child);
  if (next_group) {
    return header_overlap;
  }
  const auto* prev_group = views::AsViewClass<TabGroupView>(prev_child);
  if (prev_group && prev_group->IsCollapsed()) {
    return header_overlap;
  }
  return tab_overlap;
}

TabStripCollectionLayoutInfo CollectVisibleChildLayoutInfo(
    const std::vector<views::View*>& children,
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
    int min_width = child->GetMinimumSize().width();

    collection.visible_children.push_back({
        .view = child,
        .pref_width = pref_width,
        .min_width = min_width,
    });

    if (collection.visible_children.size() > 1) {
      const size_t idx = collection.visible_children.size() - 1;
      collection.overlap_total +=
          GetChildOverlap(collection.visible_children[idx - 1].view,
                          collection.visible_children[idx].view);
    }

    collection.preferred_widths.push_back(pref_width);
    collection.min_widths.push_back(min_width);
    collection.total_preferred_width += pref_width;
    collection.total_min_width += min_width;
  }
  return collection;
}

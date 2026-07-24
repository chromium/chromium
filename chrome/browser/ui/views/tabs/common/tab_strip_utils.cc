// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_strip_utils.h"

#include "chrome/browser/ui/views/tabs/common/pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

gfx::Rect GetTabStripViewTargetBounds(const views::View* view) {
  CHECK(view);

  const views::View* const parent = view->parent();
  const auto has_animating_layout_manager = [](const views::View* container) {
    // New clients of `TabCollectionAnimatingLayoutManager` should be added to
    // this list as usage expands.
    return views::IsViewClass<PinnedTabContainerView>(container) ||
           views::IsViewClass<UnpinnedTabContainerView>(container) ||
           views::IsViewClass<TabGroupView>(container);
  };
  if (!parent || !has_animating_layout_manager(parent)) {
    return view->bounds();
  }

  const auto* const layout_manager =
      static_cast<const TabCollectionAnimatingLayoutManager*>(
          parent->GetLayoutManager());
  CHECK(layout_manager);

  const views::ChildLayout* const view_layout =
      layout_manager->target_layout().GetLayoutFor(view);
  return view_layout ? view_layout->bounds : view->bounds();
}

TabStripView* GetTabStripView(views::View* view) {
  for (views::View* v = view->parent(); v; v = v->parent()) {
    if (auto* tab_strip = views::AsViewClass<TabStripView>(v)) {
      return tab_strip;
    }
  }
  return nullptr;
}

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
    int remaining_available = available_width;
    int total_shrink_needed = total_preferred_width - available_width;
    int total_shrink_capacity = total_preferred_width - total_min_width;

    for (size_t i = 0; i < num_children; ++i) {
      int shrink_capacity = child_preferred_widths[i] - child_min_widths[i];
      int child_shrink = 0;
      if (total_shrink_capacity > 0 && total_shrink_needed > 0) {
        child_shrink =
            (total_shrink_needed * shrink_capacity) / total_shrink_capacity;
        child_shrink = std::min(
            child_shrink, child_preferred_widths[i] - child_min_widths[i]);
      }
      int child_width = child_preferred_widths[i] - child_shrink;
      // The last child gets all remaining space to absorb rounding errors.
      if (i == num_children - 1) {
        child_width = remaining_available;
      }

      allocated_widths[i] = child_width;
      remaining_available -= child_width;
    }
  }

  return allocated_widths;
}

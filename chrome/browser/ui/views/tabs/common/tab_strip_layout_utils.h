// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_STRIP_LAYOUT_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_STRIP_LAYOUT_UTILS_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"

namespace views {
class View;
}

// Layout and sizing info for a single visible child view in a tab strip
// container.
struct TabStripChildLayoutInfo {
  raw_ptr<views::View> view;
  int pref_width;
  int crossover_width;
  int min_width;
};

// Collected layout info and width totals for all visible children in a tab
// strip container.
struct TabStripCollectionLayoutInfo {
  std::vector<TabStripChildLayoutInfo> visible_children;
  std::vector<int> preferred_widths;
  std::vector<int> crossover_widths;
  std::vector<int> min_widths;
  int total_preferred_width = 0;
  int total_crossover_width = 0;
  int total_min_width = 0;
  int overlap_total = 0;
};

using ChildVisibilityCallback =
    base::RepeatingCallback<bool(const views::View*)>;

// Proportionally distributes `available_width` among child views:
// - When available width is at least the crossover width, all tabs (active and
//   inactive) are the same size and shrink at the same rate.
// - When available width is below total crossover width, active tabs stay at
//   their minimum width (to keep the close button visible) while inactive tabs
//   shrink towards their minimum inactive width.
std::vector<int> CalculateProportionalChildWidths(
    int available_width,
    const std::vector<int>& child_preferred_widths,
    const std::vector<int>& child_crossover_widths,
    const std::vector<int>& child_min_widths,
    int total_preferred_width,
    int total_crossover_width,
    int total_min_width);

// Overload that treats crossover_width as min_width for convenience.
std::vector<int> CalculateProportionalChildWidths(
    int available_width,
    const std::vector<int>& child_preferred_widths,
    const std::vector<int>& child_min_widths,
    int total_preferred_width,
    int total_min_width);

// Returns the minimum width for `child` where all child tabs maintain equal
// sizes.
int GetChildCrossoverWidth(const views::View* child);

// Returns the horizontal overlap between `prev_child` and `next_child`.
int GetChildOverlap(const views::View* prev_child,
                    const views::View* next_child);

// Measures preferred, crossover, and minimum widths and accumulates total
// layout info for all visible, non-hidden children matching `is_child_visible`.
TabStripCollectionLayoutInfo CollectVisibleChildLayoutInfo(
    TabCollectionNode::ChildViews children,
    int container_height,
    ChildVisibilityCallback is_child_visible);

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_STRIP_LAYOUT_UTILS_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_STRIP_LAYOUT_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_STRIP_LAYOUT_UTILS_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace views {
class View;
}

// Layout and sizing info for a single visible child view in a tab strip
// container.
struct TabStripChildLayoutInfo {
  raw_ptr<views::View> view;
  int pref_width;
  int min_width;
};

// Collected layout info and width totals for all visible children in a tab
// strip container.
struct TabStripCollectionLayoutInfo {
  std::vector<TabStripChildLayoutInfo> visible_children;
  std::vector<int> preferred_widths;
  std::vector<int> min_widths;
  int total_preferred_width = 0;
  int total_min_width = 0;
  int overlap_total = 0;
};

using ChildVisibilityCallback =
    base::RepeatingCallback<bool(const views::View*)>;

// Proportionally distributes `available_width` among child views between their
// preferred and minimum widths.
std::vector<int> CalculateProportionalChildWidths(
    int available_width,
    const std::vector<int>& child_preferred_widths,
    const std::vector<int>& child_min_widths,
    int total_preferred_width,
    int total_min_width);

// Returns the horizontal overlap between `prev_child` and `next_child`.
int GetChildOverlap(const views::View* prev_child,
                    const views::View* next_child);

// Measures preferred and minimum widths and accumulates total layout info for
// all visible, non-hidden children matching `is_child_visible`.
TabStripCollectionLayoutInfo CollectVisibleChildLayoutInfo(
    const std::vector<views::View*>& children,
    int container_height,
    ChildVisibilityCallback is_child_visible);

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_STRIP_LAYOUT_UTILS_H_

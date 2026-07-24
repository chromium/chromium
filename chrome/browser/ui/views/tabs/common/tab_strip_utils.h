// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_STRIP_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_STRIP_UTILS_H_

#include "ui/gfx/geometry/rect.h"

class TabStripView;

namespace views {
class View;
}

// Returns the target bounds for the provided `view` in the tab strip
// hierarchy. For views managed by `TabCollectionAnimatingLayoutManager` this
// may differ from current `View::bounds()` due to animated transitions. For
// other views the current bounds will be returned.
gfx::Rect GetTabStripViewTargetBounds(const views::View* view);

// Returns the tab strip view for the provided `view`. Iterates
// through the parent hierarchy until a `TabStripView` is found.
TabStripView* GetTabStripView(views::View* view);

// Calculates the allocated width for each child view given the available
// width, preferred widths, and minimum widths. If available space is
// constrained, children are proportionally shrunk between preferred and minimum
// widths.
std::vector<int> CalculateProportionalChildWidths(
    int available_width,
    const std::vector<int>& child_preferred_widths,
    const std::vector<int>& child_min_widths,
    int total_preferred_width,
    int total_min_width);

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_STRIP_UTILS_H_

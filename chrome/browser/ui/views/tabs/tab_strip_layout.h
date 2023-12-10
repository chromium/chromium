// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_H_

#include <optional>
#include <vector>

#include "chrome/browser/ui/views/tabs/tab_strip_layout_types.h"
#include "chrome/browser/ui/views/tabs/tab_width_constraints.h"

namespace gfx {
class Rect;
}

// Determines the size of each tab given information on the overall amount
// of space available relative to how much the tabs could use.
class TabSizer {
 public:
  TabSizer(LayoutDomain domain, float space_fraction_available);
  TabSizer(const TabSizer&) = default;
  TabSizer& operator=(const TabSizer&) = default;

  int CalculateTabWidth(const TabWidthConstraints& tab) const;

  // Returns true iff it's OK for this tab to be one pixel wider than
  // CalculateTabWidth(|tab|).
  bool TabAcceptsExtraSpace(const TabWidthConstraints& tab) const;

  bool IsAlreadyPreferredWidth() const;

 private:
  LayoutDomain domain_;

  // The proportion of space requirements we can fulfill within the layout
  // domain we're in.
  float space_fraction_available_;
};

// Contains the information needed to freeze the width of each tab.
struct TabWidthOverride {
  TabSizer sizer;

  // The number of pixels of extra width that should be distributed.
  int extra_space;
};

// Calculates and returns the bounds of the tabs. |width| is the available
// width to use for tab layout. This never sizes the tabs smaller then the
// minimum widths in TabSizeInfo, and as a result the calculated bounds may go
// beyond |width|.
std::vector<gfx::Rect> CalculateTabBounds(
    const TabLayoutConstants& layout_constants,
    const std::vector<TabWidthConstraints>& tabs,
    std::optional<int> width);

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_H_

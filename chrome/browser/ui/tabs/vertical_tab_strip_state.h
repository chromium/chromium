// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_H_
#define CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_H_

namespace tabs {

inline constexpr int kVerticalTabStripDefaultUncollapsedWidth = 240;
// TODO(crbug.com/465833741): Replace constant with derived value based on
// caption buttons.
inline constexpr int kVerticalTabStripUncollapsedMinWidth = 126;
// TODO(crbug.com/465832180): Replace constant based width final max width for
// view.
inline constexpr int kVerticalTabStripUncollapsedMaxWidth = 400;
inline constexpr int kVerticalTabStripCollapsedWidth = 56;
// TODO(crbug.com/465833741): Determine snapping behavior.
inline constexpr int kVerticalTabStripCollapseSnapWidth =
    (kVerticalTabStripUncollapsedMinWidth + kVerticalTabStripCollapsedWidth) /
    2;

// Per-window state for the vertical tab strip.
struct VerticalTabStripState {
  // Whether the vertical tab strip is collapsed.
  bool collapsed = false;
  // The width of the vertical tab strip when it is not collapsed.
  int uncollapsed_width = kVerticalTabStripDefaultUncollapsedWidth;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_H_

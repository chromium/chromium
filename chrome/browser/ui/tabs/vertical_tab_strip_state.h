// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_H_
#define CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_H_

namespace tabs {

// Per-window state for the vertical tab strip.
struct VerticalTabStripState {
  // Whether the vertical tab strip is collapsed.
  bool collapsed = false;
  // The width of the vertical tab strip when it is not collapsed.
  int uncollapsed_width = 0;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_VERTICAL_TAB_STRIP_STATE_H_

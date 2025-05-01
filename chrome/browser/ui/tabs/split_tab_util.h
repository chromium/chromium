// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_TAB_UTIL_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_TAB_UTIL_H_

class TabStripModel;

namespace split_tabs {

class SplitTabId;

enum class SplitTabActiveLocation {
  // In LTR languages, the starting location will be on the left half
  // but mirrored on the right side for RTL languages.
  kStart,
  // In LTR languages, the starting location will be on the right half
  // but mirrored on the left side for RTL languages.
  kEnd,
  kTop,
  kBottom,
};

// Returns the index of the last active tab in a split.
int GetIndexOfLastActiveTab(TabStripModel* tab_strip_model, SplitTabId id);

// Returns where the last active tab was oriented in a split.
SplitTabActiveLocation GetLastActiveTabLocation(TabStripModel* tab_strip_model,
                                                SplitTabId split_id);
}  // namespace split_tabs

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_TAB_UTIL_H_

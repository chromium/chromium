// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_METRICS_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_METRICS_H_

namespace base {
class TimeDelta;
}  // namespace base

enum class TabStripUIOpenAction {
  kTapOnTabCounter = 0,
  kToolbarDrag = 1,
  kTabDraggedIntoWindow = 2,
  kMaxValue = kTabDraggedIntoWindow,
};

enum class TabStripUICloseAction {
  kTapOnTabCounter = 0,
  // No longer used
  // kTapOutsideTabStrip = 1,
  kTabSelected = 2,
  kTapInTabContent = 3,
  kOmniboxFocusedOrNewTabOpened = 4,
  kMaxValue = kOmniboxFocusedOrNewTabOpened,
};

void RecordTabStripUIOpenHistogram(TabStripUIOpenAction action);
void RecordTabStripUICloseHistogram(TabStripUICloseAction action);
void RecordTabStripUIOpenDurationHistogram(base::TimeDelta duration);

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_METRICS_H_

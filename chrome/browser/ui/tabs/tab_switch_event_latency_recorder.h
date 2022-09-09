// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_SWITCH_EVENT_LATENCY_RECORDER_H_
#define CHROME_BROWSER_UI_TABS_TAB_SWITCH_EVENT_LATENCY_RECORDER_H_

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Store the timestamps related to switching tabs, and generate UMA metrics to
// track the latency between the input event timestamp and the time when the
// tab strip begins processing the tab switch.
class TabSwitchEventLatencyRecorder {
 public:
  TabSwitchEventLatencyRecorder();
  TabSwitchEventLatencyRecorder(const TabSwitchEventLatencyRecorder&) = delete;
  TabSwitchEventLatencyRecorder& operator=(
      const TabSwitchEventLatencyRecorder&) = delete;

  // Starts timing the tab switch input event latency. If this is called again
  // without a following OnWillChangeActiveTab, this will overwrite the
  // previously recorded value.
  void BeginLatencyTiming(TabStripUserGestureDetails details);

  // Finishes the latency tracking started by BeginLatencyTiming and record the
  // result to UMA. If this is called without a preceding BeginLatencyTiming,
  // this do nothing.
  void OnWillChangeActiveTab(const base::TimeTicks change_time);

  absl::optional<TabStripUserGestureDetails> details() const {
    return details_;
  }

 private:
  absl::optional<TabStripUserGestureDetails> details_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_SWITCH_EVENT_LATENCY_RECORDER_H_

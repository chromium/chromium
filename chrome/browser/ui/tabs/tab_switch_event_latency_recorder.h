// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_SWITCH_EVENT_LATENCY_RECORDER_H_
#define CHROME_BROWSER_UI_TABS_TAB_SWITCH_EVENT_LATENCY_RECORDER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"

// Store the timestamps related to switching tabs, and generate UMA metrics to
// track the latency between the input event timestamp and the time when the
// tab strip begins processing the tab switch.
class TabSwitchEventLatencyRecorder {
 public:
  enum class EventType { kMouse, kKeyboard, kTouch, kWheel, kOther };

  TabSwitchEventLatencyRecorder();

  // Starts timing the tab switch input event latency. If this is called again
  // without a following OnWillChangeActiveTab, this will overwrite the
  // previously recorded value.
  void BeginLatencyTiming(const base::TimeTicks event_timestamp,
                          EventType event_type);

  // Finishes the latency tracking started by BeginLatencyTiming and record the
  // result to UMA. If this is called without a preceding BeginLatencyTiming,
  // this do nothing.
  void OnWillChangeActiveTab(const base::TimeTicks change_time);

  base::TimeTicks input_event_timestamp() const {
    return input_event_timestamp_;
  }

 private:
  base::TimeTicks input_event_timestamp_ = base::TimeTicks();
  base::Optional<EventType> event_type_ = base::nullopt;

  DISALLOW_COPY_AND_ASSIGN(TabSwitchEventLatencyRecorder);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_SWITCH_EVENT_LATENCY_RECORDER_H_

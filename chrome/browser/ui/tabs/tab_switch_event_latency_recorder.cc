// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_switch_event_latency_recorder.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"

TabSwitchEventLatencyRecorder::TabSwitchEventLatencyRecorder() = default;

void TabSwitchEventLatencyRecorder::BeginLatencyTiming(
    TabStripUserGestureDetails details) {
  details_ = details;
}

void TabSwitchEventLatencyRecorder::OnWillChangeActiveTab(
    const base::TimeTicks change_time) {
  if (!details_.has_value())
    return;

  const auto delta = change_time - details_->time_stamp;
  switch (details_.value().type) {
    case TabStripUserGestureDetails::GestureType::kKeyboard:
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Browser.Tabs.InputEventToSelectionTime.Keyboard", delta,
          base::Microseconds(100), base::Milliseconds(50), 50);
      break;
    case TabStripUserGestureDetails::GestureType::kMouse:
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Browser.Tabs.InputEventToSelectionTime.Mouse", delta,
          base::Microseconds(100), base::Milliseconds(50), 50);
      break;
    case TabStripUserGestureDetails::GestureType::kTouch:
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Browser.Tabs.InputEventToSelectionTime.Touch", delta,
          base::Microseconds(100), base::Milliseconds(50), 50);
      break;
    case TabStripUserGestureDetails::GestureType::kWheel:
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Browser.Tabs.InputEventToSelectionTime.Wheel", delta,
          base::Microseconds(100), base::Milliseconds(50), 50);
      break;
    case TabStripUserGestureDetails::GestureType::kTabMenu:
    case TabStripUserGestureDetails::GestureType::kOther:
    case TabStripUserGestureDetails::GestureType::kNone:
      break;
  }

  details_ = absl::nullopt;
}

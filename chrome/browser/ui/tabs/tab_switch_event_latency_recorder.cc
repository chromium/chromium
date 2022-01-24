// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_switch_event_latency_recorder.h"

#include "base/check.h"
#include "base/metrics/histogram_macros.h"

TabSwitchEventLatencyRecorder::TabSwitchEventLatencyRecorder() {}

void TabSwitchEventLatencyRecorder::BeginLatencyTiming(
    const base::TimeTicks event_timestamp,
    EventType event_type) {
  input_event_timestamp_ = event_timestamp;
  event_type_ = event_type;
}

void TabSwitchEventLatencyRecorder::OnWillChangeActiveTab(
    const base::TimeTicks change_time) {
  if (!event_type_.has_value() || *event_type_ == EventType::kOther)
    return;

  DCHECK(!input_event_timestamp_.is_null());
  const auto delta = change_time - input_event_timestamp_;
  switch (event_type_.value()) {
    case EventType::kKeyboard:
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Browser.Tabs.InputEventToSelectionTime.Keyboard", delta,
          base::Microseconds(100), base::Milliseconds(50), 50);
      break;
    case EventType::kMouse:
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Browser.Tabs.InputEventToSelectionTime.Mouse", delta,
          base::Microseconds(100), base::Milliseconds(50), 50);
      break;
    case EventType::kTouch:
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Browser.Tabs.InputEventToSelectionTime.Touch", delta,
          base::Microseconds(100), base::Milliseconds(50), 50);
      break;
    case EventType::kWheel:
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Browser.Tabs.InputEventToSelectionTime.Wheel", delta,
          base::Microseconds(100), base::Milliseconds(50), 50);
      break;
    case EventType::kOther:
      break;
  }
  event_type_ = absl::nullopt;
  input_event_timestamp_ = base::TimeTicks();
}

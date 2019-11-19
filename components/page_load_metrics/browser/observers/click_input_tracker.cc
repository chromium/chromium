// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/click_input_tracker.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"

namespace page_load_metrics {

const int kMaxCountForUkm = 50;

ClickInputTracker::ClickInputTracker() {
  // Set thresholds per Feature parameters as appropriate.
  time_delta_threshold_ =
      base::TimeDelta::FromMilliseconds(base::GetFieldTrialParamByFeatureAsInt(
          kClickInputTracker, "time_delta_threshold_ms", 500));
  position_delta_threshold_ = base::GetFieldTrialParamByFeatureAsInt(
      kClickInputTracker, "position_delta_threshold", 10);
  burst_count_threshold_ = base::GetFieldTrialParamByFeatureAsInt(
      kClickInputTracker, "burst_count_threshold", 3);
}

ClickInputTracker::~ClickInputTracker() = default;

void ClickInputTracker::OnUserInput(const blink::WebInputEvent& event) {
  if (!base::FeatureList::IsEnabled(kClickInputTracker))
    return;

  if (event.GetType() != blink::WebInputEvent::kGestureTap &&
      event.GetType() != blink::WebInputEvent::kMouseUp) {
    return;
  }

  blink::WebFloatPoint position;
  if (event.GetType() == blink::WebInputEvent::kGestureTap) {
    const blink::WebGestureEvent& gesture =
        static_cast<const blink::WebGestureEvent&>(event);
    position = gesture.PositionInScreen();
  } else if (event.GetType() == blink::WebInputEvent::kMouseUp) {
    const blink::WebMouseEvent& mouse_click =
        static_cast<const blink::WebMouseEvent&>(event);
    position = mouse_click.PositionInScreen();
  } else {
    NOTREACHED();
    return;
  }

  if (!last_click_timestamp_.is_null()) {
    base::TimeDelta delta = event.TimeStamp() - last_click_timestamp_;
    if (delta < time_delta_threshold_ &&
        std::abs(position.x - last_click_position_.x) <
            position_delta_threshold_ &&
        std::abs(position.y - last_click_position_.y) <
            position_delta_threshold_) {
      current_click_input_burst_++;
      max_click_input_burst_ =
          std::max(max_click_input_burst_, current_click_input_burst_);
    } else {
      current_click_input_burst_ = 1;
    }
  } else {
    current_click_input_burst_ = 1;
  }

  last_click_timestamp_ = event.TimeStamp();
  last_click_position_ = position;
}

void ClickInputTracker::RecordClickBurst(ukm::SourceId source_id) {
  if (!base::FeatureList::IsEnabled(kClickInputTracker))
    return;

  if (max_click_input_burst_ >= burst_count_threshold_) {
    UMA_HISTOGRAM_COUNTS_100("PageLoad.Experimental.ClickInputBurst",
                             max_click_input_burst_);
    ukm::builders::ClickInput(source_id)
        .SetExperimental_ClickInputBurst(
            std::min(max_click_input_burst_, kMaxCountForUkm))
        .Record(ukm::UkmRecorder::Get());
  }
}

}  // namespace page_load_metrics

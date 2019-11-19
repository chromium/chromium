// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CLICK_INPUT_TRACKER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CLICK_INPUT_TRACKER_H_

#include "base/feature_list.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_input_event.h"

namespace page_load_metrics {

const base::Feature kClickInputTracker{"ClickInputTracker",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// This class considers user input clicks for a page load to determine if a
// burst of clicks occurs at the screen position. This is a possible signal
// that the user may be rage clicking on an unresponsive page.
class ClickInputTracker {
 public:
  ClickInputTracker();
  ~ClickInputTracker();

  // Considers whether |event| is part of a user click burst. Must be called
  // when the user input is encountered (for proper timing consideration).
  void OnUserInput(const blink::WebInputEvent& event);

  // If this tracker identified a click burst, this will report metrics for it.
  void RecordClickBurst(ukm::SourceId source_id);

  int GetCurrentBurstCountForTesting() const {
    return current_click_input_burst_;
  }

  int GetMaxBurstCountForTesting() const { return max_click_input_burst_; }

 private:
  // Maximum time delta between clicks at same location to be considered part
  // of a click burst. May be set via Feature parameter.
  base::TimeDelta time_delta_threshold_;

  // Maximum position delta in X or Y dimensions to be considered the "same"
  // click location to be considered as part of a click burst. May be set via
  // Feature parameter.
  int position_delta_threshold_;

  // Number of clicks to be considered a burst. May be set via Feature
  // parameter.
  int burst_count_threshold_;

  // Counter of user clicks (GestureTap or MouseUp) in a burst (that occur
  // close together in time and position). This counter is reset when there
  // is a time gap or position gap between clicks.
  int current_click_input_burst_ = 0;

  // Max user click input burst. This is the maximum of any previous counts
  // recorded in |current_click_input_burst_| for the page load. This will
  // be recorded to metrics if over a minimum size.
  int max_click_input_burst_ = 0;

  // Timestamp of last user click input.
  base::TimeTicks last_click_timestamp_;

  // Position of the last click input.
  blink::WebFloatPoint last_click_position_;

  DISALLOW_COPY_AND_ASSIGN(ClickInputTracker);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CLICK_INPUT_TRACKER_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_scrubbing_metrics.h"

#include <stddef.h>
#include <stdint.h>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"

namespace {
constexpr base::TimeDelta kTabScrubbingHistogramIntervalTime =
    base::Seconds(30);

constexpr base::TimeDelta kMaxTimeConsideredScrubbing =
    base::Milliseconds(1500);
}  // namespace

void TabStripScrubbingMetrics::Init() {
  last_tab_switch_timestamp_ = base::TimeTicks::Now();
  tab_scrubbing_interval_timer_.Start(
      FROM_HERE, kTabScrubbingHistogramIntervalTime,
      base::BindRepeating(&TabStripScrubbingMetrics::RecordTabScrubbingMetrics,
                          base::Unretained(this)));
}

void TabStripScrubbingMetrics::IncrementPressCount(
    const TabStripUserGestureDetails& user_gesture) {
  // Maybe increment count of tabs 'scrubbed' by mouse or key press for
  // histogram data.
  if (user_gesture.type == TabStripUserGestureDetails::GestureType::kMouse ||
      user_gesture.type == TabStripUserGestureDetails::GestureType::kKeyboard) {
    base::TimeDelta tab_switch_delta =
        base::TimeTicks::Now() - last_tab_switch_timestamp_;
    if (tab_switch_delta <= kMaxTimeConsideredScrubbing) {
      if (user_gesture.type == TabStripUserGestureDetails::GestureType::kMouse)
        ++tabs_scrubbed_by_mouse_press_count_;
      else if (user_gesture.type ==
               TabStripUserGestureDetails::GestureType::kKeyboard)
        ++tabs_scrubbed_by_key_press_count_;
    }
  }
  last_tab_switch_timestamp_ = base::TimeTicks::Now();
}

void TabStripScrubbingMetrics::RecordTabScrubbingMetrics() {
  UMA_HISTOGRAM_COUNTS_10000("Tabs.ScrubbedInInterval.MousePress",
                             tabs_scrubbed_by_mouse_press_count_);
  UMA_HISTOGRAM_COUNTS_10000("Tabs.ScrubbedInInterval.KeyPress",
                             tabs_scrubbed_by_key_press_count_);
  tabs_scrubbed_by_mouse_press_count_ = 0;
  tabs_scrubbed_by_key_press_count_ = 0;
}

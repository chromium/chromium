// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_SCRUBBING_METRICS_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_SCRUBBING_METRICS_H_

#include <stddef.h>
#include <stdint.h>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"

class TabStripScrubbingMetrics {
 public:
  // Initializes the timer and timestamp.
  void Init();

  // Determines which counter to bump, and bumps it, resetting the timestamp if
  // required.
  void IncrementPressCount(const TabStripUserGestureDetails& user_gesture);

  // Logs and resets the press counts.
  void RecordTabScrubbingMetrics();

 private:
  // Timer used to mark intervals for metric collection on how many tabs are
  // scrubbed over a certain interval of time.
  base::RepeatingTimer tab_scrubbing_interval_timer_;

  // Timestamp marking the last time a tab was activated by mouse press. This is
  // used in determining how long a tab was active for metrics.
  base::TimeTicks last_tab_switch_timestamp_ = base::TimeTicks();

  // Counter used to keep track of tab scrubs during intervals set by
  // |tab_scrubbing_interval_timer_|.
  size_t tabs_scrubbed_by_mouse_press_count_ = 0;

  // Counter used to keep track of tab scrubs during intervals set by
  // |tab_scrubbing_interval_timer_|.
  size_t tabs_scrubbed_by_key_press_count_ = 0;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_SCRUBBING_METRICS_H_

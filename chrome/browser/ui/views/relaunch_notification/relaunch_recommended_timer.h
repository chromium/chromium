// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_RECOMMENDED_TIMER_H_
#define CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_RECOMMENDED_TIMER_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"

// Timer that handles notification title refresh for relaunch recommended
// notification. Created by RelaunchRecommendedBubbleView for Chrome desktop.
// Title refresh is invoked with the |callback| provided at creation.
class RelaunchRecommendedTimer {
 public:
  // |upgrade_detected_time| is used to compose current notification title
  // (see below comment).
  // |callback| is called every time the notification title has to be updated.
  RelaunchRecommendedTimer(base::Time upgrade_detected_time,
                           base::RepeatingClosure callback);

  RelaunchRecommendedTimer(const RelaunchRecommendedTimer&) = delete;
  RelaunchRecommendedTimer& operator=(const RelaunchRecommendedTimer&) = delete;

  ~RelaunchRecommendedTimer();

  // Returns current notification's title, composed depending on how much time
  // has passed since the update was detected (see above comment).
  std::u16string GetWindowTitle() const;

 private:
  // Schedules a timer to fire the next time the title text must be updated; for
  // example, from "...is available" to "...has been available for 1 day".
  void ScheduleNextTitleRefresh();

  // Invoked when the timer fires to refresh the title text.
  void OnTitleRefresh();

  // The time at which Chrome noticed that an update was available. This
  // is used to write the proper string into the dialog's title and to schedule
  // title refreshes to update said string.
  const base::Time upgrade_detected_time_;

  // A timer with which title refreshes are scheduled.
  base::WallClockTimer refresh_timer_;

  // Callback which triggers the actual title update on Chrome desktop.
  base::RepeatingClosure callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_RECOMMENDED_TIMER_H_

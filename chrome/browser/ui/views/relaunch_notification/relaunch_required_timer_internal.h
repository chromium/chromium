// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_REQUIRED_TIMER_INTERNAL_H_
#define CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_REQUIRED_TIMER_INTERNAL_H_

#include "base/time/time.h"

namespace relaunch_notification {

// Rounds |deadline_offset| to the nearest day/hour/minute/second for display
// in the notification's title.
base::TimeDelta ComputeDeadlineDelta(base::TimeDelta deadline_offset);

// Returns the offset from an arbitrary "now" into |deadline_offset| at which
// the notification's title must be refreshed.
base::TimeDelta ComputeNextRefreshDelta(base::TimeDelta deadline_offset);

}  // namespace relaunch_notification

#endif  // CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_REQUIRED_TIMER_INTERNAL_H_

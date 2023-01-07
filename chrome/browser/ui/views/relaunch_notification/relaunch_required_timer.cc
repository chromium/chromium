// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_timer.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_timer_internal.h"

RelaunchRequiredTimer::RelaunchRequiredTimer(base::Time deadline,
                                             base::RepeatingClosure callback)
    : deadline_(deadline), callback_(std::move(callback)) {
  ScheduleNextTitleRefresh();
}

RelaunchRequiredTimer::~RelaunchRequiredTimer() {}

void RelaunchRequiredTimer::ScheduleNextTitleRefresh() {
  // Refresh at the next second, minute, hour, or day boundary; depending on the
  // relaunch deadline.
  const base::Time now = base::Time::Now();
  const base::TimeDelta deadline_offset = deadline_ - now;

  // Don't start the timer if the deadline is in the past or right now.
  if (deadline_offset <= base::TimeDelta())
    return;

  const base::TimeDelta refresh_delta =
      relaunch_notification::ComputeNextRefreshDelta(deadline_offset);

  refresh_timer_.Start(FROM_HERE, now + refresh_delta, this,
                       &RelaunchRequiredTimer::OnTitleRefresh);
}

void RelaunchRequiredTimer::SetDeadline(base::Time deadline) {
  if (deadline != deadline_) {
    deadline_ = deadline;
    // Refresh the title immediately.
    OnTitleRefresh();
  }
}

base::TimeDelta RelaunchRequiredTimer::GetRoundedDeadlineDelta() const {
  return relaunch_notification::ComputeDeadlineDelta(deadline_ -
                                                     base::Time::Now());
}

void RelaunchRequiredTimer::OnTitleRefresh() {
  callback_.Run();
  ScheduleNextTitleRefresh();
}

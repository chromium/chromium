// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_timer.h"

#include <utility>

#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_timer_internal.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

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

std::u16string RelaunchRequiredTimer::GetWindowTitle() const {
  // Round the time-to-relaunch to the nearest "boundary", which may be a day,
  // hour, minute, or second. For example, two days and eighteen hours will be
  // rounded up to three days, while two days and one hour will be rounded down
  // to two days. This rounding is significant for only the initial showing of
  // the dialog. Each refresh of the title thereafter will take place at the
  // moment when the boundary value changes. For example, the title will be
  // refreshed from three days to two days when there are exactly two days
  // remaning. This scales nicely to the final seconds, when one would expect a
  // "3..2..1.." countdown to change precisely on the per-second boundaries.
  const base::TimeDelta rounded_offset =
      relaunch_notification::ComputeDeadlineDelta(deadline_ -
                                                  base::Time::Now());

  int amount = rounded_offset.InSeconds();
  int message_id = IDS_RELAUNCH_REQUIRED_TITLE_SECONDS;
  if (rounded_offset.InDays() >= 2) {
    amount = rounded_offset.InDays();
    message_id = IDS_RELAUNCH_REQUIRED_TITLE_DAYS;
  } else if (rounded_offset.InHours() >= 1) {
    amount = rounded_offset.InHours();
    message_id = IDS_RELAUNCH_REQUIRED_TITLE_HOURS;
  } else if (rounded_offset.InMinutes() >= 1) {
    amount = rounded_offset.InMinutes();
    message_id = IDS_RELAUNCH_REQUIRED_TITLE_MINUTES;
  }

  return l10n_util::GetPluralStringFUTF16(message_id, amount);
}

void RelaunchRequiredTimer::OnTitleRefresh() {
  callback_.Run();
  ScheduleNextTitleRefresh();
}

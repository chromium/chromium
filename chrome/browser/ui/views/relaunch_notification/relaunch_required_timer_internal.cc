// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_timer_internal.h"

#include <cmath>

namespace relaunch_notification {

base::TimeDelta ComputeDeadlineDelta(base::TimeDelta deadline_offset) {
  // Round deadline_offset to the nearest second for the computations below.
  deadline_offset = base::Seconds(std::round(deadline_offset.InSecondsF()));

  // At/above 47.5 hours, round up to showing N days (min 2).
  // TODO(grt): Explore ways to make this more obvious by way of new methods in
  // base::TimeDelta (e.g., float variants of FromXXX and rounding variants of
  // InXXX).
  static constexpr base::TimeDelta kMinDaysDelta = base::Minutes(47 * 60 + 30);
  // At/above 59.5 minutes, round up to showing N hours (min 1).
  static constexpr base::TimeDelta kMinHoursDelta = base::Seconds(59 * 60 + 30);
  // At/above 59.5 seconds, round up to showing N minutes (min 1).
  static constexpr base::TimeDelta kMinMinutesDelta =
      base::Milliseconds(59 * 1000 + 500);

  // Round based on the time scale.
  if (deadline_offset >= kMinDaysDelta) {
    return base::Days((deadline_offset + base::Hours(12)).InDays());
  }

  if (deadline_offset >= kMinHoursDelta) {
    return base::Hours((deadline_offset + base::Minutes(30)).InHours());
  }

  if (deadline_offset >= kMinMinutesDelta) {
    return base::Minutes((deadline_offset + base::Seconds(30)).InMinutes());
  }

  return base::Seconds((deadline_offset + base::Milliseconds(500)).InSeconds());
}

base::TimeDelta ComputeNextRefreshDelta(base::TimeDelta deadline_offset) {
  // What would be in the title now?
  const base::TimeDelta rounded_offset = ComputeDeadlineDelta(deadline_offset);

  // Compute the refresh moment to bring |rounded_offset| down to the next value
  // to be displayed. This is the moment that the title must switch from N to
  // N-1 of the same units (e.g., # of days) or from one form of units to the
  // next granular form of units (e.g., 2 days to 47 hours).
  // TODO(grt): Find a way to reduce duplication with the constants in
  // ComputeDeadlineDelta once https://crbug.com/761570 is resolved.
  static constexpr base::TimeDelta kMinDays = base::Days(2);
  static constexpr base::TimeDelta kMinHours = base::Hours(1);
  static constexpr base::TimeDelta kMinMinutes = base::Minutes(1);
  static constexpr base::TimeDelta kMinSeconds = base::Seconds(1);

  base::TimeDelta delta;
  if (rounded_offset > kMinDays)
    delta = base::Days(rounded_offset.InDays() - 1);
  else if (rounded_offset > kMinHours)
    delta = base::Hours(rounded_offset.InHours() - 1);
  else if (rounded_offset > kMinMinutes)
    delta = base::Minutes(rounded_offset.InMinutes() - 1);
  else if (rounded_offset > kMinSeconds)
    delta = base::Seconds(rounded_offset.InSeconds() - 1);

  return deadline_offset - delta;
}

}  // namespace relaunch_notification

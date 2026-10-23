// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TIME_PERIOD_FILTER_UTILS_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_TIME_PERIOD_FILTER_UTILS_H_

#include <string>
#include <vector>

#include "base/time/time.h"

namespace history {

// Identifies a predefined time period option in the history page's time period
// filter.
// Must be kept in sync with `history.mojom.TimePeriodOptionId` in
// ui/webui/resources/cr_components/history/history.mojom.
// LINT.IfChange(TimePeriodOptionId)
enum class TimePeriodOptionId {
  // No time period restriction. This is the default option.
  kAllTime = 0,
  // The day before today.
  kYesterday = 1,
  // Monday of the current week up to now.
  kEarlierThisWeek = 2,
  // The previous calendar week (Monday to Sunday).
  kLastWeek = 3,
  // The calendar week (Monday to Sunday) before the previous one.
  kTwoWeeksAgo = 4,
  kMaxValue = kTwoWeeksAgo,
};
// LINT.ThenChange(//ui/webui/resources/cr_components/history/history.mojom:TimePeriodOptionId)

// A predefined time period the user can select on the history page to restrict
// the shown history to. The time period maps directly onto
// `QueryOptions::begin_time` and `QueryOptions::end_time`.
struct TimePeriodOption {
  TimePeriodOptionId id = TimePeriodOptionId::kAllTime;

  // The localized label shown to the user in the filter UI.
  std::u16string label;

  // The start of the time period (inclusive). Null if unbounded.
  base::Time begin_time;

  // The end of the time period (exclusive). Null if unbounded.
  base::Time end_time;

  friend bool operator==(const TimePeriodOption&,
                         const TimePeriodOption&) = default;
};

// Computes the predefined time period options for the history page's time
// period filter relative to `now`, in the local time zone. Weeks start on
// Monday. The returned list is always in the same order:
// "All time", "Yesterday", "Earlier this week", "Last week", "2 weeks ago".
// In the unlikely event that `now` can't be converted to a local calendar
// date, only the "All time" option is returned.
std::vector<TimePeriodOption> ComputeTimePeriodOptions(base::Time now);

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TIME_PERIOD_FILTER_UTILS_H_

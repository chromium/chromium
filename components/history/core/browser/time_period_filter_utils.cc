// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/time_period_filter_utils.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "components/history/core/browser/features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace history {

namespace {

// Returns local midnight of the day `days` days before the day that `midnight`
// (a local midnight) falls on. Adding half a day before re-snapping to midnight
// makes this robust against DST transitions, which can make a day 23 or 25
// hours long.
base::Time LocalMidnightDaysBefore(base::Time midnight, int days) {
  return (midnight - base::Days(days) + base::Hours(12)).LocalMidnight();
}

}  // namespace

std::vector<TimePeriodOption> ComputeTimePeriodOptions(base::Time now) {
  // If feature is disabled, return an empty list.
  if (!base::FeatureList::IsEnabled(history::kBrowsingHistoryFilterByTime)) {
    return {};
  }

  TimePeriodOption all_time{
      .id = TimePeriodOptionId::kAllTime,
      .label =
          l10n_util::GetStringUTF16(IDS_HISTORY_TIME_PERIOD_FILTER_ALL_TIME),
      .begin_time = base::Time(),
      .end_time = base::Time(),
  };

  // Weeks start on Monday. `base::Time::Exploded::day_of_week` is 0-based
  // starting from Sunday.
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);
  if (!exploded.HasValidValues()) {
    // Without a valid day of week the week-based options can't be computed, so
    // degrade gracefully by offering only the unfiltered option.
    return {std::move(all_time)};
  }

  const base::Time today = now.LocalMidnight();
  const base::Time yesterday = LocalMidnightDaysBefore(today, 1);
  const int days_since_monday = (exploded.day_of_week + 6) % 7;
  const base::Time this_week_start =
      LocalMidnightDaysBefore(today, days_since_monday);
  const base::Time last_week_start =
      LocalMidnightDaysBefore(this_week_start, 7);
  const base::Time two_weeks_ago_start =
      LocalMidnightDaysBefore(last_week_start, 7);

  return {
      std::move(all_time),
      TimePeriodOption{
          .id = TimePeriodOptionId::kYesterday,
          .label = l10n_util::GetStringUTF16(
              IDS_HISTORY_TIME_PERIOD_FILTER_YESTERDAY),
          .begin_time = yesterday,
          .end_time = today,
      },
      TimePeriodOption{
          .id = TimePeriodOptionId::kEarlierThisWeek,
          .label = l10n_util::GetStringUTF16(
              IDS_HISTORY_TIME_PERIOD_FILTER_EARLIER_THIS_WEEK),
          .begin_time = this_week_start,
          .end_time = now,
      },
      TimePeriodOption{
          .id = TimePeriodOptionId::kLastWeek,
          .label = l10n_util::GetStringUTF16(
              IDS_HISTORY_TIME_PERIOD_FILTER_LAST_WEEK),
          .begin_time = last_week_start,
          .end_time = this_week_start,
      },
      TimePeriodOption{
          .id = TimePeriodOptionId::kTwoWeeksAgo,
          .label = l10n_util::GetStringUTF16(
              IDS_HISTORY_TIME_PERIOD_FILTER_TWO_WEEKS_AGO),
          .begin_time = two_weeks_ago_start,
          .end_time = last_week_start,
      },
  };
}

}  // namespace history

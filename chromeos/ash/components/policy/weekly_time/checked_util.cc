// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/weekly_time/checked_util.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval_checked.h"

namespace policy::weekly_time {

std::optional<std::vector<WeeklyTimeIntervalChecked>> ExtractIntervalsFromList(
    const base::Value::List& list) {
  std::vector<WeeklyTimeIntervalChecked> intervals;

  for (const auto& interval_value : list) {
    if (!interval_value.is_dict()) {
      LOG(ERROR) << "Interval not a dict: " << interval_value.DebugString();
      return std::nullopt;
    }
    const auto& interval_dict = interval_value.GetDict();

    auto interval = WeeklyTimeIntervalChecked::FromDict(interval_dict);
    if (!interval.has_value()) {
      LOG(ERROR) << "Couldn't parse interval: " << interval_dict.DebugString();
      return std::nullopt;
    }

    intervals.push_back(interval.value());
  }

  return intervals;
}

bool IntervalsContainTime(
    const std::vector<WeeklyTimeIntervalChecked>& intervals,
    const WeeklyTimeChecked& time) {
  for (const auto& interval : intervals) {
    if (interval.Contains(time)) {
      return true;
    }
  }
  return false;
}

std::optional<WeeklyTimeChecked> GetNextEvent(
    const std::vector<WeeklyTimeIntervalChecked>& intervals,
    const WeeklyTimeChecked& time) {
  std::optional<WeeklyTimeChecked> next_event = std::nullopt;
  // Sentinel value set to max which is 1 week.
  base::TimeDelta min_duration = base::Days(7);
  for (const auto& interval : intervals) {
    for (const WeeklyTimeChecked& event : {interval.start(), interval.end()}) {
      const base::TimeDelta duration =
          WeeklyTimeIntervalChecked(time, event).Duration();
      if (duration < min_duration) {
        min_duration = duration;
        next_event = event;
      }
    }
  }
  return next_event;
}

std::optional<base::TimeDelta> GetDurationToNextEvent(
    const std::vector<WeeklyTimeIntervalChecked>& intervals,
    const WeeklyTimeChecked& time) {
  std::optional<WeeklyTimeChecked> next_event = GetNextEvent(intervals, time);
  if (!next_event.has_value()) {
    return std::nullopt;
  }
  return WeeklyTimeIntervalChecked(time, next_event.value()).Duration();
}

}  // namespace policy::weekly_time

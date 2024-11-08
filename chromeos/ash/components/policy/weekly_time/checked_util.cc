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

namespace {

std::optional<base::Time> UTCToLocal(base::Time utc) {
  base::Time::Exploded utc_exploded_local;
  utc.LocalExplode(&utc_exploded_local);
  if (!utc_exploded_local.HasValidValues()) {
    return std::nullopt;
  }

  base::Time utc_exploded_local_unexploded_utc;
  if (!base::Time::FromUTCExploded(utc_exploded_local,
                                   &utc_exploded_local_unexploded_utc)) {
    return std::nullopt;
  }

  return utc_exploded_local_unexploded_utc;
}

std::optional<base::Time> LocalToUTC(base::Time local) {
  base::Time::Exploded local_exploded_utc;
  local.UTCExplode(&local_exploded_utc);
  if (!local_exploded_utc.HasValidValues()) {
    return std::nullopt;
  }

  base::Time local_exploded_utc_unexploded_local;
  if (!base::Time::FromLocalExploded(local_exploded_utc,
                                     &local_exploded_utc_unexploded_local)) {
    return std::nullopt;
  }

  return local_exploded_utc_unexploded_local;
}

}  // namespace

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

base::Time AddOffsetInLocalTime(base::Time utc, base::TimeDelta offset) {
  std::optional<base::Time> local = UTCToLocal(utc);
  if (!local.has_value()) {
    return utc + offset;
  }

  base::Time local_with_offset = local.value() + offset;

  std::optional<base::Time> utc_with_offset = LocalToUTC(local_with_offset);
  if (!utc_with_offset.has_value()) {
    return utc + offset;
  }

  return utc_with_offset.value();
}

}  // namespace policy::weekly_time

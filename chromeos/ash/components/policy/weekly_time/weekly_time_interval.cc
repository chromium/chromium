// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"

#include "base/logging.h"
#include "base/time/time.h"

namespace em = enterprise_management;

namespace policy {

// static
const char WeeklyTimeInterval::kStart[] = "start";
const char WeeklyTimeInterval::kEnd[] = "end";

WeeklyTimeInterval::WeeklyTimeInterval(const WeeklyTime& start,
                                       const WeeklyTime& end)
    : start_(start), end_(end) {
  DCHECK_GT(start.GetDurationTo(end), base::TimeDelta());
  DCHECK(start.timezone_offset() == end.timezone_offset());
}

WeeklyTimeInterval::WeeklyTimeInterval(const WeeklyTimeInterval& rhs) = default;

WeeklyTimeInterval& WeeklyTimeInterval::operator=(
    const WeeklyTimeInterval& rhs) = default;

base::Value WeeklyTimeInterval::ToValue() const {
  base::Value interval(base::Value::Type::DICT);
  interval.GetDict().Set(kStart, start_.ToValue());
  interval.GetDict().Set(kEnd, end_.ToValue());
  return interval;
}

bool WeeklyTimeInterval::Contains(const WeeklyTime& w) const {
  DCHECK_EQ(start_.timezone_offset().has_value(),
            w.timezone_offset().has_value());
  if (w.GetDurationTo(end_).is_zero())
    return false;
  base::TimeDelta interval_duration = start_.GetDurationTo(end_);
  return start_.GetDurationTo(w) + w.GetDurationTo(end_) == interval_duration;
}

// static
std::unique_ptr<WeeklyTimeInterval> WeeklyTimeInterval::ExtractFromProto(
    const em::WeeklyTimeIntervalProto& container,
    absl::optional<int> timezone_offset) {
  if (!container.has_start() || !container.has_end()) {
    LOG(WARNING) << "Interval without start or/and end.";
    return nullptr;
  }
  auto start = WeeklyTime::ExtractFromProto(container.start(), timezone_offset);
  auto end = WeeklyTime::ExtractFromProto(container.end(), timezone_offset);
  if (!start || !end)
    return nullptr;
  return std::make_unique<WeeklyTimeInterval>(*start, *end);
}

// static
std::unique_ptr<WeeklyTimeInterval> WeeklyTimeInterval::ExtractFromDict(
    const base::Value::Dict& dict,
    absl::optional<int> timezone_offset) {
  const base::Value* start_value = dict.Find(kStart);
  if (!start_value) {
    LOG(WARNING) << "Interval without start.";
    return nullptr;
  }
  const base::Value* end_value = dict.Find(kEnd);
  if (!end_value) {
    LOG(WARNING) << "Interval without end.";
    return nullptr;
  }

  auto start =
      WeeklyTime::ExtractFromDict(start_value->GetDict(), timezone_offset);
  auto end = WeeklyTime::ExtractFromDict(end_value->GetDict(), timezone_offset);
  if (!start || !end)
    return nullptr;
  return std::make_unique<WeeklyTimeInterval>(*start, *end);
}

}  // namespace policy

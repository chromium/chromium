// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/policy/weekly_time/weekly_time_interval.h"

#include "base/logging.h"
#include "base/time/time.h"

namespace em = enterprise_management;

namespace policy {

WeeklyTimeInterval::WeeklyTimeInterval(const WeeklyTime& start,
                                       const WeeklyTime& end)
    : start_(start), end_(end) {
  DCHECK_GT(start.GetDurationTo(end), base::TimeDelta());
  DCHECK(start.timezone_offset() == end.timezone_offset());
}

WeeklyTimeInterval::WeeklyTimeInterval(const WeeklyTimeInterval& rhs) = default;

WeeklyTimeInterval& WeeklyTimeInterval::operator=(
    const WeeklyTimeInterval& rhs) = default;

std::unique_ptr<base::DictionaryValue> WeeklyTimeInterval::ToValue() const {
  auto interval = std::make_unique<base::DictionaryValue>();
  interval->SetDictionary("start", start_.ToValue());
  interval->SetDictionary("end", end_.ToValue());
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
    base::Optional<int> timezone_offset) {
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

}  // namespace policy

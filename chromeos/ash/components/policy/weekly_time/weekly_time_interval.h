// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_WEEKLY_TIME_INTERVAL_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_WEEKLY_TIME_INTERVAL_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

namespace policy {

// Represents non-empty time interval [start, end) between two weekly times.
// Interval can be wrapped across the end of the week.
// Interval is empty if start = end. Empty intervals aren't allowed.
// Both WeeklyTimes need to have the same timezone_offset.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY) WeeklyTimeInterval {
 public:
  // Dictionary value key constants for testing.
  static const char kStart[];
  static const char kEnd[];

  // Determines if `interval_a` and `interval_b` have any overlap in time.
  // Intervals are considered half-open (i.e., the start time is included, the
  // end time is not).
  static bool IntervalsOverlap(const WeeklyTimeInterval& interval_a,
                               const WeeklyTimeInterval& interval_b);

  // Return time interval made from WeeklyTimeIntervalProto structure. Return
  // nullptr if the proto contains an invalid interval.
  static std::unique_ptr<WeeklyTimeInterval> ExtractFromProto(
      const enterprise_management::WeeklyTimeIntervalProto& container,
      std::optional<int> timezone_offset);

  // Return time interval made from Value::Dict in format:
  // { "start" : WeeklyTime,
  //   "end" : WeeklyTime }
  // WeeklyTime dictionary format:
  // { "day_of_week" : int # value is from 1 to 7 (1 = Monday, 2 = Tuesday,
  // etc.)
  //   "time" : int # in milliseconds from the beginning of the day.
  //   "timezone_offset" : int # in milliseconds, how much time ahead of GMT.
  // }
  // Return nullptr if `dict` contains an invalid interval.
  static std::unique_ptr<WeeklyTimeInterval> ExtractFromDict(
      const base::Value::Dict& dict,
      std::optional<int> timezone_offset);

  WeeklyTimeInterval(const WeeklyTime& start, const WeeklyTime& end);

  WeeklyTimeInterval(const WeeklyTimeInterval& rhs);

  WeeklyTimeInterval& operator=(const WeeklyTimeInterval& rhs);

  bool operator==(const WeeklyTimeInterval& rhs) const {
    return start_ == rhs.start() && end_ == rhs.end();
  }

  // Return a Dictionary type Value in format:
  // { "start" : WeeklyTime,
  //   "end" : WeeklyTime }
  // WeeklyTime dictionary format:
  // { "day_of_week" : int # value is from 1 to 7 (1 = Monday, 2 = Tuesday,
  // etc.)
  //   "time" : int # in milliseconds from the beginning of the day.
  //   "timezone_offset" : int # in milliseconds, how much time ahead of GMT.
  // }
  base::Value ToValue() const;

  // Check if |w| is in [WeeklyTimeIntervall.start, WeeklyTimeInterval.end).
  // |end| time is always after |start| time. It's possible because week time is
  // cyclic. (i.e. [Friday 17:00, Monday 9:00) )
  // |w| must be in the same type of timezone as the interval (timezone agnostic
  // or in a set timezone).
  bool Contains(const WeeklyTime& w) const;

  WeeklyTime start() const { return start_; }

  WeeklyTime end() const { return end_; }

 private:
  WeeklyTime start_;
  WeeklyTime end_;
};

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_WEEKLY_TIME_INTERVAL_H_

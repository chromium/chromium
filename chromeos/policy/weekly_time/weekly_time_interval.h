// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_POLICY_WEEKLY_TIME_WEEKLY_TIME_INTERVAL_H_
#define CHROMEOS_POLICY_WEEKLY_TIME_WEEKLY_TIME_INTERVAL_H_

#include <memory>

#include "base/optional.h"
#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/policy/weekly_time/weekly_time.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

namespace policy {

// Represents non-empty time interval [start, end) between two weekly times.
// Interval can be wrapped across the end of the week.
// Interval is empty if start = end. Empty intervals aren't allowed.
// Both WeeklyTimes need to have the same timezone_offset.
class CHROMEOS_EXPORT WeeklyTimeInterval {
 public:
  WeeklyTimeInterval(const WeeklyTime& start, const WeeklyTime& end);

  WeeklyTimeInterval(const WeeklyTimeInterval& rhs);

  WeeklyTimeInterval& operator=(const WeeklyTimeInterval& rhs);

  bool operator==(const WeeklyTimeInterval& rhs) const {
    return start_ == rhs.start() && end_ == rhs.end();
  }

  // Return DictionaryValue in format:
  // { "start" : WeeklyTime,
  //   "end" : WeeklyTime }
  // WeeklyTime dictionary format:
  // { "day_of_week" : int # value is from 1 to 7 (1 = Monday, 2 = Tuesday,
  // etc.)
  //   "time" : int # in milliseconds from the beginning of the day.
  //   "timezone_offset" : int # in milliseconds, how much time ahead of GMT.
  // }
  std::unique_ptr<base::DictionaryValue> ToValue() const;

  // Check if |w| is in [WeeklyTimeIntervall.start, WeeklyTimeInterval.end).
  // |end| time is always after |start| time. It's possible because week time is
  // cyclic. (i.e. [Friday 17:00, Monday 9:00) )
  // |w| must be in the same type of timezone as the interval (timezone agnostic
  // or in a set timezone).
  bool Contains(const WeeklyTime& w) const;

  // Returns the timezone_offset that |start_| and |end_| have.
  base::Optional<int> GetIntervalOffset(int timezone_offset) const {
    return start_.timezone_offset();
  }

  // Return time interval made from WeeklyTimeIntervalProto structure. Return
  // nullptr if the proto contains an invalid interval.
  static std::unique_ptr<WeeklyTimeInterval> ExtractFromProto(
      const enterprise_management::WeeklyTimeIntervalProto& container,
      base::Optional<int> timezone_offset);

  WeeklyTime start() const { return start_; }

  WeeklyTime end() const { return end_; }

 private:
  WeeklyTime start_;
  WeeklyTime end_;
};

}  // namespace policy

#endif  // CHROMEOS_POLICY_WEEKLY_TIME_WEEKLY_TIME_INTERVAL_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_POLICY_WEEKLY_TIME_WEEKLY_TIME_H_
#define CHROMEOS_POLICY_WEEKLY_TIME_WEEKLY_TIME_H_

#include <memory>

#include "base/optional.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

namespace policy {

// WeeklyTime class contains day of week and time. Day of week is number from 1
// to 7 (1 = Monday, 2 = Tuesday, etc.) Time is in milliseconds from the
// beginning of the day.
class CHROMEOS_EXPORT WeeklyTime {
 public:
  WeeklyTime(int day_of_week,
             int milliseconds,
             base::Optional<int> timezone_offset);

  WeeklyTime(const WeeklyTime& rhs);

  WeeklyTime& operator=(const WeeklyTime& rhs);

  bool operator==(const WeeklyTime& rhs) const {
    return day_of_week_ == rhs.day_of_week() &&
           milliseconds_ == rhs.milliseconds() &&
           timezone_offset_ == rhs.timezone_offset();
  }

  // Return DictionaryValue in format:
  // { "day_of_week" : int # value is from 1 to 7 (1 = Monday, 2 = Tuesday,
  // etc.)
  //   "time" : int # in milliseconds from the beginning of the day.
  // }
  std::unique_ptr<base::DictionaryValue> ToValue() const;

  int day_of_week() const { return day_of_week_; }

  int milliseconds() const { return milliseconds_; }

  base::Optional<int> timezone_offset() const { return timezone_offset_; }

  // Return duration from |start| till |end| week times. |end| time
  // is always after |start| time. It's possible because week time is cyclic.
  // (i.e. [Friday 17:00, Monday 9:00) )
  // Both WeeklyTimes have to have the same kind of timezone (timezone agnostic
  // or set timezone).
  base::TimeDelta GetDurationTo(const WeeklyTime& other) const;

  // Add milliseconds to WeeklyTime.
  WeeklyTime AddMilliseconds(int milliseconds) const;

  // Creates a new WeeklyTime from the current one that has
  // |timezone_offset_| set to |timezone_offset|. Furthermore, the new
  // WeeklyTime is offset based on the difference between the timezones. This
  // function should only take in WeeklyTimes with a defined timezone (i.e. not
  // nullopt).
  WeeklyTime ConvertToTimezone(int timezone_offset) const;

  // Creates a new WeeklyTime that has |timezone_offset_| set to
  // |timezone_offset|. This function is to be used to set the timezone of
  // timezone agnostic WeeklyTime objects, i.e. objects where |timezone_offset_|
  // == nullopt.
  WeeklyTime ConvertToCustomTimezone(int timezone_offset) const;

  // Return WeeklyTime structure from WeeklyTimeProto. Return nullptr if
  // WeeklyTime structure isn't correct.
  static std::unique_ptr<WeeklyTime> ExtractFromProto(
      const enterprise_management::WeeklyTimeProto& container,
      base::Optional<int> timezone_offset);

  // Return the current time in GMT in WeeklyTime structure.
  static WeeklyTime GetCurrentGmtWeeklyTime(base::Clock* clock);

  // Return the current time in the system's local time in WeeklyTime structure.
  static WeeklyTime GetCurrentLocalWeeklyTime(base::Clock* clock);

 private:
  // Number of weekday (1 = Monday, 2 = Tuesday, etc.)
  int day_of_week_;

  // Time of day in milliseconds from the beginning of the day.
  int milliseconds_;

  // Offset from GMT in milliseconds for the timezone that the Weekly Time is
  // in. If |timezone_offset_| is 0 then it the WeeklyTime will be considered to
  // be in GMT. If |timezone_offset_| is non-zero, then the WeeklyTime will be
  // considered to be in the timezone corresponding to that offset. If
  // |timezone_offset_| is |nullopt|, then it will be interpreted to be in the
  // system's local timezone.
  base::Optional<int> timezone_offset_;
};

}  // namespace policy

#endif  // CHROMEOS_POLICY_WEEKLY_TIME_WEEKLY_TIME_H_

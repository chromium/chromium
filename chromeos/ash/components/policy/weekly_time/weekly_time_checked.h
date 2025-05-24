// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_WEEKLY_TIME_CHECKED_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_WEEKLY_TIME_CHECKED_H_

#include <array>
#include <optional>

#include "base/component_export.h"
#include "base/time/time.h"
#include "base/values.h"

namespace policy {

// `WeeklyTimeChecked` corresponds to the same type from
// `components/policy/resources/templates/common_schemas.yaml`. It is used to
// read and unpack the policy value into a proper C++ type.
//
// It contains the day of the week (number from 1 to 7; 1 = Monday, 2 = Tuesday,
// etc.) and the time (milliseconds since midnight).
//
// It is different from `WeeklyTime` in that it is checked automatically during
// policy decoding for validity, and it also doesn't have the timezone info
// tacked on top.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY) WeeklyTimeChecked {
 public:
  // Test support.
  static const char kDayOfWeek[];
  static const char kMillisecondsSinceMidnight[];
  static std::array<const char*, 7> kWeekDays;

  // Explicit numbering because we map 1-to-1 with `WeekDay` from
  // `common_schemas.yaml`.
  // TODO(b/345186543, isandrk): Consider separating `Day` into its own class in
  // a separate file. More type safety and better logical encapsulation.
  enum class Day {
    kMonday = 1,
    kTuesday = 2,
    kWednesday = 3,
    kThursday = 4,
    kFriday = 5,
    kSaturday = 6,
    kSunday = 7,
  };

  static Day NextDay(Day day);

  WeeklyTimeChecked(Day day_of_week, int milliseconds_since_midnight);
  WeeklyTimeChecked(const WeeklyTimeChecked&);
  WeeklyTimeChecked& operator=(const WeeklyTimeChecked&);

  friend bool operator==(const WeeklyTimeChecked&,
                         const WeeklyTimeChecked&) = default;

  // Constructs from a Value::Dict:
  // {
  //   "day_of_week": int,
  //   "milliseconds_since_midnight": int
  // }.
  static std::optional<WeeklyTimeChecked> FromDict(
      const base::Value::Dict& dict);

  // Constructs from an exploded base::Time.
  static WeeklyTimeChecked FromExploded(const base::Time::Exploded& exploded);

  // Constructs from `time` in local time (as opposed to GMT, PST, etc.).
  static WeeklyTimeChecked FromTimeAsLocalTime(base::Time time);

  // Constructs from `time_delta`. Opposite of `ToTimeDelta()`.
  static WeeklyTimeChecked FromTimeDelta(base::TimeDelta time_delta);

  // Convert to base::TimeDelta. (Monday, 0) = 0, (Monday, 5h) = 5h,
  // (Tuesday, 0) = 1d, (Tuesday, 5m) = 1d + 5m, etc.
  base::TimeDelta ToTimeDelta() const;

  Day day_of_week() const { return day_of_week_; }

  int milliseconds_since_midnight() const {
    return milliseconds_since_midnight_;
  }

 private:
  Day day_of_week_;
  int milliseconds_since_midnight_;
};

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_WEEKLY_TIME_CHECKED_H_

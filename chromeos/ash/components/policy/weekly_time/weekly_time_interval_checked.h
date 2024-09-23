// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_WEEKLY_TIME_INTERVAL_CHECKED_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_WEEKLY_TIME_INTERVAL_CHECKED_H_

#include <optional>

#include "base/component_export.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_checked.h"

namespace policy {

// `WeeklyTimeIntervalChecked` corresponds to the same type from
// `components/policy/resources/templates/common_schemas.yaml`. It is used to
// read and unpack the policy value into a proper C++ type.
//
// It represents a non-empty time interval [start, end) between two weekly
// times. It can be wrapped around the end of the week.
//
// It is different from `WeeklyTimeInterval` in that it is checked automatically
// during policy decoding for validity, and it also doesn't have the timezone
// info tacked on top.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
    WeeklyTimeIntervalChecked {
 public:
  // Test support.
  static const char kStart[];
  static const char kEnd[];

  WeeklyTimeIntervalChecked(const WeeklyTimeChecked& start,
                            const WeeklyTimeChecked& end);
  WeeklyTimeIntervalChecked(const WeeklyTimeIntervalChecked&);
  WeeklyTimeIntervalChecked& operator=(const WeeklyTimeIntervalChecked&);

  friend bool operator==(const WeeklyTimeIntervalChecked&,
                         const WeeklyTimeIntervalChecked&) = default;

  // Determines if `interval_a` and `interval_b` have any overlap in time.
  // Intervals are considered half-open (i.e., the start time is included, the
  // end time is not).
  static bool IntervalsOverlap(const WeeklyTimeIntervalChecked& a,
                               const WeeklyTimeIntervalChecked& b);

  // Constructs from a Value::Dict:
  // {
  //   "start": WeeklyTimeChecked,
  //   "end": WeeklyTimeChecked
  // }
  static std::optional<WeeklyTimeIntervalChecked> FromDict(
      const base::Value::Dict& dict);

  // Duration of the current interval [start_, end_). NB: Duration of interval
  // where `start_` == `end_` is defined as 1 week and not 0.
  base::TimeDelta Duration() const;

  // Check if `w` is in [`start_`, `end_`).
  bool Contains(const WeeklyTimeChecked& w) const;

  WeeklyTimeChecked start() const { return start_; }

  WeeklyTimeChecked end() const { return end_; }

 private:
  WeeklyTimeChecked start_;
  WeeklyTimeChecked end_;
};

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_WEEKLY_TIME_WEEKLY_TIME_INTERVAL_CHECKED_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/weekly_time/weekly_time_checked.h"

#include <array>
#include <optional>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/values.h"

namespace policy {

// static
const char WeeklyTimeChecked::kDayOfWeek[] = "day_of_week";
const char WeeklyTimeChecked::kMillisecondsSinceMidnight[] =
    "milliseconds_since_midnight";
std::array<const char*, 7> WeeklyTimeChecked::kWeekDays = {
    "MONDAY", "TUESDAY",  "WEDNESDAY", "THURSDAY",
    "FRIDAY", "SATURDAY", "SUNDAY"};

// static
WeeklyTimeChecked::Day WeeklyTimeChecked::NextDay(Day day) {
  int next_value = static_cast<int>(day) % 7 + 1;
  return static_cast<Day>(next_value);
}

WeeklyTimeChecked::WeeklyTimeChecked(Day day_of_week,
                                     int milliseconds_since_midnight)
    : day_of_week_(day_of_week),
      milliseconds_since_midnight_(milliseconds_since_midnight) {}

WeeklyTimeChecked::WeeklyTimeChecked(const WeeklyTimeChecked&) = default;

WeeklyTimeChecked& WeeklyTimeChecked::operator=(const WeeklyTimeChecked&) =
    default;

// static
std::optional<WeeklyTimeChecked> WeeklyTimeChecked::FromDict(
    const base::Value::Dict& dict) {
  auto* day_of_week_str = dict.FindString(kDayOfWeek);
  if (!day_of_week_str) {
    LOG(ERROR) << "Missing day_of_week.";
    return std::nullopt;
  }

  int day_of_week_value =
      base::ranges::find(kWeekDays, *day_of_week_str) - kWeekDays.begin() + 1;
  if (day_of_week_value < 1 || day_of_week_value > 7) {
    LOG(ERROR) << "Invalid day_of_week: " << *day_of_week_str;
    return std::nullopt;
  }
  Day day_of_week = static_cast<Day>(day_of_week_value);

  auto milliseconds = dict.FindInt(kMillisecondsSinceMidnight);
  if (!milliseconds.has_value()) {
    LOG(ERROR) << "Missing milliseconds_since_midnight.";
    return std::nullopt;
  }

  if (milliseconds.value() < 0 ||
      milliseconds.value() >= base::Time::kMillisecondsPerDay) {
    LOG(ERROR) << "Invalid milliseconds_since_midnight value: "
               << milliseconds.value() << ", the value should be in range [0, "
               << base::Time::kMillisecondsPerDay << ").";
    return std::nullopt;
  }

  return WeeklyTimeChecked(day_of_week, milliseconds.value());
}

// static
WeeklyTimeChecked WeeklyTimeChecked::FromExploded(
    const base::Time::Exploded& exploded) {
  int day_of_week = exploded.day_of_week == 0 ? 7 : exploded.day_of_week;
  int milliseconds = static_cast<int>(
      base::Hours(exploded.hour).InMilliseconds() +
      base::Minutes(exploded.minute).InMilliseconds() +
      base::Seconds(exploded.second).InMilliseconds() + exploded.millisecond);
  return WeeklyTimeChecked(static_cast<Day>(day_of_week), milliseconds);
}

// static
WeeklyTimeChecked WeeklyTimeChecked::FromTimeAsLocalTime(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return FromExploded(exploded);
}

// static
WeeklyTimeChecked WeeklyTimeChecked::FromTimeDelta(base::TimeDelta time_delta) {
  time_delta %= base::Days(7);
  if (time_delta.is_negative()) {
    time_delta += base::Days(7);
  }

  int day = time_delta.InDays();
  time_delta -= base::Days(day);
  int millis = time_delta.InMilliseconds();

  // Add one to day since Monday starts at 1, not at 0.
  return WeeklyTimeChecked(static_cast<Day>(day + 1), millis);
}

base::TimeDelta WeeklyTimeChecked::ToTimeDelta() const {
  return base::Days(static_cast<int>(day_of_week_) - 1) +
         base::Milliseconds(milliseconds_since_midnight_);
}

}  // namespace policy

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/weekly_time/weekly_time.h"

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr int64_t kMillisecondsPerWeek = base::Days(7).InMilliseconds();

}  // namespace

// static
const char WeeklyTime::kDayOfWeek[] = "day_of_week";
const char WeeklyTime::kTime[] = "time";
const char WeeklyTime::kTimezoneOffset[] = "timezone_offset";

const std::vector<std::string> WeeklyTime::kWeekDays = {
    "UNSPECIFIED", "MONDAY", "TUESDAY",  "WEDNESDAY",
    "THURSDAY",    "FRIDAY", "SATURDAY", "SUNDAY"};

WeeklyTime::WeeklyTime(int day_of_week,
                       int milliseconds,
                       std::optional<int> timezone_offset)
    : day_of_week_(day_of_week),
      milliseconds_(milliseconds),
      timezone_offset_(timezone_offset) {
  DCHECK_GT(day_of_week, 0);
  DCHECK_LE(day_of_week, 7);
  DCHECK_GE(milliseconds, 0);
  DCHECK_LT(milliseconds, base::Time::kMillisecondsPerDay);
}

WeeklyTime::WeeklyTime(const WeeklyTime& rhs) = default;

WeeklyTime& WeeklyTime::operator=(const WeeklyTime& rhs) = default;

base::Value WeeklyTime::ToValue() const {
  base::Value weekly_time(base::Value::Type::DICT);
  weekly_time.GetDict().Set(kDayOfWeek, day_of_week_);
  weekly_time.GetDict().Set(kTime, milliseconds_);
  if (timezone_offset_)
    weekly_time.GetDict().Set(kTimezoneOffset, timezone_offset_.value());
  return weekly_time;
}

base::TimeDelta WeeklyTime::GetDurationTo(const WeeklyTime& other) const {
  // Can't compare timezone agnostic intervals and non-timezone agnostic
  // intervals.
  DCHECK_EQ(timezone_offset_.has_value(), other.timezone_offset().has_value());
  WeeklyTime other_converted =
      timezone_offset_ ? other.ConvertToTimezone(timezone_offset_.value())
                       : other;
  int duration = base::Days(other_converted.day_of_week() - day_of_week_)
                     .InMilliseconds() +
                 other_converted.milliseconds() - milliseconds_;
  if (duration < 0) {
    duration += kMillisecondsPerWeek;
  }
  return base::Milliseconds(duration);
}

WeeklyTime WeeklyTime::AddMilliseconds(int milliseconds) const {
  milliseconds %= kMillisecondsPerWeek;
  // Make |milliseconds| positive number (add number of milliseconds per week)
  // for easier evaluation.
  milliseconds += kMillisecondsPerWeek;
  int shifted_milliseconds = milliseconds_ + milliseconds;
  // Get milliseconds from the start of the day.
  int result_milliseconds =
      shifted_milliseconds % base::Time::kMillisecondsPerDay;
  int day_offset = shifted_milliseconds / base::Time::kMillisecondsPerDay;
  // Convert day of week considering week is cyclic. +/- 1 is
  // because day of week is from 1 to 7.
  int result_day_of_week = (day_of_week_ + day_offset - 1) % 7 + 1;
  // AddMilliseconds should preserve the timezone.
  return WeeklyTime(result_day_of_week, result_milliseconds, timezone_offset_);
}

WeeklyTime WeeklyTime::ConvertToTimezone(int timezone_offset) const {
  DCHECK(timezone_offset_);
  return WeeklyTime(day_of_week_, milliseconds_, timezone_offset)
      .AddMilliseconds(timezone_offset - timezone_offset_.value());
}

// static
WeeklyTime WeeklyTime::GetGmtWeeklyTime(base::Time time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  return GetWeeklyTimeFromExploded(exploded, 0);
}

// static
WeeklyTime WeeklyTime::GetLocalWeeklyTime(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  WeeklyTime result = GetWeeklyTimeFromExploded(exploded, std::nullopt);
  return result;
}

// static
std::unique_ptr<WeeklyTime> WeeklyTime::ExtractFromProto(
    const em::WeeklyTimeProto& container,
    std::optional<int> timezone_offset) {
  if (!container.has_day_of_week() ||
      container.day_of_week() == em::WeeklyTimeProto::DAY_OF_WEEK_UNSPECIFIED) {
    LOG(ERROR) << "Day of week is absent or unspecified.";
    return nullptr;
  }
  if (!container.has_time()) {
    LOG(ERROR) << "Time is absent.";
    return nullptr;
  }
  int time_of_day = container.time();
  if (!(time_of_day >= 0 && time_of_day < base::Time::kMillisecondsPerDay)) {
    LOG(ERROR) << "Invalid time value: " << time_of_day
               << ", the value should be in [0; "
               << base::Time::kMillisecondsPerDay << ").";
    return nullptr;
  }
  return std::make_unique<WeeklyTime>(container.day_of_week(), time_of_day,
                                      timezone_offset);
}

// static
std::unique_ptr<WeeklyTime> WeeklyTime::ExtractFromDict(
    const base::Value::Dict& dict,
    std::optional<int> timezone_offset) {
  auto* day_of_week = dict.FindString(kDayOfWeek);
  if (!day_of_week) {
    LOG(ERROR) << "day_of_week is absent.";
    return nullptr;
  }
  int day_of_week_value =
      base::ranges::find(kWeekDays, *day_of_week) - kWeekDays.begin();
  if (day_of_week_value <= 0 || day_of_week_value > 7) {
    LOG(ERROR) << "Invalid day_of_week: " << day_of_week;
    return nullptr;
  }
  auto time_of_day = dict.FindInt(kTime);
  if (!time_of_day.has_value()) {
    LOG(ERROR) << "Time is absent";
    return nullptr;
  }

  if (!(time_of_day.value() >= 0 &&
        time_of_day.value() < base::Time::kMillisecondsPerDay)) {
    LOG(ERROR) << "Invalid time value: " << time_of_day.value()
               << ", the value should be in [0; "
               << base::Time::kMillisecondsPerDay << ").";
    return nullptr;
  }
  return std::make_unique<WeeklyTime>(day_of_week_value, time_of_day.value(),
                                      timezone_offset);
}

WeeklyTime GetWeeklyTimeFromExploded(const base::Time::Exploded& exploded,
                                     const std::optional<int> timezone_offset) {
  int day_of_week = exploded.day_of_week == 0 ? 7 : exploded.day_of_week;
  int milliseconds = static_cast<int>(
      base::Hours(exploded.hour).InMilliseconds() +
      base::Minutes(exploded.minute).InMilliseconds() +
      base::Seconds(exploded.second).InMilliseconds() + exploded.millisecond);
  return WeeklyTime(day_of_week, milliseconds, timezone_offset);
}

}  // namespace policy

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/policy/weekly_time/weekly_time.h"

#include "base/logging.h"
#include "base/time/time.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr base::TimeDelta kWeek = base::TimeDelta::FromDays(7);
constexpr base::TimeDelta kDay = base::TimeDelta::FromDays(1);
constexpr base::TimeDelta kHour = base::TimeDelta::FromHours(1);
constexpr base::TimeDelta kMinute = base::TimeDelta::FromMinutes(1);
constexpr base::TimeDelta kSecond = base::TimeDelta::FromSeconds(1);

WeeklyTime GetWeeklyTimeFromExploded(
    const base::Time::Exploded& exploded,
    const base::Optional<int> timezone_offset) {
  int day_of_week = exploded.day_of_week == 0 ? 7 : exploded.day_of_week;
  int milliseconds = exploded.hour * kHour.InMilliseconds() +
                     exploded.minute * kMinute.InMilliseconds() +
                     exploded.second * kSecond.InMilliseconds() +
                     exploded.millisecond;
  return WeeklyTime(day_of_week, milliseconds, timezone_offset);
}

}  // namespace

// static
const char WeeklyTime::kDayOfWeek[] = "day_of_week";
const char WeeklyTime::kTime[] = "time";
const char WeeklyTime::kTimezoneOffset[] = "timezon_offset";

WeeklyTime::WeeklyTime(int day_of_week,
                       int milliseconds,
                       base::Optional<int> timezone_offset)
    : day_of_week_(day_of_week),
      milliseconds_(milliseconds),
      timezone_offset_(timezone_offset) {
  DCHECK_GT(day_of_week, 0);
  DCHECK_LE(day_of_week, 7);
  DCHECK_GE(milliseconds, 0);
  DCHECK_LT(milliseconds, kDay.InMilliseconds());
}

WeeklyTime::WeeklyTime(const WeeklyTime& rhs) = default;

WeeklyTime& WeeklyTime::operator=(const WeeklyTime& rhs) = default;

std::unique_ptr<base::DictionaryValue> WeeklyTime::ToValue() const {
  auto weekly_time = std::make_unique<base::DictionaryValue>();
  weekly_time->SetInteger(kDayOfWeek, day_of_week_);
  weekly_time->SetInteger(kTime, milliseconds_);
  if (timezone_offset_)
    weekly_time->SetInteger(kTimezoneOffset, timezone_offset_.value());
  return weekly_time;
}

base::TimeDelta WeeklyTime::GetDurationTo(const WeeklyTime& other) const {
  // Can't compare timezone agnostic intervals and non-timezone agnostic
  // intervals.
  DCHECK_EQ(timezone_offset_.has_value(), other.timezone_offset().has_value());
  WeeklyTime other_converted =
      timezone_offset_ ? other.ConvertToTimezone(timezone_offset_.value())
                       : other;
  int duration =
      (other_converted.day_of_week() - day_of_week_) * kDay.InMilliseconds() +
      other_converted.milliseconds() - milliseconds_;
  if (duration < 0)
    duration += kWeek.InMilliseconds();
  return base::TimeDelta::FromMilliseconds(duration);
}

WeeklyTime WeeklyTime::AddMilliseconds(int milliseconds) const {
  milliseconds %= kWeek.InMilliseconds();
  // Make |milliseconds| positive number (add number of milliseconds per week)
  // for easier evaluation.
  milliseconds += kWeek.InMilliseconds();
  int shifted_milliseconds = milliseconds_ + milliseconds;
  // Get milliseconds from the start of the day.
  int result_milliseconds = shifted_milliseconds % kDay.InMilliseconds();
  int day_offset = shifted_milliseconds / kDay.InMilliseconds();
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

WeeklyTime WeeklyTime::ConvertToCustomTimezone(int timezone_offset) const {
  DCHECK(!timezone_offset_);
  return WeeklyTime(day_of_week_, milliseconds_, timezone_offset);
}

// static
WeeklyTime WeeklyTime::GetCurrentGmtWeeklyTime(base::Clock* clock) {
  base::Time::Exploded exploded;
  clock->Now().UTCExplode(&exploded);
  return GetWeeklyTimeFromExploded(exploded, 0);
}

// static
WeeklyTime WeeklyTime::GetCurrentLocalWeeklyTime(base::Clock* clock) {
  base::Time::Exploded exploded;
  clock->Now().LocalExplode(&exploded);
  WeeklyTime result = GetWeeklyTimeFromExploded(exploded, base::nullopt);
  return result;
}

// static
std::unique_ptr<WeeklyTime> WeeklyTime::ExtractFromProto(
    const em::WeeklyTimeProto& container,
    base::Optional<int> timezone_offset) {
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
  if (!(time_of_day >= 0 && time_of_day < kDay.InMilliseconds())) {
    LOG(ERROR) << "Invalid time value: " << time_of_day
               << ", the value should be in [0; " << kDay.InMilliseconds()
               << ").";
    return nullptr;
  }
  return std::make_unique<WeeklyTime>(container.day_of_week(), time_of_day,
                                      timezone_offset);
}

// static
std::unique_ptr<WeeklyTime> WeeklyTime::ExtractFromValue(
    const base::Value* value,
    base::Optional<int> timezone_offset) {
  if (!value) {
    LOG(ERROR) << "Passed nullptr value.";
    return nullptr;
  }
  auto day_of_week = value->FindIntKey(kDayOfWeek);
  if (!day_of_week.has_value() || day_of_week.value() < 1 ||
      day_of_week.value() > 7) {
    LOG(ERROR) << "Day of week is absent or invalid";
    return nullptr;
  }
  auto time_of_day = value->FindIntKey(kTime);
  if (!time_of_day.has_value()) {
    LOG(ERROR) << "Time is absent";
    return nullptr;
  }

  if (!(time_of_day.value() >= 0 &&
        time_of_day.value() < kDay.InMilliseconds())) {
    LOG(ERROR) << "Invalid time value: " << time_of_day.value()
               << ", the value should be in [0; " << kDay.InMilliseconds()
               << ").";
    return nullptr;
  }
  return std::make_unique<WeeklyTime>(day_of_week.value(), time_of_day.value(),
                                      timezone_offset);
}

}  // namespace policy

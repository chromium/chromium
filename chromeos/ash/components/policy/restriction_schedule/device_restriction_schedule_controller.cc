// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/checked_util.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_checked.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval_checked.h"
#include "chromeos/constants/pref_names.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace policy {

namespace {

// Display a notification about approaching session end this long in advance.
constexpr base::TimeDelta kNotificationLeadTime = base::Minutes(30);

using ::policy::weekly_time::ExtractIntervalsFromList;
using ::policy::weekly_time::GetDurationToNextEvent;
using ::policy::weekly_time::GetNextEvent;
using ::policy::weekly_time::IntervalsContainTime;
using Day = ::policy::WeeklyTimeChecked::Day;

int GetDayOfWeekStringId(Day day_of_week) {
  switch (day_of_week) {
    case Day::kMonday:
      return IDS_DEVICE_DISABLED_EXPLANATION_RESTRICTION_SCHEDULE_MONDAY;
    case Day::kTuesday:
      return IDS_DEVICE_DISABLED_EXPLANATION_RESTRICTION_SCHEDULE_TUESDAY;
    case Day::kWednesday:
      return IDS_DEVICE_DISABLED_EXPLANATION_RESTRICTION_SCHEDULE_WEDNESDAY;
    case Day::kThursday:
      return IDS_DEVICE_DISABLED_EXPLANATION_RESTRICTION_SCHEDULE_THURSDAY;
    case Day::kFriday:
      return IDS_DEVICE_DISABLED_EXPLANATION_RESTRICTION_SCHEDULE_FRIDAY;
    case Day::kSaturday:
      return IDS_DEVICE_DISABLED_EXPLANATION_RESTRICTION_SCHEDULE_SATURDAY;
    case Day::kSunday:
      return IDS_DEVICE_DISABLED_EXPLANATION_RESTRICTION_SCHEDULE_SUNDAY;
  }
}

}  // namespace

DeviceRestrictionScheduleController::DeviceRestrictionScheduleController(
    Delegate& delegate,
    PrefService& local_state)
    : delegate_(delegate) {
  registrar_.Init(&local_state);
  // `base::Unretained` is safe here because `this` outlives `registrar_` which
  // unregisters the observer when it is destroyed.
  registrar_.Add(
      chromeos::prefs::kDeviceRestrictionSchedule,
      base::BindRepeating(&DeviceRestrictionScheduleController::OnPolicyUpdated,
                          base::Unretained(this)));

  MaybeShowPostLogoutNotification();
  OnPolicyUpdated();
}

DeviceRestrictionScheduleController::~DeviceRestrictionScheduleController() =
    default;

// static
void DeviceRestrictionScheduleController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(chromeos::prefs::kDeviceRestrictionSchedule);
  registry->RegisterBooleanPref(
      chromeos::prefs::kDeviceRestrictionScheduleShowPostLogoutNotification,
      false);
}

bool DeviceRestrictionScheduleController::RestrictionScheduleEnabled() const {
  return state_ == State::kRestricted;
}

// Returns "Today", "Tomorrow", or the specific day of week with a preposition
// for later days (eg. "on Wednesday").
std::u16string DeviceRestrictionScheduleController::RestrictionScheduleEndDay()
    const {
  const WeeklyTimeChecked current_weekly_time =
      WeeklyTimeChecked::FromTimeAsLocalTime(base::Time::Now());
  std::optional<WeeklyTimeChecked> next_event =
      GetNextEvent(intervals_, current_weekly_time);
  if (state_ == State::kRegular || !next_event.has_value()) {
    return std::u16string();
  }

  const Day week_day_today = current_weekly_time.day_of_week();
  const Day week_day_next_event = next_event.value().day_of_week();

  if (week_day_today == week_day_next_event) {
    return l10n_util::GetStringUTF16(IDS_PAST_TIME_TODAY);
  }

  if (WeeklyTimeChecked::NextDay(week_day_today) == week_day_next_event) {
    return l10n_util::GetStringUTF16(IDS_TIME_TOMORROW);
  }

  return l10n_util::GetStringUTF16(GetDayOfWeekStringId(week_day_next_event));
}

std::u16string DeviceRestrictionScheduleController::RestrictionScheduleEndTime()
    const {
  if (state_ == State::kRegular || !next_run_time_.has_value()) {
    return std::u16string();
  }
  return base::TimeFormatTimeOfDay(next_run_time_.value());
}

void DeviceRestrictionScheduleController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceRestrictionScheduleController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceRestrictionScheduleController::OnPolicyUpdated() {
  const base::Value::List& policy_value =
      registrar_.prefs()->GetList(chromeos::prefs::kDeviceRestrictionSchedule);

  if (!UpdateIntervalsIfChanged(policy_value)) {
    return;
  }

  Run();
}

void DeviceRestrictionScheduleController::Run() {
  // Reset any potentially running timers.
  run_timer_.Stop();
  notification_timer_.Stop();

  // Update state.
  const base::Time current_time = base::Time::Now();
  next_run_time_ = GetNextRunTime(current_time);
  state_ = GetCurrentState(current_time);

  // Set up timers if there's a schedule (`intervals_` isn't empty).
  if (next_run_time_.has_value()) {
    // Show end session notification in regular state.
    if (state_ == State::kRegular) {
      StartNotificationTimer(current_time, next_run_time_.value());
    }

    // Set up next run of the function.
    StartRunTimer(next_run_time_.value());
  }

  // Schedule a post-logout notification if necessary.
  if (state_ == State::kRestricted && delegate_->IsUserLoggedIn()) {
    registrar_.prefs()->SetBoolean(
        chromeos::prefs::kDeviceRestrictionScheduleShowPostLogoutNotification,
        true);
  }

  // Block or unblock login. This needs to be the last statement since it could
  // cause a restart to the login-screen.
  for (auto& observer : observers_) {
    observer.OnRestrictionScheduleStateChanged(state_ == State::kRestricted);
  }
}

void DeviceRestrictionScheduleController::MaybeShowUpcomingLogoutNotification(
    base::Time logout_time) {
  if (delegate_->IsUserLoggedIn()) {
    delegate_->ShowUpcomingLogoutNotification(logout_time);
  }
}

void DeviceRestrictionScheduleController::MaybeShowPostLogoutNotification() {
  if (registrar_.prefs()->GetBoolean(
          chromeos::prefs::
              kDeviceRestrictionScheduleShowPostLogoutNotification)) {
    registrar_.prefs()->SetBoolean(
        chromeos::prefs::kDeviceRestrictionScheduleShowPostLogoutNotification,
        false);
    delegate_->ShowPostLogoutNotification();
  }
}

// TODO(isandrk): Pass in `intervals_` and convert to pure function in the empty
// namespace.
std::optional<base::Time> DeviceRestrictionScheduleController::GetNextRunTime(
    base::Time current_time) const {
  const WeeklyTimeChecked current_weekly_time_checked =
      WeeklyTimeChecked::FromTimeAsLocalTime(current_time);
  std::optional<base::TimeDelta> time_to_next_run =
      GetDurationToNextEvent(intervals_, current_weekly_time_checked);

  if (!time_to_next_run.has_value()) {
    // `intervals_` is empty.
    return std::nullopt;
  }

  return current_time + time_to_next_run.value();
}

// TODO(isandrk): Pass in `intervals_` and convert to pure function in the empty
// namespace.
DeviceRestrictionScheduleController::State
DeviceRestrictionScheduleController::GetCurrentState(
    base::Time current_time) const {
  auto current_weekly_time_checked =
      WeeklyTimeChecked::FromTimeAsLocalTime(current_time);
  return IntervalsContainTime(intervals_, current_weekly_time_checked)
             ? State::kRestricted
             : State::kRegular;
}

bool DeviceRestrictionScheduleController::UpdateIntervalsIfChanged(
    const base::Value::List& policy_value) {
  std::vector<WeeklyTimeIntervalChecked> new_intervals;
  auto intervals_opt = ExtractIntervalsFromList(policy_value);
  if (intervals_opt.has_value()) {
    new_intervals = std::move(intervals_opt.value());
  } else {
    // Intervals parsing error. Use empty intervals.
    new_intervals = {};
  }

  if (new_intervals == intervals_) {
    return false;
  }
  intervals_ = std::move(new_intervals);
  return true;
}

void DeviceRestrictionScheduleController::StartNotificationTimer(
    base::Time current_time,
    base::Time logout_time) {
  // Clamp past times to current time.
  const base::Time notification_time =
      std::max(logout_time - kNotificationLeadTime, current_time);

  // `this` outlives `notification_timer_`.
  notification_timer_.Start(
      FROM_HERE, notification_time,
      base::BindOnce(&DeviceRestrictionScheduleController::
                         MaybeShowUpcomingLogoutNotification,
                     base::Unretained(this), logout_time));
}

void DeviceRestrictionScheduleController::StartRunTimer(
    base::Time next_run_time) {
  // `this` outlives `run_timer_`.
  run_timer_.Start(FROM_HERE, next_run_time,
                   base::BindOnce(&DeviceRestrictionScheduleController::Run,
                                  base::Unretained(this)));
}

}  // namespace policy

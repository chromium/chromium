// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "base/values.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller_delegate_impl.h"
#include "chromeos/ash/components/policy/weekly_time/checked_util.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_checked.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval_checked.h"
#include "chromeos/constants/pref_names.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

namespace {

// Display a notification about approaching session end this long in advance.
constexpr base::TimeDelta kNotificationLeadTime = base::Minutes(30);

using ::policy::weekly_time::AddOffsetInLocalTime;
using ::policy::weekly_time::ExtractIntervalsFromList;
using ::policy::weekly_time::GetDurationToNextEvent;
using ::policy::weekly_time::GetNextEvent;
using ::policy::weekly_time::IntervalsContainTime;
using Day = ::policy::WeeklyTimeChecked::Day;

enum class State { kRegular, kRestricted };

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

class DeviceRestrictionScheduleControllerImpl
    : public DeviceRestrictionScheduleController,
      public ash::LoginState::Observer {
 public:
  DeviceRestrictionScheduleControllerImpl(std::unique_ptr<Delegate> delegate,
                                          PrefService& local_state);

  DeviceRestrictionScheduleControllerImpl(
      const DeviceRestrictionScheduleControllerImpl&) = delete;
  DeviceRestrictionScheduleControllerImpl& operator=(
      const DeviceRestrictionScheduleControllerImpl&) = delete;

  bool RestrictionScheduleEnabled() const override;
  std::u16string RestrictionScheduleEndDay() const override;
  std::u16string RestrictionScheduleEndTime() const override;

  void SetClockForTesting(const base::Clock& clock) override;
  void SetMessageUpdateTimerForTesting(
      std::unique_ptr<base::WallClockTimer> timer) override;

  using DeviceRestrictionScheduleController::Observer;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  // ash::LoginState::Observer:
  void LoggedInStateChanged() override;

  void OnPolicyUpdated();
  void Run();

  void MaybeShowUpcomingLogoutNotification(base::Time logout_time);
  void MaybeShowPostLogoutNotification();
  void RestrictionScheduleMessageChanged();

  std::optional<base::Time> GetNextRunTime(base::Time current_time) const;
  State GetCurrentState(base::Time current_time) const;
  bool UpdateIntervalsIfChanged(const base::Value::List& policy_value);

  void StartNotificationTimer(base::Time current_time, base::Time logout_time);
  void StartRunTimer(base::Time next_run_time);
  void StartMessageUpdateTimer(base::Time current_time);

  std::unique_ptr<Delegate> delegate_;
  PrefChangeRegistrar registrar_;
  base::ObserverList<Observer> observers_;
  raw_ref<const base::Clock> clock_{*base::DefaultClock::GetInstance()};
  base::ScopedObservation<ash::LoginState, ash::LoginState::Observer>
      login_state_observation_{this};

  std::vector<WeeklyTimeIntervalChecked> intervals_;
  State state_ = State::kRegular;
  std::optional<base::Time> next_run_time_;

  base::WallClockTimer run_timer_;
  base::WallClockTimer notification_timer_;
  std::unique_ptr<base::WallClockTimer> message_update_timer_ =
      std::make_unique<base::WallClockTimer>();
};

DeviceRestrictionScheduleControllerImpl::
    DeviceRestrictionScheduleControllerImpl(std::unique_ptr<Delegate> delegate,
                                            PrefService& local_state)
    : delegate_(std::move(delegate)) {
  registrar_.Init(&local_state);
  // `base::Unretained` is safe here because `this` outlives `registrar_` which
  // unregisters the observer when it is destroyed.
  registrar_.Add(chromeos::prefs::kDeviceRestrictionSchedule,
                 base::BindRepeating(
                     &DeviceRestrictionScheduleControllerImpl::OnPolicyUpdated,
                     base::Unretained(this)));

  login_state_observation_.Observe(ash::LoginState::Get());

  MaybeShowPostLogoutNotification();
  OnPolicyUpdated();
}

bool DeviceRestrictionScheduleControllerImpl::RestrictionScheduleEnabled()
    const {
  return state_ == State::kRestricted;
}

// Returns "Today", "Tomorrow", or the specific day of week with a preposition
// for later days (eg. "on Wednesday").
std::u16string
DeviceRestrictionScheduleControllerImpl::RestrictionScheduleEndDay() const {
  const WeeklyTimeChecked current_weekly_time =
      WeeklyTimeChecked::FromTimeAsLocalTime(clock_->Now());
  std::optional<WeeklyTimeChecked> next_event =
      GetNextEvent(intervals_, current_weekly_time);
  if (state_ == State::kRegular || !next_event.has_value()) {
    return std::u16string();
  }

  const Day week_day_today = current_weekly_time.day_of_week();
  const Day week_day_next_event = next_event.value().day_of_week();

  if (week_day_today == week_day_next_event) {
    return l10n_util::GetStringUTF16(
        IDS_DEVICE_DISABLED_EXPLANATION_RESTRICTION_SCHEDULE_TODAY);
  }

  if (WeeklyTimeChecked::NextDay(week_day_today) == week_day_next_event) {
    return l10n_util::GetStringUTF16(
        IDS_DEVICE_DISABLED_EXPLANATION_RESTRICTION_SCHEDULE_TOMORROW);
  }

  return l10n_util::GetStringUTF16(GetDayOfWeekStringId(week_day_next_event));
}

std::u16string
DeviceRestrictionScheduleControllerImpl::RestrictionScheduleEndTime() const {
  if (state_ == State::kRegular || !next_run_time_.has_value()) {
    return std::u16string();
  }
  return base::TimeFormatTimeOfDay(next_run_time_.value());
}

void DeviceRestrictionScheduleControllerImpl::SetClockForTesting(
    const base::Clock& clock) {
  clock_ = clock;
}

void DeviceRestrictionScheduleControllerImpl::SetMessageUpdateTimerForTesting(
    std::unique_ptr<base::WallClockTimer> timer) {
  message_update_timer_ = std::move(timer);
}

void DeviceRestrictionScheduleControllerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceRestrictionScheduleControllerImpl::RemoveObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceRestrictionScheduleControllerImpl::LoggedInStateChanged() {
  if (intervals_.empty()) {
    // Do nothing if the policy isn't set.
    return;
  }
  // Re-run the logic when a login event happens.
  Run();
}

void DeviceRestrictionScheduleControllerImpl::OnPolicyUpdated() {
  const base::Value::List& policy_value =
      registrar_.prefs()->GetList(chromeos::prefs::kDeviceRestrictionSchedule);

  if (!UpdateIntervalsIfChanged(policy_value)) {
    return;
  }

  Run();
}

void DeviceRestrictionScheduleControllerImpl::Run() {
  // Reset any potentially running timers.
  run_timer_.Stop();
  notification_timer_.Stop();
  message_update_timer_->Stop();

  // Update state.
  const base::Time current_time = clock_->Now();
  next_run_time_ = GetNextRunTime(current_time);
  state_ = GetCurrentState(current_time);

  // Set up timers if there's a schedule (`intervals_` isn't empty).
  if (next_run_time_.has_value()) {
    // Show end session notification in regular state.
    if (state_ == State::kRegular) {
      StartNotificationTimer(current_time, next_run_time_.value());
    }

    // Update restriction schedule banner message on day change.
    if (state_ == State::kRestricted) {
      StartMessageUpdateTimer(current_time);
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
  observers_.Notify(&Observer::OnRestrictionScheduleStateChanged,
                    state_ == State::kRestricted);
}

void DeviceRestrictionScheduleControllerImpl::
    MaybeShowUpcomingLogoutNotification(base::Time logout_time) {
  if (delegate_->IsUserLoggedIn()) {
    delegate_->ShowUpcomingLogoutNotification(logout_time);
  }
}

void DeviceRestrictionScheduleControllerImpl::
    MaybeShowPostLogoutNotification() {
  if (registrar_.prefs()->GetBoolean(
          chromeos::prefs::
              kDeviceRestrictionScheduleShowPostLogoutNotification)) {
    registrar_.prefs()->SetBoolean(
        chromeos::prefs::kDeviceRestrictionScheduleShowPostLogoutNotification,
        false);
    delegate_->ShowPostLogoutNotification();
  }
}

void DeviceRestrictionScheduleControllerImpl::
    RestrictionScheduleMessageChanged() {
  observers_.Notify(&Observer::OnRestrictionScheduleMessageChanged);
  StartMessageUpdateTimer(clock_->Now());
}

// TODO(isandrk): Pass in `intervals_` and convert to pure function in the empty
// namespace.
std::optional<base::Time>
DeviceRestrictionScheduleControllerImpl::GetNextRunTime(
    base::Time current_time) const {
  const WeeklyTimeChecked current_weekly_time_checked =
      WeeklyTimeChecked::FromTimeAsLocalTime(current_time);
  std::optional<base::TimeDelta> time_to_next_run =
      GetDurationToNextEvent(intervals_, current_weekly_time_checked);

  if (!time_to_next_run.has_value()) {
    // `intervals_` is empty.
    return std::nullopt;
  }

  return AddOffsetInLocalTime(current_time, time_to_next_run.value());
}

// TODO(isandrk): Pass in `intervals_` and convert to pure function in the empty
// namespace.
State DeviceRestrictionScheduleControllerImpl::GetCurrentState(
    base::Time current_time) const {
  auto current_weekly_time_checked =
      WeeklyTimeChecked::FromTimeAsLocalTime(current_time);
  return IntervalsContainTime(intervals_, current_weekly_time_checked)
             ? State::kRestricted
             : State::kRegular;
}

bool DeviceRestrictionScheduleControllerImpl::UpdateIntervalsIfChanged(
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

void DeviceRestrictionScheduleControllerImpl::StartNotificationTimer(
    base::Time current_time,
    base::Time logout_time) {
  // Clamp past times to current time.
  const base::Time notification_time =
      std::max(logout_time - kNotificationLeadTime, current_time);

  // `this` outlives `notification_timer_`.
  notification_timer_.Start(
      FROM_HERE, notification_time,
      base::BindOnce(&DeviceRestrictionScheduleControllerImpl::
                         MaybeShowUpcomingLogoutNotification,
                     base::Unretained(this), logout_time));
}

void DeviceRestrictionScheduleControllerImpl::StartRunTimer(
    base::Time next_run_time) {
  // `this` outlives `run_timer_`.
  run_timer_.Start(FROM_HERE, next_run_time,
                   base::BindOnce(&DeviceRestrictionScheduleControllerImpl::Run,
                                  base::Unretained(this)));
}

void DeviceRestrictionScheduleControllerImpl::StartMessageUpdateTimer(
    base::Time current_time) {
  if (!next_run_time_.has_value()) {
    // Sanity check, should never happen.
    return;
  }

  // There are at most two message updates:
  // 1) At next_run_midnight - base::Days(1): "Tomorrow".
  // 2) At next_run_midnight: "Today";
  // We only need to check our position in time relative to these two.

  const base::Time next_run_midnight = next_run_time_.value().LocalMidnight();
  // We need to include the consideration of daylight saving time in local time,
  // so cannot subtract by one day simply here. 12 hours is enough long/short to
  // point a time in the "previous local day" even with the consideration of
  // daylight saving time.
  const base::Time next_run_prev_day_midnight =
      (next_run_midnight - base::Hours(12)).LocalMidnight();
  base::Time update_time;

  if (current_time < next_run_prev_day_midnight) {
    // The message is some day of week (eg. "Wednesday"), needs to be update to
    // "Tomorrow".
    update_time = next_run_prev_day_midnight;
  } else if (current_time < next_run_midnight) {
    // The message is "Tomorrow", needs to be updated to "Today".
    update_time = next_run_midnight;
  } else {
    // The message is already "Today" so no update needed.
    return;
  }

  // `this` outlives `message_update_timer_`.
  message_update_timer_->Start(
      FROM_HERE, update_time,
      base::BindOnce(&DeviceRestrictionScheduleControllerImpl::
                         RestrictionScheduleMessageChanged,
                     base::Unretained(this)));
}

}  // namespace

// static
std::unique_ptr<DeviceRestrictionScheduleController>
DeviceRestrictionScheduleController::Create(PrefService& local_state) {
  return CreateWithDelegate(
      std::make_unique<
          policy::DeviceRestrictionScheduleControllerDelegateImpl>(),
      local_state);
}

// static
std::unique_ptr<DeviceRestrictionScheduleController>
DeviceRestrictionScheduleController::CreateWithDelegate(
    std::unique_ptr<Delegate> delegate,
    PrefService& local_state) {
  return std::make_unique<DeviceRestrictionScheduleControllerImpl>(
      std::move(delegate), local_state);
}

// static
void DeviceRestrictionScheduleController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(chromeos::prefs::kDeviceRestrictionSchedule);
  registry->RegisterBooleanPref(
      chromeos::prefs::kDeviceRestrictionScheduleShowPostLogoutNotification,
      false);
}

}  // namespace policy

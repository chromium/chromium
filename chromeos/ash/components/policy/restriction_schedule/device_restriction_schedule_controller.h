// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "base/values.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace policy {

class WeeklyTimeIntervalChecked;

// This class observes the pref `kDeviceRestrictionSchedule`, and handles
// restricting the device access when the schedule is active.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
    DeviceRestrictionScheduleController : public ash::LoginState::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Checks if a user is logged in.
    virtual bool IsUserLoggedIn() const = 0;

    // Shows an in-session notification about upcoming forced logout.
    virtual void ShowUpcomingLogoutNotification(base::Time logout_time) = 0;

    // Shows a login-screen notification after the forced logout.
    virtual void ShowPostLogoutNotification() = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when the restriction schedule state changes. `enabled` is set to
    // true if restriction schedule is enabled, and false otherwise.
    virtual void OnRestrictionScheduleStateChanged(bool enabled) = 0;

    // Called when the restriction schedule message changes.
    virtual void OnRestrictionScheduleMessageChanged() = 0;
  };

  DeviceRestrictionScheduleController(Delegate& delegate,
                                      PrefService& local_state);
  ~DeviceRestrictionScheduleController() override;

  DeviceRestrictionScheduleController(
      const DeviceRestrictionScheduleController&) = delete;
  DeviceRestrictionScheduleController& operator=(
      const DeviceRestrictionScheduleController&) = delete;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  bool RestrictionScheduleEnabled() const;
  std::u16string RestrictionScheduleEndDay() const;
  std::u16string RestrictionScheduleEndTime() const;

  void SetClockForTesting(const base::Clock& clock);
  void SetMessageUpdateTimerForTesting(
      std::unique_ptr<base::WallClockTimer> timer);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  enum class State { kRegular, kRestricted };

  // ash::LoginState::Observer:
  void LoggedInStateChanged() override;

  void LoadHighestSeenTime();
  void UpdateAndSaveHighestSeenTime(base::Time current_time);
  bool HasTimeBeenTamperedWith(const base::Time current_time) const;

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

  // `delegate_` has to outlive `DeviceRestrictionScheduleController`.
  const raw_ref<Delegate> delegate_;
  PrefChangeRegistrar registrar_;
  base::ObserverList<Observer> observers_;
  raw_ref<const base::Clock> clock_{*base::DefaultClock::GetInstance()};
  base::ScopedObservation<ash::LoginState, ash::LoginState::Observer>
      login_state_observation_{this};

  std::vector<WeeklyTimeIntervalChecked> intervals_;
  State state_ = State::kRegular;
  std::optional<base::Time> next_run_time_;

  // Represents the highest observed system time by this class. Updated on every
  // run of the `Run` function which is when the policy is updated and then
  // every time the state changes between regular and restricted schedule.
  // Used to only guard against changing the time by removing the CMOS battery;
  // that resets the system time to the UNIX epoch, but early ChromeOS startup
  // code detects this and updates the time to Jan 1 of the current year (baked
  // in during compile time).
  std::optional<base::Time> highest_seen_time_;

  base::WallClockTimer run_timer_;
  base::WallClockTimer notification_timer_;
  std::unique_ptr<base::WallClockTimer> message_update_timer_ =
      std::make_unique<base::WallClockTimer>();
};

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_H_

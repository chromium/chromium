// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace policy {

class WeeklyTimeIntervalChecked;

// This class observes the pref `kDeviceRestrictionSchedule`, and handles
// restricting the device access when the schedule is active.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
    DeviceRestrictionScheduleController {
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
  };

  DeviceRestrictionScheduleController(Delegate& delegate,
                                      PrefService& local_state);
  ~DeviceRestrictionScheduleController();

  DeviceRestrictionScheduleController(
      const DeviceRestrictionScheduleController&) = delete;
  DeviceRestrictionScheduleController& operator=(
      const DeviceRestrictionScheduleController&) = delete;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  bool RestrictionScheduleEnabled() const;
  std::u16string RestrictionScheduleEndDay() const;
  std::u16string RestrictionScheduleEndTime() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  enum class State { kRegular, kRestricted };

  void OnPolicyUpdated();
  void Run();
  void MaybeShowUpcomingLogoutNotification(base::Time logout_time);
  void MaybeShowPostLogoutNotification();
  std::optional<base::Time> GetNextRunTime(base::Time current_time) const;
  State GetCurrentState(base::Time current_time) const;
  bool UpdateIntervalsIfChanged(const base::Value::List& policy_value);
  void StartNotificationTimer(base::Time current_time, base::Time logout_time);
  void StartRunTimer(base::Time next_run_time);

  // `delegate_` has to outlive `DeviceRestrictionScheduleController`.
  const raw_ref<Delegate> delegate_;
  PrefChangeRegistrar registrar_;
  base::ObserverList<Observer> observers_;

  std::vector<WeeklyTimeIntervalChecked> intervals_;
  State state_ = State::kRegular;
  std::optional<base::Time> next_run_time_;

  base::WallClockTimer run_timer_;
  base::WallClockTimer notification_timer_;
};

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_H_

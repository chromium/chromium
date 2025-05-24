// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
class WallClockTimer;
}  // namespace base

namespace policy {

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

    // Called when the restriction schedule message changes.
    virtual void OnRestrictionScheduleMessageChanged() = 0;
  };

  static std::unique_ptr<DeviceRestrictionScheduleController> Create(
      PrefService& local_state);
  static std::unique_ptr<DeviceRestrictionScheduleController>
  CreateWithDelegate(std::unique_ptr<Delegate> delegate,
                     PrefService& local_state);

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  virtual ~DeviceRestrictionScheduleController() = default;

  virtual bool RestrictionScheduleEnabled() const = 0;
  virtual std::u16string RestrictionScheduleEndDay() const = 0;
  virtual std::u16string RestrictionScheduleEndTime() const = 0;

  virtual void SetClockForTesting(const base::Clock& clock) = 0;
  virtual void SetMessageUpdateTimerForTesting(
      std::unique_ptr<base::WallClockTimer> timer) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_H_

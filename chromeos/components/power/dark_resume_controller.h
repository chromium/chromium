// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_POWER_DARK_RESUME_CONTROLLER_H_
#define CHROMEOS_COMPONENTS_POWER_DARK_RESUME_CONTROLLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace system {

// This class listens to dark resume events from the power manager and makes
// decisions on whether to re-suspend the device or keep the device in dark
// resume based on the wake locks activated at the time of a dark resume. It -
//
// 1. Starts a timer to check for any activated app suspension wake locks. This
// gives services time to do work in dark resume. They can activate a wake lock
// to indicate to the system that they are still doing work.
//
// 2. After the timer in 1 expires, an observer for wake lock deactivation is
// set. Also a hard timeout timer is scheduled.
//
// 3. If no wake lock is held then the observer gets notified immediately and
// the power manager is requested to re-suspend the system.
//
// 4. If an app suspension wake lock is acquired then either -
// - The observer from 2 is notified when the wake lock is deactivated and the
// power manager is requested to re-suspend the system.
// Or
// - The hard timeout timer from 2 fires and the power manager is requested to
// re-suspend the system.
//
// 5. If the system transitions to a full resume all dark resume related state
// and timers are cleared as the system wakes up.
class COMPONENT_EXPORT(CHROMEOS_POWER) DarkResumeController
    : public chromeos::PowerManagerClient::Observer,
      public device::mojom::WakeLockObserver {
 public:
  explicit DarkResumeController(service_manager::Connector* connector);
  ~DarkResumeController() override;

  // Time after a dark resume when wake lock count is checked and a decision is
  // made to re-suspend or wait for wake lock release.
  static constexpr base::TimeDelta kDarkResumeWakeLockCheckTimeout =
      base::TimeDelta::FromSeconds(3);

  // chromeos::PowerManagerClient::Observer overrides.
  void PowerManagerInitialized() override;
  void DarkSuspendImminent() override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // mojom::WakeLockObserver overrides.
  void OnWakeLockDeactivated(device::mojom::WakeLockType type) override;

  // Return true iff all dark resume related state is set i.e the suspend
  // readiness token is set and wake lock release event has observers.
  bool IsDarkResumeStateSetForTesting() const;

  // Return true iff all dark resume related state is reset i.e. suspend
  // readiness token is empty, wake lock release event has no observers,
  // wake lock check timer is reset, hard timeout timer is reset and there are
  // no in flight tasks. This should be true when device exits dark resume
  // either by re-suspending or transitioning to full resume.
  bool IsDarkResumeStateClearedForTesting() const;

  // Returns |dark_resume_hard_timeout_|.
  base::TimeDelta GetHardTimeoutForTesting() const;

 private:
  // Called |kDarkResumeWakeLockCheckTimeout| after a dark resume. Checks if
  // app suspension wake locks are held. If no wake locks are held then
  // re-suspends the device else schedules HandleDarkResumeHardTimeout.
  void HandleDarkResumeWakeLockCheckTimeout();

  // Called |kDarkResumeHardTimeout| after a
  // HandleDarkResumeWakeLockCheckTimeout. Clears all dark resume state and
  // re-suspends the device.
  void HandleDarkResumeHardTimeout();

  // Clears all state associated with dark resume.
  void ClearDarkResumeState();

  // Used for acquiring, releasing and observing wake locks.
  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider_;

  // Not owned by this instance.
  service_manager::Connector* const connector_;

  // Used when system is ready to suspend after a DarkSupendImminent i.e.
  // after a dark resume.
  base::UnguessableToken block_suspend_token_;

  // The receiver used to implement device::mojom::WakeLockObserver.
  mojo::Receiver<device::mojom::WakeLockObserver> wake_lock_observer_receiver_{
      this};

  // Timer used to schedule HandleDarkResumeWakeLockCheckTimeout.
  base::OneShotTimer wake_lock_check_timer_;

  // Timer used to schedule HandleDarkResumeHardTimeout.
  base::OneShotTimer hard_timeout_timer_;

  // Max time to wait for wake lock release after a wake lock check after a dark
  // resume. After this time the system is asked to re-suspend. This is
  // initialized via PowerManagerClient when it's initialization is complete in
  // |PowerManagerInitialized|. Till then there may be a very small window after
  // booth when it takes a default value.
  base::TimeDelta dark_resume_hard_timeout_;

  // Used for checking if HandleDarkResumeWakeLockCheckTimeout and
  // HandleDarkResumeHardTimeout run on the same sequence.
  SEQUENCE_CHECKER(dark_resume_tasks_sequence_checker_);

  // This is invalidated in ClearDarkResumeState as a fail safe measure to clear
  // any lingering timer callbacks or wake lock observer callbacks. This is a
  // good to have but not a necessity as ClearDarkResumeState cancels any dark
  // resume state machine related tasks via other means. In the future if other
  // tasks or callbacks need to be added separate from the dark resume state
  // machine lifetime then a separate factory needs to be created and used.
  base::WeakPtrFactory<DarkResumeController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DarkResumeController);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_POWER_DARK_RESUME_CONTROLLER_H_

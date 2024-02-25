// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SYSTEM_CLOCK_SYSTEM_CLOCK_SYNC_OBSERVATION_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SYSTEM_CLOCK_SYSTEM_CLOCK_SYNC_OBSERVATION_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"

namespace ash {

// Supports waiting for the system clock to become synchronized.
class COMPONENT_EXPORT(SYSTEM_CLOCK) SystemClockSyncObservation
    : public SystemClockClient::Observer {
 public:
  // A callback that will be invoked with `result`==true when the system clock
  // has been synchronized, or with `result`==false if waiting for system clock
  // synchronization has failed.
  // The callback can be called with `result`==false if the system clock service
  // is not available or if waiting for system clock synchronization timed out.
  using SystemClockSyncCallback = base::OnceCallback<void(bool result)>;

  // Invokes |callback| when the system clock has been synchronized or |timeout|
  // elapses.
  static std::unique_ptr<SystemClockSyncObservation> WaitForSystemClockSync(
      SystemClockClient* system_clock_client,
      base::TimeDelta timeout,
      SystemClockSyncCallback callback);

  ~SystemClockSyncObservation() override;

  // Used by the unit test to signal that the SystemClockSyncObservation has
  // received a response to the GetLastSyncInfo call from the SystemClock
  // service.
  void set_on_last_sync_info_for_testing(
      base::OnceClosure on_last_sync_info_for_testing) {
    on_last_sync_info_for_testing_ = std::move(on_last_sync_info_for_testing);
  }

 private:
  SystemClockSyncObservation(SystemClockClient* system_clock_client,
                             base::TimeDelta timeout,
                             SystemClockSyncCallback callback);

  SystemClockSyncObservation(const SystemClockSyncObservation& other) = delete;
  SystemClockSyncObservation& operator=(
      const SystemClockSyncObservation& other) = delete;

  void OnSystemClockServiceAvailable(bool service_is_available);

  // Called on initial fetch of the system clock sync state, and when the system
  // clock sync state has changed.
  void OnLastSyncInfo(bool network_synchronized);

  // Called when the time out has been reached.
  void OnTimeout();

  // Runs the callback passed to Start with |result|.
  // If called multiple times, only the first call is respected.
  void RunCallbackWithResult(bool result);

  // SystemClockClient::Observer:
  void SystemClockUpdated() override;

  const raw_ptr<SystemClockClient> system_clock_client_;

  // The callback to be called when the system clock has been synchronized or
  // the timeout tracked by `timeout_timer_` has been reached.
  SystemClockSyncCallback callback_;

  // A timer which will fire when the timeout passed to Start expires.
  base::OneShotTimer timeout_timer_;

  // If set, executed when the SystemClock D-Bus service GetLastSyncInfo
  // response arrives. Only used for testing.
  base::OnceClosure on_last_sync_info_for_testing_;

  base::ScopedObservation<SystemClockClient, SystemClockClient::Observer>
      system_clock_client_observation_{this};

  // This weak pointer factory is used to discard previous previously triggered
  // waits if Start is called multiple times.
  base::WeakPtrFactory<SystemClockSyncObservation> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SYSTEM_CLOCK_SYSTEM_CLOCK_SYNC_OBSERVATION_H_

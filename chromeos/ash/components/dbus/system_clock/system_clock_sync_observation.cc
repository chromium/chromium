// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/system_clock/system_clock_sync_observation.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"

namespace ash {

// static
std::unique_ptr<SystemClockSyncObservation>
SystemClockSyncObservation::WaitForSystemClockSync(
    SystemClockClient* system_clock_client,
    base::TimeDelta timeout,
    SystemClockSyncCallback callback) {
  return base::WrapUnique(new SystemClockSyncObservation(
      system_clock_client, timeout, std::move(callback)));
}

SystemClockSyncObservation::SystemClockSyncObservation(
    SystemClockClient* system_clock_client,
    base::TimeDelta timeout,
    SystemClockSyncCallback callback)
    : system_clock_client_(system_clock_client) {
  callback_ = std::move(callback);

  timeout_timer_.Start(FROM_HERE, timeout,
                       base::BindOnce(&SystemClockSyncObservation::OnTimeout,
                                      weak_ptr_factory_.GetWeakPtr()));

  system_clock_client_observation_.Observe(SystemClockClient::Get());
  system_clock_client_->WaitForServiceToBeAvailable(
      base::BindOnce(&SystemClockSyncObservation::OnSystemClockServiceAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

SystemClockSyncObservation::~SystemClockSyncObservation() = default;

// Called when the system clock service is available, or when it is known that
// the system clock service is not available.
void SystemClockSyncObservation::OnSystemClockServiceAvailable(
    bool service_is_available) {
  if (!service_is_available) {
    RunCallbackWithResult(false);
    return;
  }

  system_clock_client_->GetLastSyncInfo(
      base::BindOnce(&SystemClockSyncObservation::OnLastSyncInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

// Called on initial fetch of the system clock sync state, and when the system
// clock sync state has changed.
void SystemClockSyncObservation::OnLastSyncInfo(bool network_synchronized) {
  if (on_last_sync_info_for_testing_)
    std::move(on_last_sync_info_for_testing_).Run();

  if (!network_synchronized) {
    // Don't notify the callback, wait for (another) SystemClockUpdated()
    // notification.
    return;
  }

  RunCallbackWithResult(true);
}

// Called when the time out has been reached.
void SystemClockSyncObservation::OnTimeout() {
  RunCallbackWithResult(false);
}

void SystemClockSyncObservation::RunCallbackWithResult(bool result) {
  if (callback_.is_null())
    return;

  timeout_timer_.AbandonAndStop();
  weak_ptr_factory_.InvalidateWeakPtrs();

  std::move(callback_).Run(result);
}

// SystemClockClient::Observer:
void SystemClockSyncObservation::SystemClockUpdated() {
  system_clock_client_->GetLastSyncInfo(
      base::BindOnce(&SystemClockSyncObservation::OnLastSyncInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace ash

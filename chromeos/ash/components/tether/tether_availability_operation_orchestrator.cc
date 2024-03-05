// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_availability_operation_orchestrator.h"

namespace ash::tether {

TetherAvailabilityOperationOrchestrator::Factory::~Factory() = default;

TetherAvailabilityOperationOrchestrator::
    TetherAvailabilityOperationOrchestrator() = default;
TetherAvailabilityOperationOrchestrator::
    ~TetherAvailabilityOperationOrchestrator() = default;

void TetherAvailabilityOperationOrchestrator::StartOperation(
    const multidevice::RemoteDeviceRef& remote_device) {
  // TODO: Implement this functionality.
  NOTREACHED_NORETURN();
}

void TetherAvailabilityOperationOrchestrator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TetherAvailabilityOperationOrchestrator::RemoveObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

void TetherAvailabilityOperationOrchestrator::OnScannedDeviceResult(
    const multidevice::RemoteDeviceRef& remote_device,
    std::optional<ScannedDeviceResult> result) {
  // TODO: Implement this functionality
  NOTREACHED_NORETURN();
}

void TetherAvailabilityOperationOrchestrator::NotifyObserversOfFinalScan() {
  for (auto& observer : observers_) {
    observer.OnTetherAvailabilityResponse(
        scanned_device_list_so_far_, gms_core_notifications_disabled_devices_,
        /*is_final_scan_result=*/true);
  }
}

}  // namespace ash::tether

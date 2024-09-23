// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_availability_operation_orchestrator.h"

#include "base/containers/contains.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash::tether {

TetherAvailabilityOperationOrchestrator::Factory::~Factory() = default;

TetherAvailabilityOperationOrchestrator::
    TetherAvailabilityOperationOrchestrator(
        std::unique_ptr<TetherAvailabilityOperation::Initializer>
            tether_availability_operation_initializer)
    : tether_availability_operation_initializer_(
          std::move(tether_availability_operation_initializer)) {}

TetherAvailabilityOperationOrchestrator::
    ~TetherAvailabilityOperationOrchestrator() = default;

void TetherAvailabilityOperationOrchestrator::StartOperation(
    const TetherHost& tether_host) {
  PA_LOG(VERBOSE) << "Starting TetherAvailabilityOperation for "
                  << tether_host.GetTruncatedDeviceIdForLogs() << ".";
  if (base::Contains(active_operations_, tether_host.GetDeviceId())) {
    PA_LOG(ERROR)
        << "Unable to start TetherAvailability operation for "
        << tether_host.GetTruncatedDeviceIdForLogs()
        << " since there is already an active operation for this device.";
    return;
  }

  active_operations_[tether_host.GetDeviceId()] =
      tether_availability_operation_initializer_->Initialize(
          tether_host,
          base::BindOnce(
              &TetherAvailabilityOperationOrchestrator::OnScannedDeviceResult,
              weak_ptr_factory_.GetWeakPtr(), tether_host));

  PA_LOG(VERBOSE) << "Started TetherAvailabilityOperation for "
                  << tether_host.GetTruncatedDeviceIdForLogs() << ".";
}

void TetherAvailabilityOperationOrchestrator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TetherAvailabilityOperationOrchestrator::RemoveObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

void TetherAvailabilityOperationOrchestrator::OnScannedDeviceResult(
    const TetherHost& tether_host,
    std::optional<ScannedDeviceInfo> result) {
  CHECK(base::Contains(active_operations_, tether_host.GetDeviceId()));

  if (result.has_value()) {
    ScannedDeviceInfo scanned_device_info = result.value();

    if (scanned_device_info.notifications_enabled) {
      PA_LOG(INFO) << "Received successful TetherAvailabilityResponse from "
                      "device with ID "
                   << tether_host.GetTruncatedDeviceIdForLogs() << ".";
      scanned_device_list_so_far_.push_back(result.value());
    } else {
      PA_LOG(WARNING)
          << "Received TetherAvailabilityResponse from device with ID "
          << tether_host.GetTruncatedDeviceIdForLogs() << " which "
          << "indicates that Google Play Services notifications are disabled";
      gms_core_notifications_disabled_devices_.emplace_back(result.value());
    }
  }

  active_operations_.erase(tether_host.GetDeviceId());

  bool is_final_scan_result = active_operations_.empty();
  for (auto& observer : observers_) {
    observer.OnTetherAvailabilityResponse(
        scanned_device_list_so_far_, gms_core_notifications_disabled_devices_,
        is_final_scan_result);
  }
}

void TetherAvailabilityOperationOrchestrator::NotifyObserversOfFinalScan() {
  for (auto& observer : observers_) {
    observer.OnTetherAvailabilityResponse(
        scanned_device_list_so_far_, gms_core_notifications_disabled_devices_,
        /*is_final_scan_result=*/true);
  }
}

}  // namespace ash::tether

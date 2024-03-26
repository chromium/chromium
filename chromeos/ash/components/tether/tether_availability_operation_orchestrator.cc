// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_availability_operation_orchestrator.h"
#include "base/containers/contains.h"

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
    const multidevice::RemoteDeviceRef& remote_device) {
  PA_LOG(VERBOSE) << "Starting TetherAvailabilityOperation for "
                  << remote_device.GetTruncatedDeviceIdForLogs() << ".";
  if (base::Contains(active_operations_, remote_device)) {
    PA_LOG(ERROR)
        << "Unable to start TetherAvailability operation for "
        << remote_device.GetTruncatedDeviceIdForLogs()
        << " since there is already an active operation for this device.";
    return;
  }

  active_operations_[remote_device] =
      tether_availability_operation_initializer_->Initialize(
          remote_device,
          base::BindOnce(
              &TetherAvailabilityOperationOrchestrator::OnScannedDeviceResult,
              weak_ptr_factory_.GetWeakPtr(), remote_device));

  PA_LOG(VERBOSE) << "Started TetherAvailabilityOperation for "
                  << remote_device.GetTruncatedDeviceIdForLogs() << ".";
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
  CHECK(base::Contains(active_operations_, remote_device));

  if (result.has_value()) {
    ScannedDeviceResult scanned_device_result = result.value();

    if (scanned_device_result.has_value()) {
      PA_LOG(INFO) << "Received successful TetherAvailabilityResponse from "
                      "device with ID "
                   << remote_device.GetTruncatedDeviceIdForLogs() << ".";
      scanned_device_list_so_far_.push_back(scanned_device_result.value());
    } else {
      ScannedDeviceInfoError error = scanned_device_result.error();
      if (error == ScannedDeviceInfoError::kNotificationsDisabled) {
        PA_LOG(WARNING)
            << "Received TetherAvailabilityResponse from device with ID "
            << remote_device.GetTruncatedDeviceIdForLogs() << " which "
            << "indicates that Google Play Services notifications are disabled";
        gms_core_notifications_disabled_devices_.push_back(remote_device);
      }
    }
  }

  active_operations_.erase(remote_device);

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

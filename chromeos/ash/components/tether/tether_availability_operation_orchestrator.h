// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_AVAILABILITY_OPERATION_ORCHESTRATOR_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_AVAILABILITY_OPERATION_ORCHESTRATOR_H_

#include "base/observer_list.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/tether/scanned_device_info.h"
#include "chromeos/ash/components/tether/tether_availability_operation.h"

namespace ash::tether {

class TetherAvailabilityOperationOrchestrator {
 public:
  class Factory {
   public:
    virtual ~Factory();

    virtual std::unique_ptr<TetherAvailabilityOperationOrchestrator>
    CreateInstance() = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Invoked once with an empty list when the operation begins, then invoked
    // repeatedly once each result comes in. After all devices have been
    // processed, the callback is invoked one final time with
    // |is_final_scan_result| = true.
    virtual void OnTetherAvailabilityResponse(
        const std::vector<ScannedDeviceInfo>& scanned_device_list_so_far,
        const multidevice::RemoteDeviceRefList&
            gms_core_notifications_disabled_devices,
        bool is_final_scan_result) = 0;
  };

  explicit TetherAvailabilityOperationOrchestrator(
      std::unique_ptr<TetherAvailabilityOperation::Initializer>
          tether_availability_operation_initializer);
  virtual ~TetherAvailabilityOperationOrchestrator();
  TetherAvailabilityOperationOrchestrator(
      const TetherAvailabilityOperationOrchestrator&) = delete;
  TetherAvailabilityOperationOrchestrator& operator=(
      const TetherAvailabilityOperationOrchestrator&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  virtual void Start() = 0;

 protected:
  void StartOperation(const multidevice::RemoteDeviceRef& remote_device);
  void NotifyObserversOfFinalScan();

  base::ObserverList<Observer> observers_;
  std::vector<ScannedDeviceInfo> scanned_device_list_so_far_;
  multidevice::RemoteDeviceRefList gms_core_notifications_disabled_devices_;

 private:
  void OnScannedDeviceResult(const multidevice::RemoteDeviceRef& remote_device,
                             std::optional<ScannedDeviceResult> result);

 private:
  base::flat_map<multidevice::RemoteDeviceRef,
                 std::unique_ptr<TetherAvailabilityOperation>>
      active_operations_;

  std::unique_ptr<TetherAvailabilityOperation::Initializer>
      tether_availability_operation_initializer_;

  base::WeakPtrFactory<TetherAvailabilityOperationOrchestrator>
      weak_ptr_factory_{this};
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_AVAILABILITY_OPERATION_ORCHESTRATOR_H_

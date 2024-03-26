// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_AVAILABILITY_OPERATION_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_AVAILABILITY_OPERATION_H_

#include "chromeos/ash/components/tether/tether_availability_operation.h"

namespace ash::tether {

class FakeTetherAvailabilityOperation : public TetherAvailabilityOperation {
 public:
  class Initializer : public TetherAvailabilityOperation::Initializer {
   public:
    Initializer();
    ~Initializer() override;

    // TetherAvailabilityOperation::Initializer:
    std::unique_ptr<TetherAvailabilityOperation> Initialize(
        const multidevice::RemoteDeviceRef& device_to_connect,
        TetherAvailabilityOperation::
            OnTetherAvailabilityOperationFinishedCallback callback) override;

    void send_result(const multidevice::RemoteDeviceRef& remote_device,
                     std::optional<ScannedDeviceResult> result);
    bool has_active_operation_for_device(
        const multidevice::RemoteDeviceRef& remote_device);

   private:
    void OnOperationDestroyed(const multidevice::RemoteDeviceRef remote_device);

    base::flat_map<multidevice::RemoteDeviceRef,
                   TetherAvailabilityOperation::
                       OnTetherAvailabilityOperationFinishedCallback>
        pending_callbacks_;

    base::WeakPtrFactory<FakeTetherAvailabilityOperation::Initializer>
        weak_ptr_factory_{this};
  };

  FakeTetherAvailabilityOperation(
      const multidevice::RemoteDeviceRef remote_device,
      base::OnceClosure on_destroyed_callback);
  ~FakeTetherAvailabilityOperation() override;

 private:
  base::OnceClosure on_destroyed_callback_;
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_AVAILABILITY_OPERATION_H_


// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_tether_availability_operation.h"
#include "base/containers/contains.h"

namespace ash::tether {
FakeTetherAvailabilityOperation::Initializer::Initializer()
    : TetherAvailabilityOperation::Initializer::Initializer(
          /*device_sync_client=*/nullptr,
          /*secure_channel_client=*/nullptr,
          /*tether_host_response_recorder=*/nullptr,
          /*connection_preserver=*/nullptr) {}

FakeTetherAvailabilityOperation::Initializer::~Initializer() = default;

std::unique_ptr<TetherAvailabilityOperation>
FakeTetherAvailabilityOperation::Initializer::Initialize(
    const multidevice::RemoteDeviceRef& device_to_connect,
    TetherAvailabilityOperation::OnTetherAvailabilityOperationFinishedCallback
        callback) {
  pending_callbacks_[device_to_connect] = std::move(callback);
  return base::WrapUnique<
      TetherAvailabilityOperation>(new FakeTetherAvailabilityOperation(
      device_to_connect,
      base::BindOnce(
          &FakeTetherAvailabilityOperation::Initializer::OnOperationDestroyed,
          weak_ptr_factory_.GetWeakPtr(), device_to_connect)));
}

void FakeTetherAvailabilityOperation::Initializer::send_result(
    const multidevice::RemoteDeviceRef& remote_device,
    std::optional<ScannedDeviceResult> result) {
  std::move(pending_callbacks_[remote_device]).Run(result);
  pending_callbacks_.erase(remote_device);
}

bool FakeTetherAvailabilityOperation::Initializer::
    has_active_operation_for_device(
        const multidevice::RemoteDeviceRef& remote_device) {
  return base::Contains(pending_callbacks_, remote_device);
}

void FakeTetherAvailabilityOperation::Initializer::OnOperationDestroyed(
    const multidevice::RemoteDeviceRef remote_device) {
  pending_callbacks_.erase(remote_device);
}

FakeTetherAvailabilityOperation::FakeTetherAvailabilityOperation(
    const multidevice::RemoteDeviceRef remote_device,
    base::OnceClosure on_destroyed_callback)
    : TetherAvailabilityOperation(remote_device,
                                  base::DoNothing(),
                                  /*device_sync_client=*/nullptr,
                                  /*secure_channel_client=*/nullptr,
                                  /*tether_host_response_recorder=*/nullptr,
                                  /*connection_preserver=*/nullptr),
      on_destroyed_callback_(std::move(on_destroyed_callback)) {}

FakeTetherAvailabilityOperation::~FakeTetherAvailabilityOperation() {
  std::move(on_destroyed_callback_).Run();
}

}  // namespace ash::tether

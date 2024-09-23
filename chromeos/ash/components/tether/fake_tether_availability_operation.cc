
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_tether_availability_operation.h"

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"

namespace ash::tether {
FakeTetherAvailabilityOperation::Initializer::Initializer()
    : TetherAvailabilityOperation::Initializer::Initializer(
          /*host_connection_factory=*/nullptr,
          /*tether_host()response_recorder=*/nullptr,
          /*connection_preserver=*/nullptr) {}

FakeTetherAvailabilityOperation::Initializer::~Initializer() = default;

std::unique_ptr<TetherAvailabilityOperation>
FakeTetherAvailabilityOperation::Initializer::Initialize(
    const TetherHost& tether_host,
    TetherAvailabilityOperation::OnTetherAvailabilityOperationFinishedCallback
        callback) {
  pending_callbacks_[tether_host.GetDeviceId()] = std::move(callback);
  return base::WrapUnique<
      TetherAvailabilityOperation>(new FakeTetherAvailabilityOperation(
      tether_host,
      base::BindOnce(
          &FakeTetherAvailabilityOperation::Initializer::OnOperationDestroyed,
          weak_ptr_factory_.GetWeakPtr(), tether_host)));
}

void FakeTetherAvailabilityOperation::Initializer::send_result(
    const TetherHost& tether_host,
    std::optional<ScannedDeviceInfo> result) {
  std::move(pending_callbacks_[tether_host.GetDeviceId()]).Run(result);
  pending_callbacks_.erase(tether_host.GetDeviceId());
}

bool FakeTetherAvailabilityOperation::Initializer::
    has_active_operation_for_device(const TetherHost& tether_host) {
  return base::Contains(pending_callbacks_, tether_host.GetDeviceId());
}

void FakeTetherAvailabilityOperation::Initializer::OnOperationDestroyed(
    const TetherHost& tether_host) {
  pending_callbacks_.erase(tether_host.GetDeviceId());
}

FakeTetherAvailabilityOperation::FakeTetherAvailabilityOperation(
    const TetherHost& tether_host,
    base::OnceClosure on_destroyed_callback)
    : TetherAvailabilityOperation(tether_host,
                                  base::DoNothing(),
                                  /*host_connection_factory=*/nullptr,
                                  /*tether_host()response_recorder=*/nullptr,
                                  /*connection_preserver=*/nullptr),
      on_destroyed_callback_(std::move(on_destroyed_callback)) {}

FakeTetherAvailabilityOperation::~FakeTetherAvailabilityOperation() {
  std::move(on_destroyed_callback_).Run();
}

}  // namespace ash::tether

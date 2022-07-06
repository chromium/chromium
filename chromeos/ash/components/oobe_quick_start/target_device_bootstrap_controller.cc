// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/target_device_bootstrap_controller.h"

#include "base/callback_helpers.h"
#include "chromeos/ash/components/oobe_quick_start/connectivity/target_device_connection_broker_factory.h"

namespace ash::quick_start {

TargetDeviceBootstrapController::TargetDeviceBootstrapController() {
  connection_broker_ = TargetDeviceConnectionBrokerFactory::Create();
}

TargetDeviceBootstrapController::~TargetDeviceBootstrapController() = default;

void TargetDeviceBootstrapController::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void TargetDeviceBootstrapController::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

base::WeakPtr<TargetDeviceBootstrapController>
TargetDeviceBootstrapController::GetAsWeakPtrForClient() {
  // Only one client at a time should have a pointer.
  DCHECK(!weak_ptr_factory_for_clients_.HasWeakPtrs());
  return weak_ptr_factory_for_clients_.GetWeakPtr();
}

void TargetDeviceBootstrapController::StartAdvertising() {
  DCHECK(connection_broker_->GetFeatureSupportStatus() ==
         TargetDeviceConnectionBroker::FeatureSupportStatus::kSupported);
  // TODO: Handle result callback
  connection_broker_->StartAdvertising(this, base::DoNothing());
}

void TargetDeviceBootstrapController::StopAdvertising() {
  // TODO: Handle result callback
  connection_broker_->StopAdvertising(base::DoNothing());
}

}  // namespace ash::quick_start

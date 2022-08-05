// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/target_device_bootstrap_controller.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "chromeos/ash/components/oobe_quick_start/connectivity/target_device_connection_broker_factory.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace ash::quick_start {

TargetDeviceBootstrapController::TargetDeviceBootstrapController() {
  connection_broker_ = TargetDeviceConnectionBrokerFactory::Create();
}

TargetDeviceBootstrapController::~TargetDeviceBootstrapController() = default;

TargetDeviceBootstrapController::Status::Status() = default;
TargetDeviceBootstrapController::Status::~Status() = default;

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
  DCHECK_EQ(status_.step, Step::NONE);

  // No pending requests.
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());

  status_.step = Step::ADVERTISING;
  connection_broker_->StartAdvertising(
      this,
      base::BindOnce(&TargetDeviceBootstrapController::OnStartAdvertisingResult,
                     weak_ptr_factory_.GetWeakPtr()));
  NotifyObservers();
}

void TargetDeviceBootstrapController::StopAdvertising() {
  DCHECK_EQ(status_.step, Step::ADVERTISING);

  // No pending requests.
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());

  connection_broker_->StopAdvertising(
      base::BindOnce(&TargetDeviceBootstrapController::OnStopAdvertising,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TargetDeviceBootstrapController::OnIncomingConnectionInitiated(
    const std::string& source_device_id,
    base::WeakPtr<TargetDeviceConnectionBroker::IncomingConnection>
        connection) {
  // TODO(b/239855593): Implement
  NOTIMPLEMENTED();
}

void TargetDeviceBootstrapController::OnConnectionAccepted(
    const std::string& source_device_id,
    base::WeakPtr<TargetDeviceConnectionBroker::AcceptedConnection>
        connection) {
  // TODO(b/239855593): Implement
  NOTIMPLEMENTED();
}

void TargetDeviceBootstrapController::OnConnectionRejected(
    const std::string& source_device_id) {
  // TODO(b/239855593): Implement
  NOTIMPLEMENTED();
}

void TargetDeviceBootstrapController::OnConnectionClosed(
    const std::string& source_device_id) {
  // TODO(b/239855593): Implement
  NOTIMPLEMENTED();
}

void TargetDeviceBootstrapController::NotifyObservers() {
  for (auto& obs : observers_) {
    obs.OnStatusChanged(status_);
  }
}

void TargetDeviceBootstrapController::OnStartAdvertisingResult(bool success) {
  DCHECK_EQ(status_.step, Step::ADVERTISING);
  if (success)
    return;
  status_.step = Step::ERROR;
  status_.payload = ErrorCode::START_ADVERTISING_FAILED;
  NotifyObservers();
}

void TargetDeviceBootstrapController::OnStopAdvertising() {
  DCHECK_EQ(status_.step, Step::ADVERTISING);

  status_.step = Step::NONE;
  status_.payload.emplace<absl::monostate>();
  NotifyObservers();
}

}  // namespace ash::quick_start

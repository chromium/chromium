// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_initiator_operation.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/services/secure_channel/authenticated_channel.h"
#include "chromeos/services/secure_channel/ble_connection_manager.h"

namespace chromeos {

namespace secure_channel {

// static
BleInitiatorOperation::Factory* BleInitiatorOperation::Factory::test_factory_ =
    nullptr;

// static
BleInitiatorOperation::Factory* BleInitiatorOperation::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void BleInitiatorOperation::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

BleInitiatorOperation::Factory::~Factory() = default;

std::unique_ptr<ConnectToDeviceOperation<BleInitiatorFailureType>>
BleInitiatorOperation::Factory::BuildInstance(
    BleConnectionManager* ble_connection_manager,
    ConnectToDeviceOperation<BleInitiatorFailureType>::ConnectionSuccessCallback
        success_callback,
    const ConnectToDeviceOperation<
        BleInitiatorFailureType>::ConnectionFailedCallback& failure_callback,
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority,
    scoped_refptr<base::TaskRunner> task_runner) {
  return base::WrapUnique(new BleInitiatorOperation(
      ble_connection_manager, std::move(success_callback),
      std::move(failure_callback), device_id_pair, connection_priority,
      task_runner));
}
BleInitiatorOperation::BleInitiatorOperation(
    BleConnectionManager* ble_connection_manager,
    ConnectToDeviceOperation<BleInitiatorFailureType>::ConnectionSuccessCallback
        success_callback,
    const ConnectToDeviceOperation<
        BleInitiatorFailureType>::ConnectionFailedCallback& failure_callback,
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority,
    scoped_refptr<base::TaskRunner> task_runner)
    : ConnectToDeviceOperationBase<BleInitiatorFailureType>(
          std::move(success_callback),
          std::move(failure_callback),
          device_id_pair,
          connection_priority,
          task_runner),
      ble_connection_manager_(ble_connection_manager) {}

BleInitiatorOperation::~BleInitiatorOperation() = default;

void BleInitiatorOperation::PerformAttemptConnectionToDevice(
    ConnectionPriority connection_priority) {
  is_attempt_active_ = true;
  ble_connection_manager_->AttemptBleInitiatorConnection(
      device_id_pair(), connection_priority,
      base::BindOnce(&BleInitiatorOperation::OnSuccessfulConnection,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&BleInitiatorOperation::OnConnectionFailure,
                          weak_ptr_factory_.GetWeakPtr()));
}

void BleInitiatorOperation::PerformCancellation() {
  is_attempt_active_ = false;
  ble_connection_manager_->CancelBleInitiatorConnectionAttempt(
      device_id_pair());
}

void BleInitiatorOperation::PerformUpdateConnectionPriority(
    ConnectionPriority connection_priority) {
  ble_connection_manager_->UpdateBleInitiatorConnectionPriority(
      device_id_pair(), connection_priority);
}

void BleInitiatorOperation::OnSuccessfulConnection(
    std::unique_ptr<AuthenticatedChannel> authenticated_channel) {
  is_attempt_active_ = false;
  OnSuccessfulConnectionAttempt(std::move(authenticated_channel));
}

void BleInitiatorOperation::OnConnectionFailure(
    BleInitiatorFailureType failure_type) {
  OnFailedConnectionAttempt(failure_type);
}

}  // namespace secure_channel

}  // namespace chromeos

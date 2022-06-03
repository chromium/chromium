// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_initiator_connection_attempt.h"

#include "base/memory/ptr_util.h"
#include "chromeos/services/secure_channel/ble_initiator_operation.h"

namespace chromeos {

namespace secure_channel {

// static
BleInitiatorConnectionAttempt::Factory*
    BleInitiatorConnectionAttempt::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<ConnectionAttempt<BleInitiatorFailureType>>
BleInitiatorConnectionAttempt::Factory::Create(
    BleConnectionManager* ble_connection_manager,
    ConnectionAttemptDelegate* delegate,
    const ConnectionAttemptDetails& connection_attempt_details) {
  if (test_factory_) {
    return test_factory_->CreateInstance(ble_connection_manager, delegate,
                                         connection_attempt_details);
  }

  return base::WrapUnique(new BleInitiatorConnectionAttempt(
      ble_connection_manager, delegate, connection_attempt_details));
}

// static
void BleInitiatorConnectionAttempt::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

BleInitiatorConnectionAttempt::Factory::~Factory() = default;

BleInitiatorConnectionAttempt::BleInitiatorConnectionAttempt(
    BleConnectionManager* ble_connection_manager,
    ConnectionAttemptDelegate* delegate,
    const ConnectionAttemptDetails& connection_attempt_details)
    : ConnectionAttemptBase<BleInitiatorFailureType>(
          delegate,
          connection_attempt_details),
      ble_connection_manager_(ble_connection_manager) {}

BleInitiatorConnectionAttempt::~BleInitiatorConnectionAttempt() = default;

std::unique_ptr<ConnectToDeviceOperation<BleInitiatorFailureType>>
BleInitiatorConnectionAttempt::CreateConnectToDeviceOperation(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority,
    ConnectToDeviceOperation<BleInitiatorFailureType>::ConnectionSuccessCallback
        success_callback,
    const ConnectToDeviceOperation<
        BleInitiatorFailureType>::ConnectionFailedCallback& failure_callback) {
  return BleInitiatorOperation::Factory::Create(
      ble_connection_manager_, std::move(success_callback), failure_callback,
      device_id_pair, connection_priority);
}

}  // namespace secure_channel

}  // namespace chromeos

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/ble_listener_connection_attempt.h"

#include "base/memory/ptr_util.h"
#include "chromeos/ash/services/secure_channel/ble_listener_operation.h"
#include "chromeos/ash/services/secure_channel/connection_metrics_logger.h"

namespace ash::secure_channel {

// static
BleListenerConnectionAttempt::Factory*
    BleListenerConnectionAttempt::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<ConnectionAttempt<BleListenerFailureType>>
BleListenerConnectionAttempt::Factory::Create(
    BleConnectionManager* ble_connection_manager,
    ConnectionAttemptDelegate* delegate,
    const ConnectionAttemptDetails& connection_attempt_details) {
  if (test_factory_) {
    return test_factory_->CreateInstance(ble_connection_manager, delegate,
                                         connection_attempt_details);
  }

  return base::WrapUnique(new BleListenerConnectionAttempt(
      ble_connection_manager, delegate, connection_attempt_details));
}

// static
void BleListenerConnectionAttempt::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

BleListenerConnectionAttempt::Factory::~Factory() = default;

BleListenerConnectionAttempt::BleListenerConnectionAttempt(
    BleConnectionManager* ble_connection_manager,
    ConnectionAttemptDelegate* delegate,
    const ConnectionAttemptDetails& connection_attempt_details)
    : ConnectionAttemptBase<BleListenerFailureType>(delegate,
                                                    connection_attempt_details),
      ble_connection_manager_(ble_connection_manager) {}

BleListenerConnectionAttempt::~BleListenerConnectionAttempt() = default;

std::unique_ptr<ConnectToDeviceOperation<BleListenerFailureType>>
BleListenerConnectionAttempt::CreateConnectToDeviceOperation(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority,
    ConnectToDeviceOperation<BleListenerFailureType>::ConnectionSuccessCallback
        success_callback,
    const ConnectToDeviceOperation<
        BleListenerFailureType>::ConnectionFailedCallback& failure_callback) {
  return BleListenerOperation::Factory::Create(
      ble_connection_manager_, std::move(success_callback), failure_callback,
      device_id_pair, connection_priority);
}

void BleListenerConnectionAttempt::ProcessSuccessfulConnectionDuration(
    const base::TimeDelta& duration) {
  LogLatencyMetric(
      "MultiDevice.SecureChannel.BLE.Performance."
      "StartScanToAuthenticationDuration.Background",
      duration);
}

}  // namespace ash::secure_channel

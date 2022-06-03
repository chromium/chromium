// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/pending_ble_listener_connection_request.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {

namespace secure_channel {

namespace {
const char kBleListenerReadableRequestTypeForLogging[] = "BLE Listener";
}  // namespace

// static
PendingBleListenerConnectionRequest::Factory*
    PendingBleListenerConnectionRequest::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<PendingConnectionRequest<BleListenerFailureType>>
PendingBleListenerConnectionRequest::Factory::Create(
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    ConnectionPriority connection_priority,
    PendingConnectionRequestDelegate* delegate,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  if (test_factory_) {
    return test_factory_->CreateInstance(
        std::move(client_connection_parameters), connection_priority, delegate,
        bluetooth_adapter);
  }

  return base::WrapUnique(new PendingBleListenerConnectionRequest(
      std::move(client_connection_parameters), connection_priority, delegate,
      bluetooth_adapter));
}

// static
void PendingBleListenerConnectionRequest::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

PendingBleListenerConnectionRequest::Factory::~Factory() = default;

PendingBleListenerConnectionRequest::PendingBleListenerConnectionRequest(
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    ConnectionPriority connection_priority,
    PendingConnectionRequestDelegate* delegate,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : PendingBleConnectionRequestBase<BleListenerFailureType>(
          std::move(client_connection_parameters),
          connection_priority,
          kBleListenerReadableRequestTypeForLogging,
          delegate,
          std::move(bluetooth_adapter)) {}

PendingBleListenerConnectionRequest::~PendingBleListenerConnectionRequest() =
    default;

void PendingBleListenerConnectionRequest::HandleConnectionFailure(
    BleListenerFailureType failure_detail) {
  DCHECK_EQ(BleListenerFailureType::kAuthenticationError, failure_detail);

  // Authentication errors cannot be solved via a retry. This situation
  // likely means that the keys for this device or the remote device are out
  // of sync.
  StopRequestDueToConnectionFailures(
      mojom::ConnectionAttemptFailureReason::AUTHENTICATION_ERROR);
}

}  // namespace secure_channel

}  // namespace chromeos

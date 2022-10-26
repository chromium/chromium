// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/pending_ble_initiator_connection_request.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace ash::secure_channel {

namespace {

const char kBleInitiatorReadableRequestTypeForLogging[] = "BLE Initiator";

}  // namespace

// The number of times to attempt to connect to a device without receiving any
// response before giving up. When a connection to a device is attempted, a
// BLE discovery session listens for advertisements from the remote device as
// the first step of the connection; if no advertisement is picked up, it is
// likely that the remote device is not nearby or is not currently responding
// to connection requests.
const size_t PendingBleInitiatorConnectionRequest::kMaxEmptyScansPerDevice = 3u;

// The number of times to attempt a GATT connection to a device after a BLE
// discovery session has already detected a nearby device. GATT connections
// may fail for a variety of reasons, but most failures are ephemeral. Thus,
// more connection attempts are allowed in such cases since it is likely that
// a subsequent attempt will succeed. See https://crbug.com/805218.
const size_t
    PendingBleInitiatorConnectionRequest::kMaxGattConnectionAttemptsPerDevice =
        6u;

// static
PendingBleInitiatorConnectionRequest::Factory*
    PendingBleInitiatorConnectionRequest::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<PendingConnectionRequest<BleInitiatorFailureType>>
PendingBleInitiatorConnectionRequest::Factory::Create(
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    ConnectionPriority connection_priority,
    PendingConnectionRequestDelegate* delegate,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  if (test_factory_) {
    return test_factory_->CreateInstance(
        std::move(client_connection_parameters), connection_priority, delegate,
        bluetooth_adapter);
  }

  return base::WrapUnique(new PendingBleInitiatorConnectionRequest(
      std::move(client_connection_parameters), connection_priority, delegate,
      bluetooth_adapter));
}

// static
void PendingBleInitiatorConnectionRequest::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

PendingBleInitiatorConnectionRequest::Factory::~Factory() = default;

PendingBleInitiatorConnectionRequest::PendingBleInitiatorConnectionRequest(
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    ConnectionPriority connection_priority,
    PendingConnectionRequestDelegate* delegate,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : PendingBleConnectionRequestBase<BleInitiatorFailureType>(
          std::move(client_connection_parameters),
          connection_priority,
          kBleInitiatorReadableRequestTypeForLogging,
          delegate,
          std::move(bluetooth_adapter)) {}

PendingBleInitiatorConnectionRequest::~PendingBleInitiatorConnectionRequest() =
    default;

void PendingBleInitiatorConnectionRequest::HandleConnectionFailure(
    BleInitiatorFailureType failure_detail) {
  switch (failure_detail) {
    case BleInitiatorFailureType::kAuthenticationError:
      // Authentication errors cannot be solved via a retry. This situation
      // likely means that the keys for this device or the remote device are out
      // of sync.
      StopRequestDueToConnectionFailures(
          mojom::ConnectionAttemptFailureReason::AUTHENTICATION_ERROR);
      break;
    case BleInitiatorFailureType::kGattConnectionError:
      ++num_gatt_failures_;
      if (num_gatt_failures_ == kMaxGattConnectionAttemptsPerDevice) {
        StopRequestDueToConnectionFailures(
            mojom::ConnectionAttemptFailureReason::GATT_CONNECTION_ERROR);
      }
      break;
    case BleInitiatorFailureType::kInterruptedByHigherPriorityConnectionAttempt:
      // This failure was not due to an actual failure to connect, so there is
      // nothing extra to do.
      break;
    case BleInitiatorFailureType::kTimeoutContactingRemoteDevice:
      ++num_empty_scan_failures_;
      if (num_empty_scan_failures_ == kMaxEmptyScansPerDevice) {
        StopRequestDueToConnectionFailures(
            mojom::ConnectionAttemptFailureReason::TIMEOUT_FINDING_DEVICE);
      }
      break;
    case BleInitiatorFailureType::kCouldNotGenerateAdvertisement:
      // Valid BeaconSeeds are required for generating BLE advertisements and
      // scan filters.
      StopRequestDueToConnectionFailures(mojom::ConnectionAttemptFailureReason::
                                             COULD_NOT_GENERATE_ADVERTISEMENT);
      break;
  }
}

}  // namespace ash::secure_channel

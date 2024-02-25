// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/pending_nearby_initiator_connection_request.h"

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash::secure_channel {

namespace {

const char kRequestTypeForLogging[] = "Nearby Initiator";

}  // namespace

// static
PendingNearbyInitiatorConnectionRequest::Factory*
    PendingNearbyInitiatorConnectionRequest::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<PendingConnectionRequest<NearbyInitiatorFailureType>>
PendingNearbyInitiatorConnectionRequest::Factory::Create(
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    ConnectionPriority connection_priority,
    PendingConnectionRequestDelegate* delegate,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  if (test_factory_) {
    return test_factory_->CreateInstance(
        std::move(client_connection_parameters), connection_priority, delegate,
        bluetooth_adapter);
  }

  return base::WrapUnique(new PendingNearbyInitiatorConnectionRequest(
      std::move(client_connection_parameters), connection_priority, delegate,
      bluetooth_adapter));
}

// static
void PendingNearbyInitiatorConnectionRequest::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

PendingNearbyInitiatorConnectionRequest::Factory::~Factory() = default;

PendingNearbyInitiatorConnectionRequest::
    PendingNearbyInitiatorConnectionRequest(
        std::unique_ptr<ClientConnectionParameters>
            client_connection_parameters,
        ConnectionPriority connection_priority,
        PendingConnectionRequestDelegate* delegate,
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : PendingConnectionRequestBase<NearbyInitiatorFailureType>(
          std::move(client_connection_parameters),
          connection_priority,
          kRequestTypeForLogging,
          delegate),
      bluetooth_adapter_(std::move(bluetooth_adapter)) {
  bluetooth_adapter_->AddObserver(this);
}

PendingNearbyInitiatorConnectionRequest::
    ~PendingNearbyInitiatorConnectionRequest() {
  bluetooth_adapter_->RemoveObserver(this);
}

void PendingNearbyInitiatorConnectionRequest::HandleBleDiscoveryStateChange(
    mojom::DiscoveryResult discovery_state,
    std::optional<mojom::DiscoveryErrorCode> potential_error_code) {
  UpdateBleDiscoveryState(discovery_state, potential_error_code);
}
void PendingNearbyInitiatorConnectionRequest::HandleNearbyConnectionChange(
    mojom::NearbyConnectionStep step,
    mojom::NearbyConnectionStepResult result) {
  UpdateNearbyConnectionChange(step, result);
}
void PendingNearbyInitiatorConnectionRequest::HandleSecureChannelChanged(
    mojom::SecureChannelState secure_channel_state) {
  UpdateSecureChannelChange(secure_channel_state);
}

void PendingNearbyInitiatorConnectionRequest::HandleConnectionFailure(
    NearbyInitiatorFailureType failure_detail) {
  PA_LOG(INFO) << "Pending Nearby Connection failed : " << failure_detail;

  switch (failure_detail) {
    case NearbyInitiatorFailureType::kConnectivityError:
      StopRequestDueToConnectionFailures(
          mojom::ConnectionAttemptFailureReason::NEARBY_CONNECTION_ERROR);
      break;
    case NearbyInitiatorFailureType::kAuthenticationError:
      StopRequestDueToConnectionFailures(
          mojom::ConnectionAttemptFailureReason::AUTHENTICATION_ERROR);
      break;
  }
}

void PendingNearbyInitiatorConnectionRequest::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  DCHECK_EQ(bluetooth_adapter_, adapter);
  if (powered)
    return;

  StopRequestDueToConnectionFailures(
      mojom::ConnectionAttemptFailureReason::ADAPTER_DISABLED);
}

void PendingNearbyInitiatorConnectionRequest::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  DCHECK_EQ(bluetooth_adapter_, adapter);
  if (present)
    return;

  StopRequestDueToConnectionFailures(
      mojom::ConnectionAttemptFailureReason::ADAPTER_NOT_PRESENT);
}

}  // namespace ash::secure_channel

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_NEARBY_INITIATOR_CONNECTION_REQUEST_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_NEARBY_INITIATOR_CONNECTION_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "chromeos/ash/services/secure_channel/nearby_initiator_failure_type.h"
#include "chromeos/ash/services/secure_channel/pending_connection_request_base.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash::secure_channel {

// Pending request to create a connection via Nearby Connections in the
// initiator role.
class PendingNearbyInitiatorConnectionRequest
    : public PendingConnectionRequestBase<NearbyInitiatorFailureType>,
      public device::BluetoothAdapter::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<PendingConnectionRequest<NearbyInitiatorFailureType>>
    Create(std::unique_ptr<ClientConnectionParameters>
               client_connection_parameters,
           ConnectionPriority connection_priority,
           PendingConnectionRequestDelegate* delegate,
           scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<
        PendingConnectionRequest<NearbyInitiatorFailureType>>
    CreateInstance(
        std::unique_ptr<ClientConnectionParameters>
            client_connection_parameters,
        ConnectionPriority connection_priority,
        PendingConnectionRequestDelegate* delegate,
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) = 0;

   private:
    static Factory* test_factory_;
  };

  ~PendingNearbyInitiatorConnectionRequest() override;
  PendingNearbyInitiatorConnectionRequest(
      const PendingNearbyInitiatorConnectionRequest&) = delete;
  PendingNearbyInitiatorConnectionRequest& operator=(
      const PendingNearbyInitiatorConnectionRequest&) = delete;

 private:
  friend class SecureChannelPendingNearbyInitiatorConnectionRequestTest;

  PendingNearbyInitiatorConnectionRequest(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority,
      PendingConnectionRequestDelegate* delegate,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);

  // PendingConnectionRequest<NearbyInitiatorFailureType>:
  void HandleConnectionFailure(
      NearbyInitiatorFailureType failure_detail) override;
  void HandleBleDiscoveryStateChange(
      mojom::DiscoveryResult discovery_state,
      std::optional<mojom::DiscoveryErrorCode> potential_error_code) override;
  void HandleNearbyConnectionChange(
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result) override;
  void HandleSecureChannelChanged(
      mojom::SecureChannelState secure_channel_state) override;

  // device::BluetoothAdapter::Observer:
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_NEARBY_INITIATOR_CONNECTION_REQUEST_H_

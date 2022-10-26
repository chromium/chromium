// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_BLE_LISTENER_CONNECTION_REQUEST_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_BLE_LISTENER_CONNECTION_REQUEST_H_

#include <memory>

#include "chromeos/ash/services/secure_channel/ble_listener_failure_type.h"
#include "chromeos/ash/services/secure_channel/pending_ble_connection_request_base.h"

namespace ash::secure_channel {

class ClientConnectionParameters;
enum class ConnectionPriority;

// ConnectionRequest corresponding to BLE connections in the listener role.
class PendingBleListenerConnectionRequest
    : public PendingBleConnectionRequestBase<BleListenerFailureType> {
 public:
  class Factory {
   public:
    static std::unique_ptr<PendingConnectionRequest<BleListenerFailureType>>
    Create(std::unique_ptr<ClientConnectionParameters>
               client_connection_parameters,
           ConnectionPriority connection_priority,
           PendingConnectionRequestDelegate* delegate,
           scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<PendingConnectionRequest<BleListenerFailureType>>
    CreateInstance(
        std::unique_ptr<ClientConnectionParameters>
            client_connection_parameters,
        ConnectionPriority connection_priority,
        PendingConnectionRequestDelegate* delegate,
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) = 0;

   private:
    static Factory* test_factory_;
  };

  PendingBleListenerConnectionRequest(
      const PendingBleListenerConnectionRequest&) = delete;
  PendingBleListenerConnectionRequest& operator=(
      const PendingBleListenerConnectionRequest&) = delete;

  ~PendingBleListenerConnectionRequest() override;

 private:
  PendingBleListenerConnectionRequest(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority,
      PendingConnectionRequestDelegate* delegate,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);

  // PendingConnectionRequest<BleListenerFailureType>:
  void HandleConnectionFailure(BleListenerFailureType failure_detail) override;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_BLE_LISTENER_CONNECTION_REQUEST_H_

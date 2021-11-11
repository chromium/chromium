// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_BLE_LISTENER_CONNECTION_REQUEST_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_BLE_LISTENER_CONNECTION_REQUEST_H_

#include <memory>

#include "chromeos/services/secure_channel/ble_listener_failure_type.h"
#include "chromeos/services/secure_channel/client_connection_parameters.h"
#include "chromeos/services/secure_channel/pending_ble_connection_request_base.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace chromeos {

namespace secure_channel {

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

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_BLE_LISTENER_CONNECTION_REQUEST_H_

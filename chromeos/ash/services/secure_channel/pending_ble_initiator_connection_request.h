// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_BLE_INITIATOR_CONNECTION_REQUEST_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_BLE_INITIATOR_CONNECTION_REQUEST_H_

#include <memory>

#include "chromeos/ash/services/secure_channel/ble_initiator_failure_type.h"
#include "chromeos/ash/services/secure_channel/pending_ble_connection_request_base.h"

namespace ash::secure_channel {

class ClientConnectionParameters;
enum class ConnectionPriority;

// ConnectionRequest corresponding to BLE connections in the initiator role.
class PendingBleInitiatorConnectionRequest
    : public PendingBleConnectionRequestBase<BleInitiatorFailureType> {
 public:
  class Factory {
   public:
    static std::unique_ptr<PendingConnectionRequest<BleInitiatorFailureType>>
    Create(std::unique_ptr<ClientConnectionParameters>
               client_connection_parameters,
           ConnectionPriority connection_priority,
           PendingConnectionRequestDelegate* delegate,
           scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<PendingConnectionRequest<BleInitiatorFailureType>>
    CreateInstance(
        std::unique_ptr<ClientConnectionParameters>
            client_connection_parameters,
        ConnectionPriority connection_priority,
        PendingConnectionRequestDelegate* delegate,
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) = 0;

   private:
    static Factory* test_factory_;
  };

  PendingBleInitiatorConnectionRequest(
      const PendingBleInitiatorConnectionRequest&) = delete;
  PendingBleInitiatorConnectionRequest& operator=(
      const PendingBleInitiatorConnectionRequest&) = delete;

  ~PendingBleInitiatorConnectionRequest() override;

 private:
  static const size_t kMaxEmptyScansPerDevice;
  static const size_t kMaxGattConnectionAttemptsPerDevice;

  PendingBleInitiatorConnectionRequest(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority,
      PendingConnectionRequestDelegate* delegate,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);

  // PendingConnectionRequest<BleInitiatorFailureType>:
  void HandleConnectionFailure(BleInitiatorFailureType failure_detail) override;

  size_t num_empty_scan_failures_ = 0u;
  size_t num_gatt_failures_ = 0;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_BLE_INITIATOR_CONNECTION_REQUEST_H_

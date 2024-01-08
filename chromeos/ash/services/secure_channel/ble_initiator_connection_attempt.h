// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_CONNECTION_ATTEMPT_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_CONNECTION_ATTEMPT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/secure_channel/ble_initiator_failure_type.h"
#include "chromeos/ash/services/secure_channel/connection_attempt_base.h"

namespace ash::secure_channel {

class BleConnectionManager;

// Attempts to connect to a remote device over BLE via the initiator role.
class BleInitiatorConnectionAttempt
    : public ConnectionAttemptBase<BleInitiatorFailureType> {
 public:
  class Factory {
   public:
    static std::unique_ptr<ConnectionAttempt<BleInitiatorFailureType>> Create(
        BleConnectionManager* ble_connection_manager,
        ConnectionAttemptDelegate* delegate,
        const ConnectionAttemptDetails& connection_attempt_details);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<ConnectionAttempt<BleInitiatorFailureType>>
    CreateInstance(
        BleConnectionManager* ble_connection_manager,
        ConnectionAttemptDelegate* delegate,
        const ConnectionAttemptDetails& connection_attempt_details) = 0;

   private:
    static Factory* test_factory_;
  };

  BleInitiatorConnectionAttempt(const BleInitiatorConnectionAttempt&) = delete;
  BleInitiatorConnectionAttempt& operator=(
      const BleInitiatorConnectionAttempt&) = delete;

  ~BleInitiatorConnectionAttempt() override;

 private:
  BleInitiatorConnectionAttempt(
      BleConnectionManager* ble_connection_manager,
      ConnectionAttemptDelegate* delegate,
      const ConnectionAttemptDetails& connection_attempt_details);

  std::unique_ptr<ConnectToDeviceOperation<BleInitiatorFailureType>>
  CreateConnectToDeviceOperation(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority,
      ConnectToDeviceOperation<
          BleInitiatorFailureType>::ConnectionSuccessCallback success_callback,
      const ConnectToDeviceOperation<BleInitiatorFailureType>::
          ConnectionFailedCallback& failure_callback) override;

  raw_ptr<BleConnectionManager> ble_connection_manager_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_CONNECTION_ATTEMPT_H_

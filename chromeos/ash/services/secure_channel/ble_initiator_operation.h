// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_OPERATION_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_OPERATION_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/services/secure_channel/ble_initiator_failure_type.h"
#include "chromeos/ash/services/secure_channel/connect_to_device_operation.h"
#include "chromeos/ash/services/secure_channel/connect_to_device_operation_base.h"

namespace ash::secure_channel {

class BleConnectionManager;
enum class ConnectionPriority;

// Attempts to connect to a remote device over BLE via the initiator role.
class BleInitiatorOperation
    : public ConnectToDeviceOperationBase<BleInitiatorFailureType> {
 public:
  class Factory {
   public:
    static std::unique_ptr<ConnectToDeviceOperation<BleInitiatorFailureType>>
    Create(BleConnectionManager* ble_connection_manager,
           ConnectToDeviceOperation<BleInitiatorFailureType>::
               ConnectionSuccessCallback success_callback,
           const ConnectToDeviceOperation<BleInitiatorFailureType>::
               ConnectionFailedCallback& failure_callback,
           const DeviceIdPair& device_id_pair,
           ConnectionPriority connection_priority,
           scoped_refptr<base::TaskRunner> task_runner =
               base::SingleThreadTaskRunner::GetCurrentDefault());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<ConnectToDeviceOperation<BleInitiatorFailureType>>
    CreateInstance(BleConnectionManager* ble_connection_manager,
                   ConnectToDeviceOperation<BleInitiatorFailureType>::
                       ConnectionSuccessCallback success_callback,
                   const ConnectToDeviceOperation<BleInitiatorFailureType>::
                       ConnectionFailedCallback& failure_callback,
                   const DeviceIdPair& device_id_pair,
                   ConnectionPriority connection_priority,
                   scoped_refptr<base::TaskRunner> task_runner) = 0;

   private:
    static Factory* test_factory_;
  };

  BleInitiatorOperation(const BleInitiatorOperation&) = delete;
  BleInitiatorOperation& operator=(const BleInitiatorOperation&) = delete;

  ~BleInitiatorOperation() override;

 private:
  BleInitiatorOperation(
      BleConnectionManager* ble_connection_manager,
      ConnectToDeviceOperation<
          BleInitiatorFailureType>::ConnectionSuccessCallback success_callback,
      const ConnectToDeviceOperation<
          BleInitiatorFailureType>::ConnectionFailedCallback& failure_callback,
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority,
      scoped_refptr<base::TaskRunner> task_runner);

  // ConnectToDeviceOperationBase<BleInitiatorFailureType>:
  void PerformAttemptConnectionToDevice(
      ConnectionPriority connection_priority) override;
  void PerformCancellation() override;
  void PerformUpdateConnectionPriority(
      ConnectionPriority connection_priority) override;

  void OnSuccessfulConnection(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel);
  void OnConnectionFailure(BleInitiatorFailureType failure_type);

  raw_ptr<BleConnectionManager> ble_connection_manager_;
  bool is_attempt_active_ = false;

  base::WeakPtrFactory<BleInitiatorOperation> weak_ptr_factory_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_OPERATION_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_OPERATION_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_OPERATION_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/services/secure_channel/ble_initiator_failure_type.h"
#include "chromeos/services/secure_channel/connect_to_device_operation.h"
#include "chromeos/services/secure_channel/connect_to_device_operation_base.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace chromeos {

namespace secure_channel {

class BleConnectionManager;

// Attempts to connect to a remote device over BLE via the initiator role.
class BleInitiatorOperation
    : public ConnectToDeviceOperationBase<BleInitiatorFailureType> {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<ConnectToDeviceOperation<BleInitiatorFailureType>>
    BuildInstance(BleConnectionManager* ble_connection_manager,
                  ConnectToDeviceOperation<BleInitiatorFailureType>::
                      ConnectionSuccessCallback success_callback,
                  const ConnectToDeviceOperation<BleInitiatorFailureType>::
                      ConnectionFailedCallback& failure_callback,
                  const DeviceIdPair& device_id_pair,
                  ConnectionPriority connection_priority,
                  scoped_refptr<base::TaskRunner> task_runner =
                      base::ThreadTaskRunnerHandle::Get());

   private:
    static Factory* test_factory_;
  };

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

  BleConnectionManager* ble_connection_manager_;
  bool is_attempt_active_ = false;

  base::WeakPtrFactory<BleInitiatorOperation> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BleInitiatorOperation);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_OPERATION_H_

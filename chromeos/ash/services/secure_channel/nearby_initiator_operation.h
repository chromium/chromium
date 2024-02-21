// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_INITIATOR_OPERATION_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_INITIATOR_OPERATION_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/services/secure_channel/connect_to_device_operation.h"
#include "chromeos/ash/services/secure_channel/connect_to_device_operation_base.h"
#include "chromeos/ash/services/secure_channel/nearby_connection_manager.h"
#include "chromeos/ash/services/secure_channel/nearby_initiator_failure_type.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"

namespace ash::secure_channel {

class AuthenticatedChannel;

// Attempts to connect to a remote device over Nearby Connections via the
// initiator role.
class NearbyInitiatorOperation
    : public ConnectToDeviceOperationBase<NearbyInitiatorFailureType> {
 public:
  class Factory {
   public:
    static std::unique_ptr<ConnectToDeviceOperation<NearbyInitiatorFailureType>>
    Create(NearbyConnectionManager* nearby_connection_manager,
           ConnectToDeviceOperation<NearbyInitiatorFailureType>::
               ConnectionSuccessCallback success_callback,
           const ConnectToDeviceOperation<NearbyInitiatorFailureType>::
               ConnectionFailedCallback& failure_callback,
           const NearbyConnectionManager::BleDiscoveryStateChangeCallback&
               ble_discovery_state_changed_callback,
           const NearbyConnectionManager::NearbyConnectionStateChangeCallback&
               nearby_connection_state_changed_callback,
           const NearbyConnectionManager::SecureChannelStateChangeCallback&
               secure_channel_authentication_state_changed_callback,
           const DeviceIdPair& device_id_pair,
           ConnectionPriority connection_priority,
           scoped_refptr<base::TaskRunner> task_runner =
               base::SingleThreadTaskRunner::GetCurrentDefault());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<
        ConnectToDeviceOperation<NearbyInitiatorFailureType>>
    CreateInstance(
        NearbyConnectionManager* nearby_connection_manager,
        ConnectToDeviceOperation<NearbyInitiatorFailureType>::
            ConnectionSuccessCallback success_callback,
        const ConnectToDeviceOperation<NearbyInitiatorFailureType>::
            ConnectionFailedCallback& failure_callback,
        const NearbyConnectionManager::BleDiscoveryStateChangeCallback&
            ble_discovery_state_changed_callback,
        const NearbyConnectionManager::NearbyConnectionStateChangeCallback&
            nearby_connection_state_changed_callback,
        const NearbyConnectionManager::SecureChannelStateChangeCallback&
            secure_channel_authentication_state_changed_callback,
        const DeviceIdPair& device_id_pair,
        ConnectionPriority connection_priority,
        scoped_refptr<base::TaskRunner> task_runner) = 0;

   private:
    static Factory* test_factory_;
  };

  NearbyInitiatorOperation(const NearbyInitiatorOperation&) = delete;
  NearbyInitiatorOperation& operator=(const NearbyInitiatorOperation&) = delete;
  ~NearbyInitiatorOperation() override;

 private:
  NearbyInitiatorOperation(
      NearbyConnectionManager* nearby_connection_manager,
      ConnectToDeviceOperation<NearbyInitiatorFailureType>::
          ConnectionSuccessCallback success_callback,
      const ConnectToDeviceOperation<NearbyInitiatorFailureType>::
          ConnectionFailedCallback& failure_callback,
      const NearbyConnectionManager::BleDiscoveryStateChangeCallback&
          ble_discovery_state_changed_callback,
      const NearbyConnectionManager::NearbyConnectionStateChangeCallback&
          nearby_connection_state_changed_callback,
      const NearbyConnectionManager::SecureChannelStateChangeCallback&
          secure_channel_authentication_state_changed_callback,
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority,
      scoped_refptr<base::TaskRunner> task_runner);

  // ConnectToDeviceOperationBase<NearbyInitiatorFailureType>:
  void PerformAttemptConnectionToDevice(
      ConnectionPriority connection_priority) override;
  void PerformCancellation() override;
  void PerformUpdateConnectionPriority(
      ConnectionPriority connection_priority) override;

  void OnSuccessfulConnection(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel);
  void OnConnectionFailure(NearbyInitiatorFailureType failure_type);

  void OnBleDiscoveryStateChanged(
      mojom::DiscoveryResult discovery_result,
      std::optional<mojom::DiscoveryErrorCode> potential_error_code);
  void OnNearbyConnectionStateChanged(mojom::NearbyConnectionStep step,
                                      mojom::NearbyConnectionStepResult result);
  void OnSecureChannelAuthenticationStateChanged(
      mojom::SecureChannelState secure_channel_state);

  raw_ptr<NearbyConnectionManager> nearby_connection_manager_;
  NearbyConnectionManager::BleDiscoveryStateChangeCallback
      ble_discovery_state_changed_callback_;
  NearbyConnectionManager::NearbyConnectionStateChangeCallback
      nearby_connection_state_changed_callback_;
  NearbyConnectionManager::SecureChannelStateChangeCallback
      secure_channel_authentication_state_changed_callback_;
  base::WeakPtrFactory<NearbyInitiatorOperation> weak_ptr_factory_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_INITIATOR_OPERATION_H_

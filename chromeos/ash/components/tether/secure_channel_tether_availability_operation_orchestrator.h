// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_SECURE_CHANNEL_TETHER_AVAILABILITY_OPERATION_ORCHESTRATOR_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_SECURE_CHANNEL_TETHER_AVAILABILITY_OPERATION_ORCHESTRATOR_H_

#include "chromeos/ash/components/tether/tether_availability_operation_orchestrator.h"
#include "chromeos/ash/components/tether/tether_host_fetcher.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"

namespace ash::tether {

class SecureChannelTetherAvailabilityOperationOrchestrator
    : public TetherAvailabilityOperationOrchestrator {
 public:
  class Factory : public TetherAvailabilityOperationOrchestrator::Factory {
   public:
    Factory(raw_ptr<TetherHostFetcher> tether_host_fetcher,
            raw_ptr<device_sync::DeviceSyncClient> device_sync_client,
            raw_ptr<HostConnection::Factory> host_connection_factory,
            raw_ptr<TetherHostResponseRecorder> tether_host_response_recorder,
            raw_ptr<ConnectionPreserver> connection_preserver);

    ~Factory() override;

    // TetherAvailabilityOperationOrchestrator::Factory:
    std::unique_ptr<TetherAvailabilityOperationOrchestrator> CreateInstance()
        override;

   private:
    raw_ptr<device_sync::DeviceSyncClient> device_sync_client_;
    raw_ptr<TetherHostResponseRecorder> tether_host_response_recorder_;
    raw_ptr<HostConnection::Factory> host_connection_factory_;
    raw_ptr<ConnectionPreserver> connection_preserver_;
    raw_ptr<TetherHostFetcher> tether_host_fetcher_;
  };

  SecureChannelTetherAvailabilityOperationOrchestrator(
      std::unique_ptr<TetherAvailabilityOperation::Initializer>
          tether_availability_operation_initializer,
      raw_ptr<TetherHostFetcher> tether_host_fetcher);
  ~SecureChannelTetherAvailabilityOperationOrchestrator() override;
  SecureChannelTetherAvailabilityOperationOrchestrator(
      const SecureChannelTetherAvailabilityOperationOrchestrator&) = delete;
  SecureChannelTetherAvailabilityOperationOrchestrator& operator=(
      const SecureChannelTetherAvailabilityOperationOrchestrator&) = delete;

  // TetherAvailabilityOperationOrchestrator:
  void Start() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(
      SecureChannelTetherAvailabilityOperationOrchestratorTest,
      HostFetcher_WillFetchAllDevices);

  raw_ptr<TetherHostFetcher> tether_host_fetcher_;
  base::WeakPtrFactory<SecureChannelTetherAvailabilityOperationOrchestrator>
      weak_ptr_factory_{this};
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_SECURE_CHANNEL_TETHER_AVAILABILITY_OPERATION_ORCHESTRATOR_H_

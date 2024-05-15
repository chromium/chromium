
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/secure_channel_tether_availability_operation_orchestrator.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash::tether {

SecureChannelTetherAvailabilityOperationOrchestrator::Factory::Factory(
    raw_ptr<TetherHostFetcher> tether_host_fetcher,
    raw_ptr<device_sync::DeviceSyncClient> device_sync_client,
    raw_ptr<HostConnection::Factory> host_connection_factory,
    raw_ptr<TetherHostResponseRecorder> tether_host_response_recorder,
    raw_ptr<ConnectionPreserver> connection_preserver)
    : device_sync_client_(device_sync_client),
      tether_host_response_recorder_(tether_host_response_recorder),
      host_connection_factory_(host_connection_factory),
      connection_preserver_(connection_preserver),
      tether_host_fetcher_(tether_host_fetcher) {}

SecureChannelTetherAvailabilityOperationOrchestrator::Factory::~Factory() =
    default;

std::unique_ptr<TetherAvailabilityOperationOrchestrator>
SecureChannelTetherAvailabilityOperationOrchestrator::Factory::
    CreateInstance() {
  return std::make_unique<SecureChannelTetherAvailabilityOperationOrchestrator>(
      std::make_unique<TetherAvailabilityOperation::Initializer>(
          host_connection_factory_, tether_host_response_recorder_,
          connection_preserver_),
      tether_host_fetcher_);
}

SecureChannelTetherAvailabilityOperationOrchestrator::
    SecureChannelTetherAvailabilityOperationOrchestrator(
        std::unique_ptr<TetherAvailabilityOperation::Initializer>
            tether_availability_operation_initializer,
        raw_ptr<TetherHostFetcher> tether_host_fetcher)
    : TetherAvailabilityOperationOrchestrator(
          std::move(tether_availability_operation_initializer)),
      tether_host_fetcher_(tether_host_fetcher) {}

SecureChannelTetherAvailabilityOperationOrchestrator::
    ~SecureChannelTetherAvailabilityOperationOrchestrator() = default;

void SecureChannelTetherAvailabilityOperationOrchestrator::Start() {
  PA_LOG(VERBOSE) << "Fetching Tether host.";
  std::optional<multidevice::RemoteDeviceRef> tether_host =
      tether_host_fetcher_->GetTetherHost();
  if (!tether_host) {
    PA_LOG(WARNING) << "Could not start host scan. No tether hosts available.";
    NotifyObserversOfFinalScan();
    return;
  }

  StartOperation(TetherHost(*tether_host));
}
}  // namespace ash::tether

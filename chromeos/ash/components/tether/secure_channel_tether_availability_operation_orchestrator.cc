
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/secure_channel_tether_availability_operation_orchestrator.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash::tether {

SecureChannelTetherAvailabilityOperationOrchestrator::Factory::Factory(
    raw_ptr<TetherHostFetcher> tether_host_fetcher)
    : tether_host_fetcher_(tether_host_fetcher) {}

SecureChannelTetherAvailabilityOperationOrchestrator::Factory::~Factory() {}

std::unique_ptr<TetherAvailabilityOperationOrchestrator>
SecureChannelTetherAvailabilityOperationOrchestrator::Factory::
    CreateInstance() {
  return std::make_unique<SecureChannelTetherAvailabilityOperationOrchestrator>(
      tether_host_fetcher_);
}

SecureChannelTetherAvailabilityOperationOrchestrator::
    SecureChannelTetherAvailabilityOperationOrchestrator(
        raw_ptr<TetherHostFetcher> tether_host_fetcher)
    : tether_host_fetcher_(tether_host_fetcher) {}

SecureChannelTetherAvailabilityOperationOrchestrator::
    ~SecureChannelTetherAvailabilityOperationOrchestrator() = default;

void SecureChannelTetherAvailabilityOperationOrchestrator::Start() {
  PA_LOG(VERBOSE) << "Fetching potential Tether hosts.";
  tether_host_fetcher_->FetchAllTetherHosts(
      base::BindOnce(&SecureChannelTetherAvailabilityOperationOrchestrator::
                         OnTetherHostsFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SecureChannelTetherAvailabilityOperationOrchestrator::OnTetherHostsFetched(
    const multidevice::RemoteDeviceRefList& tether_hosts) {
  fetched_tether_hosts_ = tether_hosts;
  if (fetched_tether_hosts_.empty()) {
    PA_LOG(WARNING) << "Could not start host scan. No tether hosts available.";
    NotifyObserversOfFinalScan();
    return;
  }

  // TODO: Complete this method once operations are fully implemented.
}

}  // namespace ash::tether

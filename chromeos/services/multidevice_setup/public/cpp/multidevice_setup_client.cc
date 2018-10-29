// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

namespace chromeos {

namespace multidevice_setup {

// static
MultiDeviceSetupClient::HostStatusWithDevice
MultiDeviceSetupClient::GenerateDefaultHostStatusWithDevice() {
  return HostStatusWithDevice{mojom::HostStatus::kNoEligibleHosts,
                              base::nullopt /* host_device */};
}

// static
MultiDeviceSetupClient::FeatureStatesMap
MultiDeviceSetupClient::GenerateDefaultFeatureStatesMap() {
  return FeatureStatesMap{
      {mojom::Feature::kBetterTogetherSuite,
       mojom::FeatureState::kProhibitedByPolicy},
      {mojom::Feature::kInstantTethering,
       mojom::FeatureState::kProhibitedByPolicy},
      {mojom::Feature::kMessages, mojom::FeatureState::kProhibitedByPolicy},
      {mojom::Feature::kSmartLock, mojom::FeatureState::kProhibitedByPolicy}};
}

MultiDeviceSetupClient::MultiDeviceSetupClient() = default;

MultiDeviceSetupClient::~MultiDeviceSetupClient() = default;

void MultiDeviceSetupClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void MultiDeviceSetupClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

mojom::FeatureState MultiDeviceSetupClient::GetFeatureState(
    mojom::Feature feature) const {
  return GetFeatureStates().find(feature)->second;
}

void MultiDeviceSetupClient::NotifyHostStatusChanged(
    const HostStatusWithDevice& host_status_with_device) {
  for (auto& observer : observer_list_)
    observer.OnHostStatusChanged(host_status_with_device);
}

void MultiDeviceSetupClient::NotifyFeatureStateChanged(
    const FeatureStatesMap& feature_states_map) {
  for (auto& observer : observer_list_)
    observer.OnFeatureStatesChanged(feature_states_map);
}

}  // namespace multidevice_setup

}  // namespace chromeos

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

#include "base/check.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash {

namespace multidevice_setup {

namespace {

std::string HostStatusToString(mojom::HostStatus status) {
  switch (status) {
    case mojom::HostStatus::kNoEligibleHosts:
      return "[kNoEligibleHosts]";
    case mojom::HostStatus::kEligibleHostExistsButNoHostSet:
      return "[kEligibleHostExistsButNoHostSet]";
    case mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation:
      return "[kHostSetLocallyButWaitingForBackendConfirmation]";
    case mojom::HostStatus::kHostSetButNotYetVerified:
      return "[kHostSetButNotYetVerified]";
    case mojom::HostStatus::kHostVerified:
      return "[kHostVerified]";
  }
}

}  // namespace

// static
MultiDeviceSetupClient::HostStatusWithDevice
MultiDeviceSetupClient::GenerateDefaultHostStatusWithDevice() {
  return HostStatusWithDevice{mojom::HostStatus::kNoEligibleHosts,
                              std::nullopt /* host_device */};
}

// static
MultiDeviceSetupClient::FeatureStatesMap
MultiDeviceSetupClient::GenerateDefaultFeatureStatesMap(
    mojom::FeatureState default_value) {
  // The default feature state map can be either kProhibitedByPolicy or
  // kUnavailableNoVerifiedHost_ClientNotReady. There are two options for the
  // default. The first is kProhibitedByPolicy for the MultideviceHandler
  // because if the MultideviceSetupClient is null, then the feature suite is
  // prohibited, since the MultideviceSetupClient is only created when the
  // Multidevice suite is allowed. The second applies to situations otherwise
  // where the default is kUnavailableNoVerifiedHost_ClientNotReady while we
  // wait for device sync.
  DCHECK(default_value ==
             mojom::FeatureState::kUnavailableNoVerifiedHost_ClientNotReady ||
         default_value == mojom::FeatureState::kProhibitedByPolicy);
  MultiDeviceSetupClient::FeatureStatesMap map =
      MultiDeviceSetupClient::FeatureStatesMap{
          {mojom::Feature::kBetterTogetherSuite, default_value},
          {mojom::Feature::kInstantTethering, default_value},
          {mojom::Feature::kSmartLock, default_value},
          {mojom::Feature::kPhoneHub, default_value},
          {mojom::Feature::kPhoneHubNotifications, default_value},
          {mojom::Feature::kPhoneHubTaskContinuation, default_value},
          {mojom::Feature::kWifiSync, default_value},
          {mojom::Feature::kEche, default_value},
          {mojom::Feature::kPhoneHubCameraRoll, default_value}};

  DCHECK(map.size() == static_cast<int32_t>(mojom::Feature::kMaxValue));
  return map;
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

std::string FeatureStatesMapToString(
    const MultiDeviceSetupClient::FeatureStatesMap& map) {
  std::ostringstream stream;
  stream << "{" << std::endl;
  for (const auto& item : map)
    stream << "  " << item.first << ": " << item.second << "," << std::endl;
  stream << "}";
  return stream.str();
}

std::string HostStatusWithDeviceToString(
    const MultiDeviceSetupClient::HostStatusWithDevice&
        host_status_with_device) {
  std::ostringstream stream;
  stream << "{" << std::endl;
  stream << "  " << HostStatusToString(host_status_with_device.first) << ": "
         << (host_status_with_device.second
                 ? host_status_with_device.second->pii_free_name()
                 : " no device")
         << std::endl;
  stream << "}";
  return stream.str();
}

}  // namespace multidevice_setup

}  // namespace ash

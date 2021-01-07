// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/eligible_host_devices_provider_impl.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/constants/chromeos_features.h"

namespace chromeos {

namespace multidevice_setup {

// static
constexpr base::TimeDelta
    EligibleHostDevicesProviderImpl::kInactiveDeviceThresholdInDays;

// static
EligibleHostDevicesProviderImpl::Factory*
    EligibleHostDevicesProviderImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<EligibleHostDevicesProvider>
EligibleHostDevicesProviderImpl::Factory::Create(
    device_sync::DeviceSyncClient* device_sync_client) {
  if (test_factory_)
    return test_factory_->CreateInstance(device_sync_client);

  return base::WrapUnique(
      new EligibleHostDevicesProviderImpl(device_sync_client));
}

// static
void EligibleHostDevicesProviderImpl::Factory::SetFactoryForTesting(
    Factory* factory) {
  test_factory_ = factory;
}

EligibleHostDevicesProviderImpl::Factory::~Factory() = default;

EligibleHostDevicesProviderImpl::EligibleHostDevicesProviderImpl(
    device_sync::DeviceSyncClient* device_sync_client)
    : device_sync_client_(device_sync_client) {
  device_sync_client_->AddObserver(this);
  UpdateEligibleDevicesSet();
}

EligibleHostDevicesProviderImpl::~EligibleHostDevicesProviderImpl() {
  device_sync_client_->RemoveObserver(this);
}

multidevice::RemoteDeviceRefList
EligibleHostDevicesProviderImpl::GetEligibleHostDevices() const {
  return eligible_devices_from_last_sync_;
}

multidevice::DeviceWithConnectivityStatusList
EligibleHostDevicesProviderImpl::GetEligibleActiveHostDevices() const {
  return eligible_active_devices_from_last_sync_;
}

void EligibleHostDevicesProviderImpl::OnNewDevicesSynced() {
  UpdateEligibleDevicesSet();
}

void EligibleHostDevicesProviderImpl::UpdateEligibleDevicesSet() {
  eligible_devices_from_last_sync_.clear();
  for (const auto& remote_device : device_sync_client_->GetSyncedDevices()) {
    multidevice::SoftwareFeatureState host_state =
        remote_device.GetSoftwareFeatureState(
            multidevice::SoftwareFeature::kBetterTogetherHost);
    if (host_state == multidevice::SoftwareFeatureState::kSupported ||
        host_state == multidevice::SoftwareFeatureState::kEnabled) {
      eligible_devices_from_last_sync_.push_back(remote_device);
    }
  }

  // Sort from most-recently-updated to least-recently-updated. The timestamp
  // used is provided by the back-end and indicates the last time at which the
  // device's metadata was updated on the server. Note that this does not
  // provide us with the last time that a user actually used this device, but it
  // is a good estimate.
  std::sort(eligible_devices_from_last_sync_.begin(),
            eligible_devices_from_last_sync_.end(),
            [](const auto& first_device, const auto& second_device) {
              return first_device.last_update_time_millis() >
                     second_device.last_update_time_millis();
            });

  eligible_active_devices_from_last_sync_.clear();
  for (const auto& remote_device : eligible_devices_from_last_sync_) {
    eligible_active_devices_from_last_sync_.push_back(
        multidevice::DeviceWithConnectivityStatus(
            remote_device,
            cryptauthv2::ConnectivityStatus::UNKNOWN_CONNECTIVITY));
  }

  if (base::FeatureList::IsEnabled(
          features::kCryptAuthV2DeviceActivityStatus)) {
    device_sync_client_->GetDevicesActivityStatus(base::BindOnce(
        &EligibleHostDevicesProviderImpl::OnGetDevicesActivityStatus,
        base::Unretained(this)));
  }
}

void EligibleHostDevicesProviderImpl::OnGetDevicesActivityStatus(
    device_sync::mojom::NetworkRequestResult network_result,
    base::Optional<std::vector<device_sync::mojom::DeviceActivityStatusPtr>>
        devices_activity_status_optional) {
  if (network_result != device_sync::mojom::NetworkRequestResult::kSuccess ||
      !devices_activity_status_optional) {
    return;
  }

  base::flat_map<std::string, device_sync::mojom::DeviceActivityStatusPtr>
      id_to_activity_status_map;
  for (device_sync::mojom::DeviceActivityStatusPtr& device_activity_status_ptr :
       *devices_activity_status_optional) {
    id_to_activity_status_map.insert({device_activity_status_ptr->device_id,
                                      std::move(device_activity_status_ptr)});
  }

  // Remove inactive devices. A device is inactive if it has a
  // last_activity_time before some defined threshold.
  base::Time now = base::Time::Now();
  eligible_active_devices_from_last_sync_.erase(
      std::remove_if(
          eligible_active_devices_from_last_sync_.begin(),
          eligible_active_devices_from_last_sync_.end(),
          [&id_to_activity_status_map,
           &now](const multidevice::DeviceWithConnectivityStatus& device) {
            auto it = id_to_activity_status_map.find(
                device.remote_device.instance_id());

            if (it == id_to_activity_status_map.end()) {
              return false;
            }

            base::Time last_activity_time =
                std::get<1>(*it)->last_activity_time;

            // Do not filter out devices whose last activity time was not set by
            // the server.
            if (last_activity_time == base::Time()) {
              return false;
            }

            return now - last_activity_time > kInactiveDeviceThresholdInDays;
          }),
      eligible_active_devices_from_last_sync_.end());

  // Sort the list preferring online devices, then last activity time, then
  // last update time.
  std::sort(
      eligible_active_devices_from_last_sync_.begin(),
      eligible_active_devices_from_last_sync_.end(),
      [&id_to_activity_status_map](const auto& first_device,
                                   const auto& second_device) {
        auto it1 = id_to_activity_status_map.find(
            first_device.remote_device.instance_id());
        auto it2 = id_to_activity_status_map.find(
            second_device.remote_device.instance_id());
        if (it1 == id_to_activity_status_map.end() &&
            it2 == id_to_activity_status_map.end()) {
          return first_device.remote_device.last_update_time_millis() >
                 second_device.remote_device.last_update_time_millis();
        }

        if (it1 == id_to_activity_status_map.end()) {
          return false;
        }

        if (it2 == id_to_activity_status_map.end()) {
          return true;
        }

        const device_sync::mojom::DeviceActivityStatusPtr&
            first_activity_status = std::get<1>(*it1);
        const device_sync::mojom::DeviceActivityStatusPtr&
            second_activity_status = std::get<1>(*it2);

        if (base::FeatureList::IsEnabled(
                features::kCryptAuthV2DeviceActivityStatusUseConnectivity)) {
          if (first_activity_status->connectivity_status ==
                  cryptauthv2::ConnectivityStatus::ONLINE &&
              second_activity_status->connectivity_status !=
                  cryptauthv2::ConnectivityStatus::ONLINE) {
            return true;
          }

          if (second_activity_status->connectivity_status ==
                  cryptauthv2::ConnectivityStatus::ONLINE &&
              first_activity_status->connectivity_status !=
                  cryptauthv2::ConnectivityStatus::ONLINE) {
            return false;
          }
        }

        if (first_activity_status->last_activity_time !=
            second_activity_status->last_activity_time) {
          return first_activity_status->last_activity_time >
                 second_activity_status->last_activity_time;
        }

        return first_device.remote_device.last_update_time_millis() >
               second_device.remote_device.last_update_time_millis();
      });

  if (base::FeatureList::IsEnabled(
          features::kCryptAuthV2DeviceActivityStatusUseConnectivity)) {
    for (auto& host_device : eligible_active_devices_from_last_sync_) {
      auto it = id_to_activity_status_map.find(
          host_device.remote_device.instance_id());
      if (it == id_to_activity_status_map.end()) {
        continue;
      }
      host_device.connectivity_status = std::get<1>(*it)->connectivity_status;
    }
  }
}

}  // namespace multidevice_setup

}  // namespace chromeos

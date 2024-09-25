// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/eligible_host_devices_provider_impl.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"

namespace ash {

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
  // This is equivalent to GetEligibleActiveHostDevices() without
  // the connectivity data.
  // TODO(https://crbug.com/1229876): Consolidate GetEligibleHostDevices() and
  // GetEligibleActiveHostDevices().
  multidevice::RemoteDeviceRefList eligible_active_devices;
  for (const auto& device : eligible_active_devices_from_last_sync_) {
    if (device.remote_device.instance_id().empty() &&
        device.remote_device.GetDeviceId().empty()) {
      // TODO(b/207089877): Add a metric to capture the frequency of missing
      // device id.
      PA_LOG(WARNING) << __func__
                      << ": encountered device with missing Instance ID and "
                         "legacy device ID";
      continue;
    }
    eligible_active_devices.push_back(device.remote_device);
  }

  return eligible_active_devices;
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
    if (remote_device.instance_id().empty() &&
        remote_device.GetDeviceId().empty()) {
      // TODO(b/207089877): Add a metric to capture the frequency of missing
      // device id.
      PA_LOG(WARNING) << __func__
                      << ": encountered device with missing Instance ID and "
                         "legacy device ID";
      continue;
    }

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
  } else {
    NotifyObserversEligibleDevicesSynced();
  }
}

void EligibleHostDevicesProviderImpl::OnGetDevicesActivityStatus(
    device_sync::mojom::NetworkRequestResult network_result,
    std::optional<std::vector<device_sync::mojom::DeviceActivityStatusPtr>>
        devices_activity_status_optional) {
  if (network_result != device_sync::mojom::NetworkRequestResult::kSuccess ||
      !devices_activity_status_optional) {
    NotifyObserversEligibleDevicesSynced();
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
  // last_activity_time or last_update_time before some defined threshold.
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

            // Note: Do not filter out devices if the last activity time was not
            // set by the server, as indicated by a trivial base::Time value.
            base::Time last_activity_time =
                std::get<1>(*it)->last_activity_time;
            if (!last_activity_time.is_null() &&
                now - last_activity_time > kInactiveDeviceThresholdInDays) {
              return true;
            }

            // Note: Do not filter out devices if the last update time was not
            // set by the server, as indicated by a trivial base::Time value.
            // Note: This |last_update_time| is from GetDevicesActivityStatus,
            // not from the RemoteDevice; they track different events.
            base::Time last_update_time = std::get<1>(*it)->last_update_time;
            return !last_update_time.is_null() &&
                   now - last_update_time > kInactiveDeviceThresholdInDays;
          }),
      eligible_active_devices_from_last_sync_.end());

  // Sort the list preferring online devices (if flag is enabled), then last
  // activity time, then the last time the device enrolled or uploaded encrypted
  // metadata to the CryptAuth server (GetDevicesActivityStatus's
  // |last_update_time|), then last time a feature bit was flipped for the
  // device on the CryptAuth server (RemoteDevice's |last_update_time_millis|).
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

        // Note: This |last_update_time| is from GetDevicesActivityStatus, not
        // from the RemoteDevice; they track different events.
        if (first_activity_status->last_update_time !=
            second_activity_status->last_update_time) {
          return first_activity_status->last_update_time >
                 second_activity_status->last_update_time;
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

  // Remove devices with the same non-trivial |last_activity_time|, keeping only
  // the device with the most recent |last_update_time| or
  // |last_update_time_millis|. Note: |eligible_active_devices_from_last_sync_|
  // is already sorted in the preferred order.
  if (base::FeatureList::IsEnabled(
          features::kCryptAuthV2DedupDeviceLastActivityTime)) {
    base::flat_set<base::Time> set_of_same_last_activity_time;
    eligible_active_devices_from_last_sync_.erase(
        std::remove_if(
            eligible_active_devices_from_last_sync_.begin(),
            eligible_active_devices_from_last_sync_.end(),
            [&id_to_activity_status_map, &set_of_same_last_activity_time](
                const multidevice::DeviceWithConnectivityStatus& device) {
              auto it = id_to_activity_status_map.find(
                  device.remote_device.instance_id());

              if (it == id_to_activity_status_map.end()) {
                return false;
              }

              base::Time last_activity_time =
                  std::get<1>(*it)->last_activity_time;

              // Do not filter out devices if the last activity time was not set
              // by the server, as indicated by a trivial base::Time value.
              if (last_activity_time.is_null()) {
                return false;
              }

              if (set_of_same_last_activity_time.contains(last_activity_time)) {
                return true;
              }

              set_of_same_last_activity_time.insert(last_activity_time);
              return false;
            }),
        eligible_active_devices_from_last_sync_.end());
  }

  // Remove devices that have duplicate `bluetooth_public_addresses`, which
  // indicate they are the same device. Filter after the other sorting happens
  // to keep the most recent version of the phone when there's duplicates
  base::flat_set<std::string> set_of_same_public_bluetooth_address;
  eligible_active_devices_from_last_sync_.erase(
      std::remove_if(
          eligible_active_devices_from_last_sync_.begin(),
          eligible_active_devices_from_last_sync_.end(),
          [&set_of_same_public_bluetooth_address](
              const multidevice::DeviceWithConnectivityStatus& device) {
            const std::string& bluetooth_public_address =
                device.remote_device.bluetooth_public_address();

            // Do not filter out devices if the `bluetooth_public_address`
            // is not available.
            if (bluetooth_public_address.empty()) {
              return false;
            }

            if (set_of_same_public_bluetooth_address.contains(
                    bluetooth_public_address)) {
              return true;
            }

            set_of_same_public_bluetooth_address.insert(
                bluetooth_public_address);
            return false;
          }),
      eligible_active_devices_from_last_sync_.end());

  NotifyObserversEligibleDevicesSynced();
}

}  // namespace multidevice_setup

}  // namespace ash

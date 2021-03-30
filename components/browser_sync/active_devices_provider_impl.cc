// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"
#include "components/browser_sync/active_devices_provider_impl.h"
#include "components/browser_sync/browser_sync_switches.h"

namespace browser_sync {

ActiveDevicesProviderImpl::ActiveDevicesProviderImpl(
    syncer::DeviceInfoTracker* device_info_tracker,
    base::Clock* clock)
    : device_info_tracker_(device_info_tracker), clock_(clock) {
  DCHECK(device_info_tracker_);
  device_info_tracker_->AddObserver(this);
}

ActiveDevicesProviderImpl::~ActiveDevicesProviderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_.is_null());
  device_info_tracker_->RemoveObserver(this);
}

size_t ActiveDevicesProviderImpl::CountActiveDevicesIfAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetActiveDevices().size();
}

std::vector<std::string>
ActiveDevicesProviderImpl::CollectFCMRegistrationTokensForInvalidations(
    const std::string& local_cache_guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::string> fcm_registration_tokens;
  if (!base::FeatureList::IsEnabled(
          switches::kSyncUseFCMRegistrationTokensList)) {
    return fcm_registration_tokens;
  }

  for (const std::unique_ptr<syncer::DeviceInfo>& device : GetActiveDevices()) {
    if (!local_cache_guid.empty() && device->guid() == local_cache_guid) {
      continue;
    }
    if (device->fcm_registration_token().empty()) {
      continue;
    }

    fcm_registration_tokens.push_back(device->fcm_registration_token());
  }

  // Do not send tokens if the list of active devices is huge. This is similar
  // to the case when the client doesn't know about other devices, so return an
  // empty list. Otherwise the client would return only a part of all active
  // clients and other clients might miss an invalidation.
  if (fcm_registration_tokens.size() >
      static_cast<size_t>(
          switches::kSyncFCMRegistrationTokensListMaxSize.Get())) {
    return std::vector<std::string>();
  }

  return fcm_registration_tokens;
}

void ActiveDevicesProviderImpl::SetActiveDevicesChangedCallback(
    ActiveDevicesChangedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The |callback_| must not be replaced with another non-null |callback|.
  DCHECK(callback_.is_null() || callback.is_null());
  callback_ = std::move(callback);
}

void ActiveDevicesProviderImpl::OnDeviceInfoChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (callback_) {
    callback_.Run();
  }
}

std::vector<std::unique_ptr<syncer::DeviceInfo>>
ActiveDevicesProviderImpl::GetActiveDevices() const {
  std::vector<std::unique_ptr<syncer::DeviceInfo>> all_devices =
      device_info_tracker_->GetAllDeviceInfo();
  if (!base::FeatureList::IsEnabled(
          switches::kSyncFilterOutInactiveDevicesForSingleClient)) {
    return all_devices;
  }

  base::EraseIf(
      all_devices, [this](const std::unique_ptr<syncer::DeviceInfo>& device) {
        const base::Time expected_expiration_time =
            device->last_updated_timestamp() + device->pulse_interval() +
            switches::kSyncActiveDeviceMargin.Get();
        // If the device's expiration time hasn't been reached, then
        // it is considered active device.
        return expected_expiration_time <= clock_->Now();
      });

  return all_devices;
}

}  // namespace browser_sync

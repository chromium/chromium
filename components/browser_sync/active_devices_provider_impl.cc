// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/browser_sync/active_devices_provider_impl.h"

namespace browser_sync {

// Enables filtering out inactive devices which haven't sent DeviceInfo update
// recently (depending on the device's pulse_interval and an additional margin).
const base::Feature kSyncFilterOutInactiveDevicesForSingleClient{
    "SyncFilterOutInactiveDevicesForSingleClient",
    base::FEATURE_ENABLED_BY_DEFAULT};

// An additional threshold to consider devices as active. It extends device's
// pulse interval to mitigate possible latency after DeviceInfo commit.
const base::FeatureParam<base::TimeDelta> kSyncActiveDeviceMargin{
    &kSyncFilterOutInactiveDevicesForSingleClient, "SyncActiveDeviceMargin",
    base::TimeDelta::FromMinutes(30)};

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
  std::vector<std::unique_ptr<syncer::DeviceInfo>> all_devices =
      device_info_tracker_->GetAllDeviceInfo();
  if (!base::FeatureList::IsEnabled(
          kSyncFilterOutInactiveDevicesForSingleClient)) {
    return all_devices.size();
  }

  size_t active_devices = 0;
  for (const auto& device : all_devices) {
    const base::Time expected_expiration_time =
        device->last_updated_timestamp() + device->pulse_interval() +
        kSyncActiveDeviceMargin.Get();
    // If the device's expiration time hasn't been reached, then it is
    // considered active device.
    if (expected_expiration_time > clock_->Now()) {
      active_devices++;
    }
  }
  return active_devices;
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

}  // namespace browser_sync

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/active_devices_provider_impl.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/sync/base/data_type.h"

namespace browser_sync {

ActiveDevicesProviderImpl::ActiveDevicesProviderImpl(
    syncer::DeviceInfoTracker* device_info_tracker,
    base::Clock* clock)
    : device_info_tracker_(device_info_tracker), clock_(clock) {
  DCHECK(device_info_tracker_);
  device_info_tracker_observation_.Observe(device_info_tracker_);
}

ActiveDevicesProviderImpl::~ActiveDevicesProviderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_.is_null());
}

syncer::ActiveDevicesInvalidationInfo
ActiveDevicesProviderImpl::CalculateInvalidationInfo(
    const std::string& local_cache_guid) const {
  TRACE_EVENT0("ui", "ActiveDevicesProviderImpl::CalculateInvalidationInfo");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::vector<const syncer::DeviceInfo*> active_devices =
      GetActiveDevicesSortedByUpdateTime();
  if (active_devices.empty()) {
    // This may happen if the engine is not initialized yet. In other cases,
    // |active_devices| must contain at least the local device.
    return syncer::ActiveDevicesInvalidationInfo::CreateUninitialized();
  }

  std::vector<std::string> all_fcm_registration_tokens;

  // List of interested data types for all other clients.
  syncer::DataTypeSet all_interested_data_types;

  syncer::DataTypeSet old_invalidations_interested_data_types;

  // FCM registration tokens with corresponding interested data types for all
  // the clients with enabled sync standalone invalidations.
  std::map<std::string, syncer::DataTypeSet>
      fcm_token_and_interested_data_types;

  for (const syncer::DeviceInfo* device : active_devices) {
    if (!local_cache_guid.empty() && device->guid() == local_cache_guid) {
      continue;
    }

    all_interested_data_types.PutAll(device->interested_data_types());

    if (!device->fcm_registration_token().empty()) {
      // If there is a duplicate FCM registration token, use the latest one. To
      // achieve this, rely on sorted |active_devices| by update time. Two
      // DeviceInfo entities can have the same FCM registration token if the
      // sync engine was reset without signout.
      fcm_token_and_interested_data_types[device->fcm_registration_token()] =
          device->interested_data_types();
      if (base::FeatureList::IsEnabled(
              switches::kSyncUseFCMRegistrationTokensList)) {
        all_fcm_registration_tokens.push_back(device->fcm_registration_token());
      }
    } else if (!device->interested_data_types().empty()) {
      // An empty FCM registration token may be set for old clients, and for
      // modern clients supporting sync standalone invalidatoins if there was an
      // error during FCM registration. This does not matter in this case since
      // the error case should be rare, and in the worst case the
      // |single_client_old_invalidations| flag will not be provided (and this
      // is just an optimization flag).
      old_invalidations_interested_data_types.PutAll(
          device->interested_data_types());
    } else {
      // For old clients which do not support interested data types assume that
      // they are subscribed to all data types.
      old_invalidations_interested_data_types.PutAll(syncer::ProtocolTypes());
    }
  }

  // Do not send tokens if the list of active devices is huge. This is similar
  // to the case when the client doesn't know about other devices, so return an
  // empty list. Otherwise the client would return only a part of all active
  // clients and other clients might miss an invalidation.
  if (all_fcm_registration_tokens.size() >
      static_cast<size_t>(
          switches::kSyncFCMRegistrationTokensListMaxSize.Get())) {
    all_fcm_registration_tokens.clear();
  }
  TRACE_EVENT0("ui",
               "ActiveDevicesProviderImpl::CalculateInvalidationInfo() => "
               "ActiveDevicesInvalidationInfo::Create");

  return syncer::ActiveDevicesInvalidationInfo::Create(
      std::move(all_fcm_registration_tokens), all_interested_data_types,
      std::move(fcm_token_and_interested_data_types),
      old_invalidations_interested_data_types);
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

std::vector<const syncer::DeviceInfo*>
ActiveDevicesProviderImpl::GetActiveDevicesSortedByUpdateTime() const {
  std::vector<const syncer::DeviceInfo*> all_devices =
      device_info_tracker_->GetAllDeviceInfo();

  if (base::FeatureList::IsEnabled(
          switches::kSyncFilterOutInactiveDevicesForSingleClient)) {
    std::erase_if(all_devices, [this](const syncer::DeviceInfo* device) {
      const base::Time expected_expiration_time =
          device->last_updated_timestamp() + device->pulse_interval() +
          switches::kSyncActiveDeviceMargin.Get();
      // If the device's expiration time hasn't been reached, then
      // it is considered active device.
      return expected_expiration_time <= clock_->Now();
    });
  }

  base::ranges::sort(all_devices, [](const syncer::DeviceInfo* left_device,
                                     const syncer::DeviceInfo* right_device) {
    return left_device->last_updated_timestamp() <
           right_device->last_updated_timestamp();
  });

  return all_devices;
}

}  // namespace browser_sync

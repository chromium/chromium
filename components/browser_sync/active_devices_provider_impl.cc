// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/active_devices_provider_impl.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/active_devices_invalidation_info.h"

namespace browser_sync {

// Max size of FCM registration tokens list used for sync invalidation
// optimization. If the number of active devices having FCM registration tokens
// is higher, then the resulting list will be empty meaning unknown FCM
// registration tokens.
constexpr size_t kSyncFCMRegistrationTokensListMaxSize = 5;

// An additional threshold to consider devices as active. It extends device's
// pulse interval to mitigate possible latency after DeviceInfo commit.
constexpr base::TimeDelta kSyncActiveDeviceMargin = base::Days(7);

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

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSyncInvalidationOptimizations)) {
    return syncer::ActiveDevicesInvalidationInfo::CreateUninitialized();
  }

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
      all_fcm_registration_tokens.push_back(device->fcm_registration_token());
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
      kSyncFCMRegistrationTokensListMaxSize) {
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
  std::vector<const syncer::DeviceInfo*> device_infos =
      device_info_tracker_->GetAllDeviceInfo();

  std::erase_if(device_infos, [this](const syncer::DeviceInfo* device) {
    const base::Time expected_expiration_time =
        device->last_updated_timestamp() + device->pulse_interval() +
        kSyncActiveDeviceMargin;
    // If the device's expiration time hasn't been reached, then it is
    // considered active device. Devices without chrome version are always
    // considered active. Note that all devices still have 56 days expiration
    // time (see DeviceInfoSyncBridge) and stale devices won't stay around
    // indefinitely .
    return !device->chrome_version().empty() &&
           expected_expiration_time <= clock_->Now();
  });

  base::ranges::sort(device_infos, [](const syncer::DeviceInfo* left_device,
                                      const syncer::DeviceInfo* right_device) {
    return left_device->last_updated_timestamp() <
           right_device->last_updated_timestamp();
  });

  return device_infos;
}

}  // namespace browser_sync

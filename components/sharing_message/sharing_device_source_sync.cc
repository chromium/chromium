// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_device_source_sync.h"

#include <vector>

#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/stl_util.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "components/sharing_message/sharing_utils.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_name_util.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "components/sync_device_info/local_device_info_util.h"

using sync_pb::SharingSpecificFields;

namespace {

bool IsStale(const syncer::DeviceInfo& device) {
  const base::Time min_updated_time =
      base::Time::Now() - kSharingDeviceExpiration;
  return device.last_updated_timestamp() < min_updated_time;
}

}  // namespace

SharingDeviceSourceSync::SharingDeviceSourceSync(
    syncer::SyncService* sync_service,
    syncer::LocalDeviceInfoProvider* local_device_info_provider,
    syncer::DeviceInfoTracker* device_info_tracker)
    : sync_service_(sync_service),
      local_device_info_provider_(local_device_info_provider),
      device_info_tracker_(device_info_tracker) {
  if (!device_info_tracker_->IsSyncing()) {
    device_info_tracker_->AddObserver(this);
  }

  if (!local_device_info_provider_->GetLocalDeviceInfo()) {
    local_device_info_ready_subscription_ =
        local_device_info_provider_->RegisterOnInitializedCallback(
            base::BindRepeating(
                &SharingDeviceSourceSync::OnLocalDeviceInfoProviderReady,
                weak_ptr_factory_.GetWeakPtr()));
  }
}

SharingDeviceSourceSync::~SharingDeviceSourceSync() {
  device_info_tracker_->RemoveObserver(this);
}

std::optional<SharingTargetDeviceInfo> SharingDeviceSourceSync::GetDeviceByGuid(
    const std::string& guid) {
  if (!IsSyncEnabledForSharing(sync_service_)) {
    return std::nullopt;
  }

  const syncer::DeviceInfo* device_info =
      device_info_tracker_->GetDeviceInfo(guid);
  if (!device_info) {
    return std::nullopt;
  }

  return SharingTargetDeviceInfo(
      device_info->guid(), syncer::GetDeviceDisplayNames(device_info).full_name,
      GetDevicePlatform(*device_info), device_info->pulse_interval(),
      device_info->form_factor(), device_info->last_updated_timestamp());
}

std::vector<SharingTargetDeviceInfo>
SharingDeviceSourceSync::GetDeviceCandidates(
    SharingSpecificFields::EnabledFeatures required_feature) {
  if (!IsSyncEnabledForSharing(sync_service_) || !IsReady()) {
    return {};
  }

  return ConvertAndDeduplicateDevices(FilterDeviceCandidates(
      device_info_tracker_->GetAllDeviceInfo(), required_feature));
}

bool SharingDeviceSourceSync::IsReady() {
  return IsSyncDisabledForSharing(sync_service_) ||
         (device_info_tracker_->IsSyncing() &&
          local_device_info_provider_->GetLocalDeviceInfo());
}

void SharingDeviceSourceSync::OnDeviceInfoChange() {
  TRACE_EVENT0("sharing", "SharingDeviceSourceSync::OnDeviceInfoChange");
  if (device_info_tracker_->IsSyncing()) {
    device_info_tracker_->RemoveObserver(this);
  }
  MaybeRunReadyCallbacks();
}

void SharingDeviceSourceSync::SetDeviceInfoTrackerForTesting(
    syncer::DeviceInfoTracker* tracker) {
  device_info_tracker_->RemoveObserver(this);
  device_info_tracker_ = tracker;
  if (!device_info_tracker_->IsSyncing()) {
    device_info_tracker_->AddObserver(this);
  }
  MaybeRunReadyCallbacks();
}

void SharingDeviceSourceSync::OnLocalDeviceInfoProviderReady() {
  DCHECK(local_device_info_provider_->GetLocalDeviceInfo());
  local_device_info_ready_subscription_ = {};
  MaybeRunReadyCallbacks();
}

std::vector<const syncer::DeviceInfo*>
SharingDeviceSourceSync::FilterDeviceCandidates(
    std::vector<const syncer::DeviceInfo*> devices,
    sync_pb::SharingSpecificFields::EnabledFeatures required_feature) const {
  std::set<SharingSpecificFields::EnabledFeatures> accepted_features{
      required_feature};
  bool can_send_via_sender_id = CanSendViaSenderID(sync_service_);

  std::erase_if(devices, [accepted_features, can_send_via_sender_id](
                             const syncer::DeviceInfo* device) {
    // Checks if |last_updated_timestamp| is not too old.
    if (IsStale(*device)) {
      return true;
    }

    // Checks if device has SharingInfo.
    if (!device->sharing_info()) {
      return true;
    }

    // Checks if message can be sent via either VAPID or sender ID.
    auto& sender_id_target_info = device->sharing_info()->sender_id_target_info;
    bool sender_id_channel_valid =
        (can_send_via_sender_id && !sender_id_target_info.fcm_token.empty() &&
         !sender_id_target_info.p256dh.empty() &&
         !sender_id_target_info.auth_secret.empty());
    if (!sender_id_channel_valid) {
      return true;
    }

    // Checks whether `device` supports any of `accepted_features`.
    return base::STLSetIntersection<
               std::vector<SharingSpecificFields::EnabledFeatures>>(
               device->sharing_info()->enabled_features, accepted_features)
        .empty();
  });
  return devices;
}

std::vector<SharingTargetDeviceInfo>
SharingDeviceSourceSync::ConvertAndDeduplicateDevices(
    std::vector<const syncer::DeviceInfo*> devices) const {
  // Sort the devices so the most recently modified devices are first.
  std::sort(devices.begin(), devices.end(),
            [](const auto& device1, const auto& device2) {
              return device1->last_updated_timestamp() >
                     device2->last_updated_timestamp();
            });

  const syncer::DeviceInfo* local_device =
      local_device_info_provider_->GetLocalDeviceInfo();
  std::optional<std::string> local_full_name;
  if (local_device) {
    local_full_name = syncer::GetDeviceDisplayNames(local_device).full_name;
  }

  // Resolve display names for the filtered list. This handles de-duplication
  // by name and chooses between short/full names based on collisions.
  std::vector<syncer::DeviceInfoWithName> device_names =
      syncer::DetermineDisplayNamesAndDeduplicate(devices, local_full_name);

  // Convert to SharingTargetDeviceInfo, filtering out any devices that were
  // de-duplicated by the naming utility.
  return base::ToVector(device_names, [](const auto& info) {
    return SharingTargetDeviceInfo(
        info.device->guid(), info.display_name, GetDevicePlatform(*info.device),
        info.device->pulse_interval(), info.device->form_factor(),
        info.device->last_updated_timestamp());
  });
}

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_device_source_sync.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/functional/callback.h"
#include "base/stl_util.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "components/sharing_message/sharing_utils.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "components/sync_device_info/local_device_info_util.h"

using sync_pb::SharingSpecificFields;

namespace {

bool IsStale(const syncer::DeviceInfo& device) {
  if (base::FeatureList::IsEnabled(kSharingMatchPulseInterval)) {
    base::TimeDelta pulse_delta = base::Hours(
        device.form_factor() == syncer::DeviceInfo::FormFactor::kDesktop
            ? kSharingPulseDeltaDesktopHours.Get()
            : kSharingPulseDeltaAndroidHours.Get());
    base::Time min_updated_time =
        base::Time::Now() - device.pulse_interval() - pulse_delta;
    return device.last_updated_timestamp() < min_updated_time;
  }

  const base::Time min_updated_time =
      base::Time::Now() - kSharingDeviceExpiration;
  return device.last_updated_timestamp() < min_updated_time;
}

SharingTargetDeviceInfo ConvertDeviceInfo(const syncer::DeviceInfo* device,
                                          bool use_short_name) {
  CHECK(device);

  const send_tab_to_self::SharingDeviceNames device_names =
      send_tab_to_self::GetSharingDeviceNames(device);

  const std::string& client_name =
      use_short_name ? device_names.short_name : device_names.full_name;

  return SharingTargetDeviceInfo(
      device->guid(), client_name, GetDevicePlatform(*device),
      device->pulse_interval(), device->form_factor(),
      device->last_updated_timestamp());
}

}  // namespace

SharingDeviceSourceSync::SharingDeviceSourceSync(
    syncer::SyncService* sync_service,
    syncer::LocalDeviceInfoProvider* local_device_info_provider,
    syncer::DeviceInfoTracker* device_info_tracker)
    : sync_service_(sync_service),
      local_device_info_provider_(local_device_info_provider),
      device_info_tracker_(device_info_tracker) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(syncer::GetPersonalizableDeviceNameBlocking),
      base::BindOnce(
          &SharingDeviceSourceSync::InitPersonalizableLocalDeviceName,
          weak_ptr_factory_.GetWeakPtr()));

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

  return ConvertDeviceInfo(device_info, /*use_short_name=*/false);
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
         (personalizable_local_device_name_ &&
          device_info_tracker_->IsSyncing() &&
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

void SharingDeviceSourceSync::InitPersonalizableLocalDeviceName(
    std::string personalizable_local_device_name) {
  personalizable_local_device_name_ =
      std::move(personalizable_local_device_name);
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
  if (required_feature == SharingSpecificFields::CLICK_TO_CALL_V2) {
    accepted_features.insert(SharingSpecificFields::CLICK_TO_CALL_VAPID);
  }
  if (required_feature == SharingSpecificFields::SHARED_CLIPBOARD_V2) {
    accepted_features.insert(SharingSpecificFields::SHARED_CLIPBOARD_VAPID);
  }

  bool can_send_via_vapid = CanSendViaVapid(sync_service_);
  bool can_send_via_sender_id = CanSendViaSenderID(sync_service_);

  std::erase_if(devices, [accepted_features, can_send_via_vapid,
                          can_send_via_sender_id](
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
    auto& vapid_target_info = device->sharing_info()->vapid_target_info;
    auto& sender_id_target_info = device->sharing_info()->sender_id_target_info;
    bool vapid_channel_valid =
        (can_send_via_vapid && !vapid_target_info.fcm_token.empty() &&
         !vapid_target_info.p256dh.empty() &&
         !vapid_target_info.auth_secret.empty());
    bool sender_id_channel_valid =
        (can_send_via_sender_id && !sender_id_target_info.fcm_token.empty() &&
         !sender_id_target_info.p256dh.empty() &&
         !sender_id_target_info.auth_secret.empty());
    if (!vapid_channel_valid && !sender_id_channel_valid) {
      return true;
    }

    // Checks whether |device| supports any of |accepted_features|.
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

  std::unordered_map<const syncer::DeviceInfo*,
                     send_tab_to_self::SharingDeviceNames>
      device_names_map;
  std::unordered_set<std::string> full_names;
  std::unordered_map<std::string, int> short_names_counter;

  // To prevent adding candidates with same full name as local device.
  full_names.insert(send_tab_to_self::GetSharingDeviceNames(
                        local_device_info_provider_->GetLocalDeviceInfo())
                        .full_name);
  // To prevent M78- instances of Chrome with same device model from showing up.
  full_names.insert(*personalizable_local_device_name_);

  for (const syncer::DeviceInfo* device : devices) {
    send_tab_to_self::SharingDeviceNames device_names =
        send_tab_to_self::GetSharingDeviceNames(device);

    // Only insert the first occurrence of each device name.
    auto inserted = full_names.insert(device_names.full_name);
    if (!inserted.second) {
      continue;
    }

    short_names_counter[device_names.short_name]++;
    device_names_map.insert({device, std::move(device_names)});
  }

  // Filter duplicates and convert devices.
  std::vector<SharingTargetDeviceInfo> converted_devices;

  for (const syncer::DeviceInfo* device : devices) {
    auto it = device_names_map.find(device);
    if (it == device_names_map.end()) {
      continue;
    }

    const send_tab_to_self::SharingDeviceNames& device_names = it->second;
    bool unique_short_name = short_names_counter[device_names.short_name] == 1;

    converted_devices.push_back(
        ConvertDeviceInfo(device,
                          /*use_short_name=*/unique_short_name));
  }

  return converted_devices;
}

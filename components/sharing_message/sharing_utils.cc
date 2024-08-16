// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_utils.h"

#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_device_info/device_info.h"

namespace {

bool CanListDevices(syncer::SyncService* sync_service) {
  syncer::DataTypeSet active_data_types = sync_service->GetActiveDataTypes();

  // Can list device using DeviceInfo and sharing.synced_devices preferences.
  if (active_data_types.HasAll({syncer::DEVICE_INFO, syncer::PREFERENCES})) {
    return true;
  }

  // Can list device using only DeviceInfo.
  if (active_data_types.Has(syncer::DEVICE_INFO)) {
    return true;
  }

  return false;
}

}  // namespace

bool CanSendViaVapid(syncer::SyncService* sync_service) {
  // Can send using VAPID key in sharing.vapid_key preferences.
  return sync_service->GetActiveDataTypes().Has(syncer::PREFERENCES);
}

bool CanSendViaSenderID(syncer::SyncService* sync_service) {
  return sync_service->GetActiveDataTypes().Has(syncer::SHARING_MESSAGE);
}

bool IsSyncEnabledForSharing(syncer::SyncService* sync_service) {
  if (!sync_service) {
    return false;
  }

  if (sync_service->GetTransportState() !=
      syncer::SyncService::TransportState::ACTIVE) {
    return false;
  }

  if (!CanListDevices(sync_service)) {
    return false;
  }

  if (!CanSendViaVapid(sync_service) && !CanSendViaSenderID(sync_service)) {
    return false;
  }

  return true;
}

bool IsSyncDisabledForSharing(syncer::SyncService* sync_service) {
  // Sync service is not initialized, we can't be sure it's disabled.
  if (!sync_service) {
    return false;
  }

  if (sync_service->GetTransportState() ==
          syncer::SyncService::TransportState::DISABLED ||
      sync_service->GetTransportState() ==
          syncer::SyncService::TransportState::PAUSED) {
    return true;
  }

  // Ignore transient states.
  if (sync_service->GetTransportState() !=
      syncer::SyncService::TransportState::ACTIVE) {
    return false;
  }

  if (!CanListDevices(sync_service)) {
    return true;
  }

  if (!CanSendViaVapid(sync_service) && !CanSendViaSenderID(sync_service)) {
    return true;
  }

  return false;
}

std::optional<components_sharing_message::FCMChannelConfiguration>
GetFCMChannel(const syncer::DeviceInfo& device_info) {
  if (!device_info.sharing_info()) {
    return std::nullopt;
  }

  components_sharing_message::FCMChannelConfiguration fcm_configuration;
  auto& vapid_target_info = device_info.sharing_info()->vapid_target_info;
  auto& sender_id_target_info =
      device_info.sharing_info()->sender_id_target_info;
  fcm_configuration.set_vapid_fcm_token(vapid_target_info.fcm_token);
  fcm_configuration.set_vapid_p256dh(vapid_target_info.p256dh);
  fcm_configuration.set_vapid_auth_secret(vapid_target_info.auth_secret);
  fcm_configuration.set_sender_id_fcm_token(sender_id_target_info.fcm_token);
  fcm_configuration.set_sender_id_p256dh(sender_id_target_info.p256dh);
  fcm_configuration.set_sender_id_auth_secret(
      sender_id_target_info.auth_secret);

  return fcm_configuration;
}

SharingDevicePlatform GetDevicePlatform(const syncer::DeviceInfo& device_info) {
  switch (device_info.os_type()) {
    case syncer::DeviceInfo::OsType::kChromeOsLacros:
    case syncer::DeviceInfo::OsType::kChromeOsAsh:
      return SharingDevicePlatform::kChromeOS;
    case syncer::DeviceInfo::OsType::kLinux:
      return SharingDevicePlatform::kLinux;
    case syncer::DeviceInfo::OsType::kMac:
      return SharingDevicePlatform::kMac;
    case syncer::DeviceInfo::OsType::kWindows:
      return SharingDevicePlatform::kWindows;
    case syncer::DeviceInfo::OsType::kAndroid:
      return SharingDevicePlatform::kAndroid;
    case syncer::DeviceInfo::OsType::kIOS:
      return SharingDevicePlatform::kIOS;
    case syncer::DeviceInfo::OsType::kUnknown:
    case syncer::DeviceInfo::OsType::kFuchsia:
      return SharingDevicePlatform::kUnknown;
  }
}

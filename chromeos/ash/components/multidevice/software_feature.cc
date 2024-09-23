// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/multidevice/software_feature.h"

#include "base/notreached.h"

namespace ash::multidevice {

SoftwareFeature FromCryptAuthFeature(
    cryptauth::SoftwareFeature cryptauth_feature) {
  switch (cryptauth_feature) {
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST:
      return SoftwareFeature::kBetterTogetherHost;
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT:
      return SoftwareFeature::kBetterTogetherClient;
    case cryptauth::SoftwareFeature::EASY_UNLOCK_HOST:
      return SoftwareFeature::kSmartLockHost;
    case cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT:
      return SoftwareFeature::kSmartLockClient;
    case cryptauth::SoftwareFeature::MAGIC_TETHER_HOST:
      return SoftwareFeature::kInstantTetheringHost;
    case cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT:
      return SoftwareFeature::kInstantTetheringClient;
    case cryptauth::SoftwareFeature::SMS_CONNECT_HOST:
      return SoftwareFeature::kMessagesForWebHost;
    case cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT:
      return SoftwareFeature::kMessagesForWebClient;
    case cryptauth::SoftwareFeature::PHONE_HUB_HOST:
      return SoftwareFeature::kPhoneHubHost;
    case cryptauth::SoftwareFeature::PHONE_HUB_CLIENT:
      return SoftwareFeature::kPhoneHubClient;
    case cryptauth::SoftwareFeature::WIFI_SYNC_HOST:
      return SoftwareFeature::kWifiSyncHost;
    case cryptauth::SoftwareFeature::WIFI_SYNC_CLIENT:
      return SoftwareFeature::kWifiSyncClient;
    case cryptauth::SoftwareFeature::ECHE_HOST:
      return SoftwareFeature::kEcheHost;
    case cryptauth::SoftwareFeature::ECHE_CLIENT:
      return SoftwareFeature::kEcheClient;
    case cryptauth::SoftwareFeature::PHONE_HUB_CAMERA_ROLL_HOST:
      return SoftwareFeature::kPhoneHubCameraRollHost;
    case cryptauth::SoftwareFeature::PHONE_HUB_CAMERA_ROLL_CLIENT:
      return SoftwareFeature::kPhoneHubCameraRollClient;
    case cryptauth::SoftwareFeature::UNKNOWN_FEATURE:
      NOTREACHED_IN_MIGRATION();
  }

  NOTREACHED_IN_MIGRATION();
  return SoftwareFeature::kBetterTogetherHost;
}

cryptauth::SoftwareFeature ToCryptAuthFeature(
    SoftwareFeature multidevice_feature) {
  // Note: No default case needed since SoftwareFeature is an enum class.
  switch (multidevice_feature) {
    case SoftwareFeature::kBetterTogetherHost:
      return cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST;
    case SoftwareFeature::kBetterTogetherClient:
      return cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT;
    case SoftwareFeature::kSmartLockHost:
      return cryptauth::SoftwareFeature::EASY_UNLOCK_HOST;
    case SoftwareFeature::kSmartLockClient:
      return cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT;
    case SoftwareFeature::kInstantTetheringHost:
      return cryptauth::SoftwareFeature::MAGIC_TETHER_HOST;
    case SoftwareFeature::kInstantTetheringClient:
      return cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT;
    case SoftwareFeature::kMessagesForWebHost:
      return cryptauth::SoftwareFeature::SMS_CONNECT_HOST;
    case SoftwareFeature::kMessagesForWebClient:
      return cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT;
    case SoftwareFeature::kPhoneHubHost:
      return cryptauth::SoftwareFeature::PHONE_HUB_HOST;
    case SoftwareFeature::kPhoneHubClient:
      return cryptauth::SoftwareFeature::PHONE_HUB_CLIENT;
    case SoftwareFeature::kWifiSyncHost:
      return cryptauth::SoftwareFeature::WIFI_SYNC_HOST;
    case SoftwareFeature::kWifiSyncClient:
      return cryptauth::SoftwareFeature::WIFI_SYNC_CLIENT;
    case SoftwareFeature::kEcheHost:
      return cryptauth::SoftwareFeature::ECHE_HOST;
    case SoftwareFeature::kEcheClient:
      return cryptauth::SoftwareFeature::ECHE_CLIENT;
    case SoftwareFeature::kPhoneHubCameraRollHost:
      return cryptauth::SoftwareFeature::PHONE_HUB_CAMERA_ROLL_HOST;
    case SoftwareFeature::kPhoneHubCameraRollClient:
      return cryptauth::SoftwareFeature::PHONE_HUB_CAMERA_ROLL_CLIENT;
  }

  NOTREACHED_IN_MIGRATION();
  return cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST;
}

std::ostream& operator<<(std::ostream& stream, const SoftwareFeature& feature) {
  switch (feature) {
    case SoftwareFeature::kBetterTogetherHost:
      stream << "[Better Together host]";
      break;
    case SoftwareFeature::kBetterTogetherClient:
      stream << "[Better Together client]";
      break;
    case SoftwareFeature::kSmartLockHost:
      stream << "[Smart Lock host]";
      break;
    case SoftwareFeature::kSmartLockClient:
      stream << "[Smart Lock client]";
      break;
    case SoftwareFeature::kInstantTetheringHost:
      stream << "[Instant Tethering host]";
      break;
    case SoftwareFeature::kInstantTetheringClient:
      stream << "[Instant Tethering client]";
      break;
    case SoftwareFeature::kMessagesForWebHost:
      stream << "[Messages for Web host]";
      break;
    case SoftwareFeature::kMessagesForWebClient:
      stream << "[Messages for Web client]";
      break;
    case SoftwareFeature::kPhoneHubHost:
      stream << "[Phone Hub host]";
      break;
    case SoftwareFeature::kPhoneHubClient:
      stream << "[Phone Hub client]";
      break;
    case SoftwareFeature::kWifiSyncHost:
      stream << "[Wifi Sync host]";
      break;
    case SoftwareFeature::kWifiSyncClient:
      stream << "[Wifi Sync client]";
      break;
    case SoftwareFeature::kEcheHost:
      stream << "[Eche host]";
      break;
    case SoftwareFeature::kEcheClient:
      stream << "[Eche client]";
      break;
    case SoftwareFeature::kPhoneHubCameraRollHost:
      stream << "[Phone Hub Camera Roll host]";
      break;
    case SoftwareFeature::kPhoneHubCameraRollClient:
      stream << "[Phone Hub Camera Roll client]";
      break;
  }
  return stream;
}

}  // namespace ash::multidevice

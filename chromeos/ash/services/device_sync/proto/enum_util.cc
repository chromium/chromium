// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/proto/enum_util.h"

namespace ash {

namespace device_sync {

std::ostream& operator<<(std::ostream& stream,
                         const cryptauth::DeviceType& device_type) {
  switch (device_type) {
    case cryptauth::DeviceType::ANDROID:
      stream << "[Android]";
      break;
    case cryptauth::DeviceType::CHROME:
      stream << "[Chrome]";
      break;
    case cryptauth::DeviceType::IOS:
      stream << "[iOS]";
      break;
    case cryptauth::DeviceType::BROWSER:
      stream << "[Browser]";
      break;
    default:
      stream << "[Unknown device type]";
      break;
  }
  return stream;
}

cryptauth::DeviceType DeviceTypeStringToEnum(
    const std::string& device_type_as_string) {
  if (device_type_as_string == "android")
    return cryptauth::DeviceType::ANDROID;
  if (device_type_as_string == "chrome")
    return cryptauth::DeviceType::CHROME;
  if (device_type_as_string == "ios")
    return cryptauth::DeviceType::IOS;
  if (device_type_as_string == "browser")
    return cryptauth::DeviceType::BROWSER;
  return cryptauth::DeviceType::UNKNOWN;
}

std::string DeviceTypeEnumToString(cryptauth::DeviceType device_type) {
  switch (device_type) {
    case cryptauth::DeviceType::ANDROID:
      return "android";
    case cryptauth::DeviceType::CHROME:
      return "chrome";
    case cryptauth::DeviceType::IOS:
      return "ios";
    case cryptauth::DeviceType::BROWSER:
      return "browser";
    default:
      return "unknown";
  }
}

std::ostream& operator<<(std::ostream& stream,
                         const cryptauth::SoftwareFeature& software_feature) {
  switch (software_feature) {
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST:
      stream << "[Better Together host]";
      break;
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT:
      stream << "[Better Together client]";
      break;
    case cryptauth::SoftwareFeature::EASY_UNLOCK_HOST:
      stream << "[EasyUnlock host]";
      break;
    case cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT:
      stream << "[EasyUnlock client]";
      break;
    case cryptauth::SoftwareFeature::MAGIC_TETHER_HOST:
      stream << "[Instant Tethering host]";
      break;
    case cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT:
      stream << "[Instant Tethering client]";
      break;
    case cryptauth::SoftwareFeature::SMS_CONNECT_HOST:
      stream << "[SMS Connect host]";
      break;
    case cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT:
      stream << "[SMS Connect client]";
      break;
    case cryptauth::SoftwareFeature::PHONE_HUB_HOST:
      stream << "[Phone Hub host]";
      break;
    case cryptauth::SoftwareFeature::PHONE_HUB_CLIENT:
      stream << "[Phone Hub client]";
      break;
    case cryptauth::SoftwareFeature::WIFI_SYNC_HOST:
      stream << "[Wifi Sync host]";
      break;
    case cryptauth::SoftwareFeature::WIFI_SYNC_CLIENT:
      stream << "[Wifi Sync client]";
      break;
    case cryptauth::SoftwareFeature::ECHE_HOST:
      stream << "[Eche host]";
      break;
    case cryptauth::SoftwareFeature::ECHE_CLIENT:
      stream << "[Eche client]";
      break;
    case cryptauth::SoftwareFeature::PHONE_HUB_CAMERA_ROLL_HOST:
      stream << "[Phone Hub Camera Roll host]";
      break;
    case cryptauth::SoftwareFeature::PHONE_HUB_CAMERA_ROLL_CLIENT:
      stream << "[Phone Hub Camera Roll client]";
      break;
    default:
      stream << "[unknown software feature]";
      break;
  }
  return stream;
}

cryptauth::SoftwareFeature SoftwareFeatureStringToEnum(
    const std::string& software_feature_as_string) {
  if (software_feature_as_string == "betterTogetherHost")
    return cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST;
  if (software_feature_as_string == "betterTogetherClient")
    return cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT;
  if (software_feature_as_string == "easyUnlockHost")
    return cryptauth::SoftwareFeature::EASY_UNLOCK_HOST;
  if (software_feature_as_string == "easyUnlockClient")
    return cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT;
  if (software_feature_as_string == "magicTetherHost")
    return cryptauth::SoftwareFeature::MAGIC_TETHER_HOST;
  if (software_feature_as_string == "magicTetherClient")
    return cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT;
  if (software_feature_as_string == "smsConnectHost")
    return cryptauth::SoftwareFeature::SMS_CONNECT_HOST;
  if (software_feature_as_string == "smsConnectClient")
    return cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT;
  if (software_feature_as_string == "phoneHubHost")
    return cryptauth::SoftwareFeature::PHONE_HUB_HOST;
  if (software_feature_as_string == "phoneHubClient")
    return cryptauth::SoftwareFeature::PHONE_HUB_CLIENT;
  if (software_feature_as_string == "wifiSyncHost")
    return cryptauth::SoftwareFeature::WIFI_SYNC_HOST;
  if (software_feature_as_string == "wifiSyncClient")
    return cryptauth::SoftwareFeature::WIFI_SYNC_CLIENT;
  if (software_feature_as_string == "phoneHubCameraRollHost")
    return cryptauth::SoftwareFeature::PHONE_HUB_CAMERA_ROLL_HOST;
  if (software_feature_as_string == "phoneHubCameraRollClient")
    return cryptauth::SoftwareFeature::PHONE_HUB_CAMERA_ROLL_CLIENT;

  return cryptauth::SoftwareFeature::UNKNOWN_FEATURE;
}

std::string SoftwareFeatureEnumToString(
    cryptauth::SoftwareFeature software_feature) {
  switch (software_feature) {
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST:
      return "betterTogetherHost";
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT:
      return "betterTogetherClient";
    case cryptauth::SoftwareFeature::EASY_UNLOCK_HOST:
      return "easyUnlockHost";
    case cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT:
      return "easyUnlockClient";
    case cryptauth::SoftwareFeature::MAGIC_TETHER_HOST:
      return "magicTetherHost";
    case cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT:
      return "magicTetherClient";
    case cryptauth::SoftwareFeature::SMS_CONNECT_HOST:
      return "smsConnectHost";
    case cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT:
      return "smsConnectClient";
    case cryptauth::SoftwareFeature::PHONE_HUB_HOST:
      return "phoneHubHost";
    case cryptauth::SoftwareFeature::PHONE_HUB_CLIENT:
      return "phoneHubClient";
    case cryptauth::SoftwareFeature::WIFI_SYNC_HOST:
      return "wifiSyncHost";
    case cryptauth::SoftwareFeature::WIFI_SYNC_CLIENT:
      return "wifiSyncClient";
    case cryptauth::SoftwareFeature::PHONE_HUB_CAMERA_ROLL_HOST:
      return "phoneHubCameraRollHost";
    case cryptauth::SoftwareFeature::PHONE_HUB_CAMERA_ROLL_CLIENT:
      return "phoneHubCameraRollClient";
    default:
      return "unknownFeature";
  }
}

std::string SoftwareFeatureEnumToStringAllCaps(
    cryptauth::SoftwareFeature software_feature) {
  switch (software_feature) {
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST:
      return "BETTER_TOGETHER_HOST";
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT:
      return "BETTER_TOGETHER_CLIENT";
    case cryptauth::SoftwareFeature::EASY_UNLOCK_HOST:
      return "EASY_UNLOCK_HOST";
    case cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT:
      return "EASY_UNLOCK_CLIENT";
    case cryptauth::SoftwareFeature::MAGIC_TETHER_HOST:
      return "MAGIC_TETHER_HOST";
    case cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT:
      return "MAGIC_TETHER_CLIENT";
    case cryptauth::SoftwareFeature::SMS_CONNECT_HOST:
      return "SMS_CONNECT_HOST";
    case cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT:
      return "SMS_CONNECT_CLIENT";
    case cryptauth::SoftwareFeature::PHONE_HUB_HOST:
      return "PHONE_HUB_HOST";
    case cryptauth::SoftwareFeature::PHONE_HUB_CLIENT:
      return "PHONE_HUB_CLIENT";
    case cryptauth::SoftwareFeature::WIFI_SYNC_HOST:
      return "WIFI_SYNC_HOST";
    case cryptauth::SoftwareFeature::WIFI_SYNC_CLIENT:
      return "WIFI_SYNC_CLIENT";
    case cryptauth::SoftwareFeature::PHONE_HUB_CAMERA_ROLL_HOST:
      return "PHONE_HUB_CAMERA_ROLL_HOST";
    case cryptauth::SoftwareFeature::PHONE_HUB_CAMERA_ROLL_CLIENT:
      return "PHONE_HUB_CAMERA_ROLL_CLIENT";
    default:
      return "UNKNOWN_FEATURE";
  }
}

}  // namespace device_sync

}  // namespace ash

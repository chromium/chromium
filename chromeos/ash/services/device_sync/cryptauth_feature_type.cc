// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_feature_type.h"

#include "ash/constants/ash_features.h"
#include "base/base64url.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "crypto/sha2.h"

namespace ash {

namespace device_sync {

namespace {

const char kBetterTogetherHostSupportedString[] =
    "BETTER_TOGETHER_HOST_SUPPORTED";
const char kBetterTogetherClientSupportedString[] =
    "BETTER_TOGETHER_CLIENT_SUPPORTED";
const char kEasyUnlockHostSupportedString[] = "EASY_UNLOCK_HOST_SUPPORTED";
const char kEasyUnlockClientSupportedString[] = "EASY_UNLOCK_CLIENT_SUPPORTED";
const char kMagicTetherHostSupportedString[] = "MAGIC_TETHER_HOST_SUPPORTED";
const char kMagicTetherClientSupportedString[] =
    "MAGIC_TETHER_CLIENT_SUPPORTED";
const char kSmsConnectHostSupportedString[] = "SMS_CONNECT_HOST_SUPPORTED";
const char kSmsConnectClientSupportedString[] = "SMS_CONNECT_CLIENT_SUPPORTED";
const char kPhoneHubHostSupportedString[] = "PHONE_HUB_HOST_SUPPORTED";
const char kPhoneHubClientSupportedString[] = "PHONE_HUB_CLIENT_SUPPORTED";
const char kWifiSyncHostSupportedString[] = "WIFI_SYNC_HOST_SUPPORTED";
const char kWifiSyncClientSupportedString[] = "WIFI_SYNC_CLIENT_SUPPORTED";
const char kEcheHostSupportedString[] = "EXO_HOST_SUPPORTED";
const char kEcheClientSupportedString[] = "EXO_CLIENT_SUPPORTED";
const char kPhoneHubCameraRollHostSupportedString[] =
    "PHONE_HUB_CAMERA_ROLL_HOST_SUPPORTED";
const char kPhoneHubCameraRollClientSupportedString[] =
    "PHONE_HUB_CAMERA_ROLL_CLIENT_SUPPORTED";

const char kBetterTogetherHostEnabledString[] = "BETTER_TOGETHER_HOST";
const char kBetterTogetherClientEnabledString[] = "BETTER_TOGETHER_CLIENT";
const char kEasyUnlockHostEnabledString[] = "EASY_UNLOCK_HOST";
const char kEasyUnlockClientEnabledString[] = "EASY_UNLOCK_CLIENT";
const char kMagicTetherHostEnabledString[] = "MAGIC_TETHER_HOST";
const char kMagicTetherClientEnabledString[] = "MAGIC_TETHER_CLIENT";
const char kSmsConnectHostEnabledString[] = "SMS_CONNECT_HOST";
const char kSmsConnectClientEnabledString[] = "SMS_CONNECT_CLIENT";
const char kPhoneHubHostEnabledString[] = "PHONE_HUB_HOST";
const char kPhoneHubClientEnabledString[] = "PHONE_HUB_CLIENT";
const char kWifiSyncHostEnabledString[] = "WIFI_SYNC_HOST";
const char kWifiSyncClientEnabledString[] = "WIFI_SYNC_CLIENT";
const char kEcheHostEnabledString[] = "EXO_HOST";
const char kEcheClientEnabledString[] = "EXO_CLIENT";
const char kPhoneHubCameraRollHostEnabledString[] =
    "PHONE_HUB_CAMERA_ROLL_HOST";
const char kPhoneHubCameraRollClientEnabledString[] =
    "PHONE_HUB_CAMERA_ROLL_CLIENT";

}  // namespace

const base::flat_set<CryptAuthFeatureType>& GetAllCryptAuthFeatureTypes() {
  static const base::NoDestructor<base::flat_set<CryptAuthFeatureType>>
      feature_set([] {
        base::flat_set<CryptAuthFeatureType> feature_set{
            CryptAuthFeatureType::kBetterTogetherHostSupported,
            CryptAuthFeatureType::kBetterTogetherClientSupported,
            CryptAuthFeatureType::kEasyUnlockHostSupported,
            CryptAuthFeatureType::kEasyUnlockClientSupported,
            CryptAuthFeatureType::kMagicTetherHostSupported,
            CryptAuthFeatureType::kMagicTetherClientSupported,
            CryptAuthFeatureType::kSmsConnectHostSupported,
            CryptAuthFeatureType::kSmsConnectClientSupported,
            CryptAuthFeatureType::kBetterTogetherHostEnabled,
            CryptAuthFeatureType::kBetterTogetherClientEnabled,
            CryptAuthFeatureType::kEasyUnlockHostEnabled,
            CryptAuthFeatureType::kEasyUnlockClientEnabled,
            CryptAuthFeatureType::kMagicTetherHostEnabled,
            CryptAuthFeatureType::kMagicTetherClientEnabled,
            CryptAuthFeatureType::kSmsConnectHostEnabled,
            CryptAuthFeatureType::kSmsConnectClientEnabled};
        if (features::IsPhoneHubEnabled()) {
          feature_set.insert(CryptAuthFeatureType::kPhoneHubClientSupported);
          feature_set.insert(CryptAuthFeatureType::kPhoneHubClientEnabled);
          feature_set.insert(CryptAuthFeatureType::kPhoneHubHostSupported);
          feature_set.insert(CryptAuthFeatureType::kPhoneHubHostEnabled);
        }
        if (features::IsWifiSyncAndroidEnabled()) {
          feature_set.insert(CryptAuthFeatureType::kWifiSyncClientSupported);
          feature_set.insert(CryptAuthFeatureType::kWifiSyncClientEnabled);
          feature_set.insert(CryptAuthFeatureType::kWifiSyncHostSupported);
          feature_set.insert(CryptAuthFeatureType::kWifiSyncHostEnabled);
        }
        if (features::IsEcheSWAEnabled()) {
          feature_set.insert(CryptAuthFeatureType::kEcheClientSupported);
          feature_set.insert(CryptAuthFeatureType::kEcheClientEnabled);
          feature_set.insert(CryptAuthFeatureType::kEcheHostSupported);
          feature_set.insert(CryptAuthFeatureType::kEcheHostEnabled);
        }
        if (features::IsPhoneHubCameraRollEnabled()) {
          feature_set.insert(
              CryptAuthFeatureType::kPhoneHubCameraRollClientSupported);
          feature_set.insert(
              CryptAuthFeatureType::kPhoneHubCameraRollClientEnabled);
          feature_set.insert(
              CryptAuthFeatureType::kPhoneHubCameraRollHostSupported);
          feature_set.insert(
              CryptAuthFeatureType::kPhoneHubCameraRollHostEnabled);
        }
        return feature_set;
      }());

  return *feature_set;
}

const base::flat_set<CryptAuthFeatureType>&
GetSupportedCryptAuthFeatureTypes() {
  static const base::NoDestructor<base::flat_set<CryptAuthFeatureType>>
      supported_set([] {
        base::flat_set<CryptAuthFeatureType> supported_set{
            CryptAuthFeatureType::kBetterTogetherHostSupported,
            CryptAuthFeatureType::kBetterTogetherClientSupported,
            CryptAuthFeatureType::kEasyUnlockHostSupported,
            CryptAuthFeatureType::kEasyUnlockClientSupported,
            CryptAuthFeatureType::kMagicTetherHostSupported,
            CryptAuthFeatureType::kMagicTetherClientSupported,
            CryptAuthFeatureType::kSmsConnectHostSupported,
            CryptAuthFeatureType::kSmsConnectClientSupported};
        if (features::IsPhoneHubEnabled()) {
          supported_set.insert(CryptAuthFeatureType::kPhoneHubHostSupported);
          supported_set.insert(CryptAuthFeatureType::kPhoneHubClientSupported);
        }
        if (features::IsWifiSyncAndroidEnabled()) {
          supported_set.insert(CryptAuthFeatureType::kWifiSyncHostSupported);
          supported_set.insert(CryptAuthFeatureType::kWifiSyncClientSupported);
        }
        if (features::IsEcheSWAEnabled()) {
          supported_set.insert(CryptAuthFeatureType::kEcheHostSupported);
          supported_set.insert(CryptAuthFeatureType::kEcheClientSupported);
        }
        if (features::IsPhoneHubCameraRollEnabled()) {
          supported_set.insert(
              CryptAuthFeatureType::kPhoneHubCameraRollHostSupported);
          supported_set.insert(
              CryptAuthFeatureType::kPhoneHubCameraRollClientSupported);
        }
        return supported_set;
      }());

  return *supported_set;
}

const base::flat_set<CryptAuthFeatureType>& GetEnabledCryptAuthFeatureTypes() {
  static const base::NoDestructor<base::flat_set<CryptAuthFeatureType>>
      enabled_set([] {
        base::flat_set<CryptAuthFeatureType> enabled_set{
            CryptAuthFeatureType::kBetterTogetherHostEnabled,
            CryptAuthFeatureType::kBetterTogetherClientEnabled,
            CryptAuthFeatureType::kEasyUnlockHostEnabled,
            CryptAuthFeatureType::kEasyUnlockClientEnabled,
            CryptAuthFeatureType::kMagicTetherHostEnabled,
            CryptAuthFeatureType::kMagicTetherClientEnabled,
            CryptAuthFeatureType::kSmsConnectHostEnabled,
            CryptAuthFeatureType::kSmsConnectClientEnabled};
        if (features::IsPhoneHubEnabled()) {
          enabled_set.insert(CryptAuthFeatureType::kPhoneHubHostEnabled);
          enabled_set.insert(CryptAuthFeatureType::kPhoneHubClientEnabled);
        }
        if (features::IsWifiSyncAndroidEnabled()) {
          enabled_set.insert(CryptAuthFeatureType::kWifiSyncHostEnabled);
          enabled_set.insert(CryptAuthFeatureType::kWifiSyncClientEnabled);
        }
        if (features::IsEcheSWAEnabled()) {
          enabled_set.insert(CryptAuthFeatureType::kEcheHostEnabled);
          enabled_set.insert(CryptAuthFeatureType::kEcheClientEnabled);
        }
        if (features::IsPhoneHubCameraRollEnabled()) {
          enabled_set.insert(
              CryptAuthFeatureType::kPhoneHubCameraRollHostEnabled);
          enabled_set.insert(
              CryptAuthFeatureType::kPhoneHubCameraRollClientEnabled);
        }
        return enabled_set;
      }());

  return *enabled_set;
}

const base::flat_set<std::string>& GetAllCryptAuthFeatureTypeStrings() {
  static const base::NoDestructor<base::flat_set<std::string>>
      feature_string_set([] {
        base::flat_set<std::string> feature_string_set;
        for (CryptAuthFeatureType feature_type : GetAllCryptAuthFeatureTypes())
          feature_string_set.insert(CryptAuthFeatureTypeToString(feature_type));

        return feature_string_set;
      }());
  return *feature_string_set;
}

const char* CryptAuthFeatureTypeToString(CryptAuthFeatureType feature_type) {
  switch (feature_type) {
    case CryptAuthFeatureType::kBetterTogetherHostSupported:
      return kBetterTogetherHostSupportedString;
    case CryptAuthFeatureType::kBetterTogetherHostEnabled:
      return kBetterTogetherHostEnabledString;
    case CryptAuthFeatureType::kBetterTogetherClientSupported:
      return kBetterTogetherClientSupportedString;
    case CryptAuthFeatureType::kBetterTogetherClientEnabled:
      return kBetterTogetherClientEnabledString;
    case CryptAuthFeatureType::kEasyUnlockHostSupported:
      return kEasyUnlockHostSupportedString;
    case CryptAuthFeatureType::kEasyUnlockHostEnabled:
      return kEasyUnlockHostEnabledString;
    case CryptAuthFeatureType::kEasyUnlockClientSupported:
      return kEasyUnlockClientSupportedString;
    case CryptAuthFeatureType::kEasyUnlockClientEnabled:
      return kEasyUnlockClientEnabledString;
    case CryptAuthFeatureType::kMagicTetherHostSupported:
      return kMagicTetherHostSupportedString;
    case CryptAuthFeatureType::kMagicTetherHostEnabled:
      return kMagicTetherHostEnabledString;
    case CryptAuthFeatureType::kMagicTetherClientSupported:
      return kMagicTetherClientSupportedString;
    case CryptAuthFeatureType::kMagicTetherClientEnabled:
      return kMagicTetherClientEnabledString;
    case CryptAuthFeatureType::kSmsConnectHostSupported:
      return kSmsConnectHostSupportedString;
    case CryptAuthFeatureType::kSmsConnectHostEnabled:
      return kSmsConnectHostEnabledString;
    case CryptAuthFeatureType::kSmsConnectClientSupported:
      return kSmsConnectClientSupportedString;
    case CryptAuthFeatureType::kSmsConnectClientEnabled:
      return kSmsConnectClientEnabledString;
    case CryptAuthFeatureType::kPhoneHubHostSupported:
      return kPhoneHubHostSupportedString;
    case CryptAuthFeatureType::kPhoneHubHostEnabled:
      return kPhoneHubHostEnabledString;
    case CryptAuthFeatureType::kPhoneHubClientSupported:
      return kPhoneHubClientSupportedString;
    case CryptAuthFeatureType::kPhoneHubClientEnabled:
      return kPhoneHubClientEnabledString;
    case CryptAuthFeatureType::kWifiSyncHostSupported:
      return kWifiSyncHostSupportedString;
    case CryptAuthFeatureType::kWifiSyncHostEnabled:
      return kWifiSyncHostEnabledString;
    case CryptAuthFeatureType::kWifiSyncClientSupported:
      return kWifiSyncClientSupportedString;
    case CryptAuthFeatureType::kWifiSyncClientEnabled:
      return kWifiSyncClientEnabledString;
    case CryptAuthFeatureType::kEcheHostSupported:
      return kEcheHostSupportedString;
    case CryptAuthFeatureType::kEcheHostEnabled:
      return kEcheHostEnabledString;
    case CryptAuthFeatureType::kEcheClientSupported:
      return kEcheClientSupportedString;
    case CryptAuthFeatureType::kEcheClientEnabled:
      return kEcheClientEnabledString;
    case CryptAuthFeatureType::kPhoneHubCameraRollHostSupported:
      return kPhoneHubCameraRollHostSupportedString;
    case CryptAuthFeatureType::kPhoneHubCameraRollHostEnabled:
      return kPhoneHubCameraRollHostEnabledString;
    case CryptAuthFeatureType::kPhoneHubCameraRollClientSupported:
      return kPhoneHubCameraRollClientSupportedString;
    case CryptAuthFeatureType::kPhoneHubCameraRollClientEnabled:
      return kPhoneHubCameraRollClientEnabledString;
  }
}

std::optional<CryptAuthFeatureType> CryptAuthFeatureTypeFromString(
    const std::string& feature_type_string) {
  if (feature_type_string == kBetterTogetherHostSupportedString)
    return CryptAuthFeatureType::kBetterTogetherHostSupported;
  if (feature_type_string == kBetterTogetherHostEnabledString)
    return CryptAuthFeatureType::kBetterTogetherHostEnabled;
  if (feature_type_string == kBetterTogetherClientSupportedString)
    return CryptAuthFeatureType::kBetterTogetherClientSupported;
  if (feature_type_string == kBetterTogetherClientEnabledString)
    return CryptAuthFeatureType::kBetterTogetherClientEnabled;
  if (feature_type_string == kEasyUnlockHostSupportedString)
    return CryptAuthFeatureType::kEasyUnlockHostSupported;
  if (feature_type_string == kEasyUnlockHostEnabledString)
    return CryptAuthFeatureType::kEasyUnlockHostEnabled;
  if (feature_type_string == kEasyUnlockClientSupportedString)
    return CryptAuthFeatureType::kEasyUnlockClientSupported;
  if (feature_type_string == kEasyUnlockClientEnabledString)
    return CryptAuthFeatureType::kEasyUnlockClientEnabled;
  if (feature_type_string == kMagicTetherHostSupportedString)
    return CryptAuthFeatureType::kMagicTetherHostSupported;
  if (feature_type_string == kMagicTetherHostEnabledString)
    return CryptAuthFeatureType::kMagicTetherHostEnabled;
  if (feature_type_string == kMagicTetherClientSupportedString)
    return CryptAuthFeatureType::kMagicTetherClientSupported;
  if (feature_type_string == kMagicTetherClientEnabledString)
    return CryptAuthFeatureType::kMagicTetherClientEnabled;
  if (feature_type_string == kSmsConnectHostSupportedString)
    return CryptAuthFeatureType::kSmsConnectHostSupported;
  if (feature_type_string == kSmsConnectHostEnabledString)
    return CryptAuthFeatureType::kSmsConnectHostEnabled;
  if (feature_type_string == kSmsConnectClientSupportedString)
    return CryptAuthFeatureType::kSmsConnectClientSupported;
  if (feature_type_string == kSmsConnectClientEnabledString)
    return CryptAuthFeatureType::kSmsConnectClientEnabled;
  if (feature_type_string == kPhoneHubHostSupportedString)
    return CryptAuthFeatureType::kPhoneHubHostSupported;
  if (feature_type_string == kPhoneHubHostEnabledString)
    return CryptAuthFeatureType::kPhoneHubHostEnabled;
  if (feature_type_string == kPhoneHubClientSupportedString)
    return CryptAuthFeatureType::kPhoneHubClientSupported;
  if (feature_type_string == kPhoneHubClientEnabledString)
    return CryptAuthFeatureType::kPhoneHubClientEnabled;
  if (feature_type_string == kWifiSyncHostSupportedString)
    return CryptAuthFeatureType::kWifiSyncHostSupported;
  if (feature_type_string == kWifiSyncHostEnabledString)
    return CryptAuthFeatureType::kWifiSyncHostEnabled;
  if (feature_type_string == kWifiSyncClientSupportedString)
    return CryptAuthFeatureType::kWifiSyncClientSupported;
  if (feature_type_string == kWifiSyncClientEnabledString)
    return CryptAuthFeatureType::kWifiSyncClientEnabled;
  if (feature_type_string == kEcheHostSupportedString)
    return CryptAuthFeatureType::kEcheHostSupported;
  if (feature_type_string == kEcheHostEnabledString)
    return CryptAuthFeatureType::kEcheHostEnabled;
  if (feature_type_string == kEcheClientSupportedString)
    return CryptAuthFeatureType::kEcheClientSupported;
  if (feature_type_string == kEcheClientEnabledString)
    return CryptAuthFeatureType::kEcheClientEnabled;
  if (feature_type_string == kPhoneHubCameraRollHostSupportedString)
    return CryptAuthFeatureType::kPhoneHubCameraRollHostSupported;
  if (feature_type_string == kPhoneHubCameraRollHostEnabledString)
    return CryptAuthFeatureType::kPhoneHubCameraRollHostEnabled;
  if (feature_type_string == kPhoneHubCameraRollClientSupportedString)
    return CryptAuthFeatureType::kPhoneHubCameraRollClientSupported;
  if (feature_type_string == kPhoneHubCameraRollClientEnabledString)
    return CryptAuthFeatureType::kPhoneHubCameraRollClientEnabled;

  return std::nullopt;
}

// Computes the base64url-encoded, SHA-256 8-byte hash of the
// CryptAuthFeatureType string.
std::string CryptAuthFeatureTypeToGcmHash(CryptAuthFeatureType feature_type) {
  std::string hash_8_bytes(8, 0);
  crypto::SHA256HashString(CryptAuthFeatureTypeToString(feature_type),
                           std::data(hash_8_bytes), 8u);

  std::string hash_base64url;
  base::Base64UrlEncode(hash_8_bytes, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &hash_base64url);

  return hash_base64url;
}

std::optional<CryptAuthFeatureType> CryptAuthFeatureTypeFromGcmHash(
    const std::string& feature_type_hash) {
  // The map from the feature type hash value that CryptAuth sends in GCM
  // messages to the CryptAuthFeatureType enum.
  static const base::NoDestructor<
      base::flat_map<std::string, CryptAuthFeatureType>>
      hash_to_feature_map([] {
        base::flat_map<std::string, CryptAuthFeatureType> hash_to_feature_map;
        for (const CryptAuthFeatureType& feature_type :
             GetAllCryptAuthFeatureTypes()) {
          hash_to_feature_map.insert_or_assign(
              CryptAuthFeatureTypeToGcmHash(feature_type), feature_type);
        }

        return hash_to_feature_map;
      }());

  auto it = hash_to_feature_map->find(feature_type_hash);

  if (it == hash_to_feature_map->end())
    return std::nullopt;

  return it->second;
}

multidevice::SoftwareFeature CryptAuthFeatureTypeToSoftwareFeature(
    CryptAuthFeatureType feature_type) {
  switch (feature_type) {
    case CryptAuthFeatureType::kBetterTogetherHostSupported:
      [[fallthrough]];
    case CryptAuthFeatureType::kBetterTogetherHostEnabled:
      return multidevice::SoftwareFeature::kBetterTogetherHost;

    case CryptAuthFeatureType::kBetterTogetherClientSupported:
      [[fallthrough]];
    case CryptAuthFeatureType::kBetterTogetherClientEnabled:
      return multidevice::SoftwareFeature::kBetterTogetherClient;

    case CryptAuthFeatureType::kEasyUnlockHostSupported:
      [[fallthrough]];
    case CryptAuthFeatureType::kEasyUnlockHostEnabled:
      return multidevice::SoftwareFeature::kSmartLockHost;

    case CryptAuthFeatureType::kEasyUnlockClientSupported:
      [[fallthrough]];
    case CryptAuthFeatureType::kEasyUnlockClientEnabled:
      return multidevice::SoftwareFeature::kSmartLockClient;

    case CryptAuthFeatureType::kMagicTetherHostSupported:
      [[fallthrough]];
    case CryptAuthFeatureType::kMagicTetherHostEnabled:
      return multidevice::SoftwareFeature::kInstantTetheringHost;

    case CryptAuthFeatureType::kMagicTetherClientSupported:
      [[fallthrough]];
    case CryptAuthFeatureType::kMagicTetherClientEnabled:
      return multidevice::SoftwareFeature::kInstantTetheringClient;

    case CryptAuthFeatureType::kSmsConnectHostSupported:
      [[fallthrough]];
    case CryptAuthFeatureType::kSmsConnectHostEnabled:
      return multidevice::SoftwareFeature::kMessagesForWebHost;

    case CryptAuthFeatureType::kSmsConnectClientSupported:
      [[fallthrough]];
    case CryptAuthFeatureType::kSmsConnectClientEnabled:
      return multidevice::SoftwareFeature::kMessagesForWebClient;

    case CryptAuthFeatureType::kPhoneHubHostSupported:
      [[fallthrough]];
    case CryptAuthFeatureType::kPhoneHubHostEnabled:
      return multidevice::SoftwareFeature::kPhoneHubHost;

    case CryptAuthFeatureType::kPhoneHubClientSupported:
      [[fallthrough]];
    case CryptAuthFeatureType::kPhoneHubClientEnabled:
      return multidevice::SoftwareFeature::kPhoneHubClient;

    case CryptAuthFeatureType::kWifiSyncHostSupported:
      [[fallthrough]];
    case CryptAuthFeatureType::kWifiSyncHostEnabled:
      return multidevice::SoftwareFeature::kWifiSyncHost;

    case CryptAuthFeatureType::kWifiSyncClientSupported:
      [[fallthrough]];
    case CryptAuthFeatureType::kWifiSyncClientEnabled:
      return multidevice::SoftwareFeature::kWifiSyncClient;

    case CryptAuthFeatureType::kEcheHostSupported:
      [[fallthrough]];
    case CryptAuthFeatureType::kEcheHostEnabled:
      return multidevice::SoftwareFeature::kEcheHost;

    case CryptAuthFeatureType::kEcheClientSupported:
      [[fallthrough]];
    case CryptAuthFeatureType::kEcheClientEnabled:
      return multidevice::SoftwareFeature::kEcheClient;

    case CryptAuthFeatureType::kPhoneHubCameraRollHostSupported:
      [[fallthrough]];
    case CryptAuthFeatureType::kPhoneHubCameraRollHostEnabled:
      return multidevice::SoftwareFeature::kPhoneHubCameraRollHost;

    case CryptAuthFeatureType::kPhoneHubCameraRollClientSupported:
      [[fallthrough]];
    case CryptAuthFeatureType::kPhoneHubCameraRollClientEnabled:
      return multidevice::SoftwareFeature::kPhoneHubCameraRollClient;
  }
}

CryptAuthFeatureType CryptAuthFeatureTypeFromSoftwareFeature(
    multidevice::SoftwareFeature software_feature) {
  switch (software_feature) {
    case multidevice::SoftwareFeature::kBetterTogetherHost:
      return CryptAuthFeatureType::kBetterTogetherHostEnabled;
    case multidevice::SoftwareFeature::kBetterTogetherClient:
      return CryptAuthFeatureType::kBetterTogetherClientEnabled;
    case multidevice::SoftwareFeature::kSmartLockHost:
      return CryptAuthFeatureType::kEasyUnlockHostEnabled;
    case multidevice::SoftwareFeature::kSmartLockClient:
      return CryptAuthFeatureType::kEasyUnlockClientEnabled;
    case multidevice::SoftwareFeature::kInstantTetheringHost:
      return CryptAuthFeatureType::kMagicTetherHostEnabled;
    case multidevice::SoftwareFeature::kInstantTetheringClient:
      return CryptAuthFeatureType::kMagicTetherClientEnabled;
    case multidevice::SoftwareFeature::kMessagesForWebHost:
      return CryptAuthFeatureType::kSmsConnectHostEnabled;
    case multidevice::SoftwareFeature::kMessagesForWebClient:
      return CryptAuthFeatureType::kSmsConnectClientEnabled;
    case multidevice::SoftwareFeature::kPhoneHubHost:
      return CryptAuthFeatureType::kPhoneHubHostEnabled;
    case multidevice::SoftwareFeature::kPhoneHubClient:
      return CryptAuthFeatureType::kPhoneHubClientEnabled;
    case multidevice::SoftwareFeature::kWifiSyncHost:
      return CryptAuthFeatureType::kWifiSyncHostEnabled;
    case multidevice::SoftwareFeature::kWifiSyncClient:
      return CryptAuthFeatureType::kWifiSyncClientEnabled;
    case multidevice::SoftwareFeature::kEcheHost:
      return CryptAuthFeatureType::kEcheHostEnabled;
    case multidevice::SoftwareFeature::kEcheClient:
      return CryptAuthFeatureType::kEcheClientEnabled;
    case multidevice::SoftwareFeature::kPhoneHubCameraRollHost:
      return CryptAuthFeatureType::kPhoneHubCameraRollHostEnabled;
    case multidevice::SoftwareFeature::kPhoneHubCameraRollClient:
      return CryptAuthFeatureType::kPhoneHubCameraRollClientEnabled;
  }
}

std::ostream& operator<<(std::ostream& stream,
                         CryptAuthFeatureType feature_type) {
  stream << CryptAuthFeatureTypeToString(feature_type);
  return stream;
}

}  // namespace device_sync

}  // namespace ash

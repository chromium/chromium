// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_feature_type.h"

#include <array>
#include <cstddef>

#include "ash/constants/ash_features.h"
#include "base/base64url.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "crypto/hash.h"

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

CryptAuthFeatureTypeSet GetAllCryptAuthFeatureTypes() {
  return base::Union(GetSupportedCryptAuthFeatureTypes(),
                     GetEnabledCryptAuthFeatureTypes());
}

CryptAuthFeatureTypeSet GetSupportedCryptAuthFeatureTypes() {
  static const CryptAuthFeatureTypeSet supported_set = [] {
    CryptAuthFeatureTypeSet supported_set{
        CryptAuthFeatureType::kBetterTogetherHostSupported,
        CryptAuthFeatureType::kBetterTogetherClientSupported,
        CryptAuthFeatureType::kEasyUnlockHostSupported,
        CryptAuthFeatureType::kEasyUnlockClientSupported,
        CryptAuthFeatureType::kMagicTetherHostSupported,
        CryptAuthFeatureType::kMagicTetherClientSupported,
        CryptAuthFeatureType::kSmsConnectHostSupported,
        CryptAuthFeatureType::kSmsConnectClientSupported,
        CryptAuthFeatureType::kEcheHostSupported,
        CryptAuthFeatureType::kEcheClientSupported,
        CryptAuthFeatureType::kPhoneHubCameraRollHostSupported,
        CryptAuthFeatureType::kPhoneHubCameraRollClientSupported};
    if (features::IsPhoneHubEnabled()) {
      supported_set.PutAll({CryptAuthFeatureType::kPhoneHubHostSupported,
                            CryptAuthFeatureType::kPhoneHubClientSupported});
    }
    if (features::IsWifiSyncAndroidEnabled()) {
      supported_set.PutAll({CryptAuthFeatureType::kWifiSyncHostSupported,
                            CryptAuthFeatureType::kWifiSyncClientSupported});
    }
    return supported_set;
  }();

  return supported_set;
}

CryptAuthFeatureTypeSet GetEnabledCryptAuthFeatureTypes() {
  static const CryptAuthFeatureTypeSet enabled_set = [] {
    CryptAuthFeatureTypeSet enabled_set{
        CryptAuthFeatureType::kBetterTogetherHostEnabled,
        CryptAuthFeatureType::kBetterTogetherClientEnabled,
        CryptAuthFeatureType::kEasyUnlockHostEnabled,
        CryptAuthFeatureType::kEasyUnlockClientEnabled,
        CryptAuthFeatureType::kMagicTetherHostEnabled,
        CryptAuthFeatureType::kMagicTetherClientEnabled,
        CryptAuthFeatureType::kSmsConnectHostEnabled,
        CryptAuthFeatureType::kSmsConnectClientEnabled,
        CryptAuthFeatureType::kEcheHostEnabled,
        CryptAuthFeatureType::kEcheClientEnabled,
        CryptAuthFeatureType::kPhoneHubCameraRollHostEnabled,
        CryptAuthFeatureType::kPhoneHubCameraRollClientEnabled};
    if (features::IsPhoneHubEnabled()) {
      enabled_set.PutAll({CryptAuthFeatureType::kPhoneHubHostEnabled,
                          CryptAuthFeatureType::kPhoneHubClientEnabled});
    }
    if (features::IsWifiSyncAndroidEnabled()) {
      enabled_set.PutAll({CryptAuthFeatureType::kWifiSyncHostEnabled,
                          CryptAuthFeatureType::kWifiSyncClientEnabled});
    }
    return enabled_set;
  }();

  return enabled_set;
}

base::span<const std::string_view> GetAllCryptAuthFeatureTypeStrings() {
  struct FeatureTypeStrings {
    std::array<std::string_view, CryptAuthFeatureTypeSet::kValueCount> data;
    size_t size = 0;
  };
  static const FeatureTypeStrings feature_strings = [] {
    FeatureTypeStrings result;
    for (CryptAuthFeatureType feature_type : GetAllCryptAuthFeatureTypes()) {
      result.data[result.size++] = CryptAuthFeatureTypeToString(feature_type);
    }
    return result;
  }();
  return base::span(feature_strings.data).first(feature_strings.size);
}

std::string_view CryptAuthFeatureTypeToString(
    CryptAuthFeatureType feature_type) {
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
    std::string_view feature_type_string) {
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

// For a given CryptAuthFeatureType, return the base64url encoding of the first
// 8 bytes of the SHA-256 of its string representation.
std::string CryptAuthFeatureTypeToGcmHash(CryptAuthFeatureType feature_type) {
  auto hash = crypto::hash::Sha256(
      base::as_byte_span(CryptAuthFeatureTypeToString(feature_type)));

  std::string hash_base64url;
  base::Base64UrlEncode(base::span(hash).first(8u),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
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
        for (CryptAuthFeatureType feature_type :
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

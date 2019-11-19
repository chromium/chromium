// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_feature_type.h"

#include "base/base64url.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "crypto/sha2.h"

namespace chromeos {

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

const char kBetterTogetherHostEnabledString[] = "BETTER_TOGETHER_HOST";
const char kBetterTogetherClientEnabledString[] = "BETTER_TOGETHER_CLIENT";
const char kEasyUnlockHostEnabledString[] = "EASY_UNLOCK_HOST";
const char kEasyUnlockClientEnabledString[] = "EASY_UNLOCK_CLIENT";
const char kMagicTetherHostEnabledString[] = "MAGIC_TETHER_HOST";
const char kMagicTetherClientEnabledString[] = "MAGIC_TETHER_CLIENT";
const char kSmsConnectHostEnabledString[] = "SMS_CONNECT_HOST";
const char kSmsConnectClientEnabledString[] = "SMS_CONNECT_CLIENT";

}  // namespace

const base::flat_set<CryptAuthFeatureType>& GetAllCryptAuthFeatureTypes() {
  static const base::flat_set<CryptAuthFeatureType> feature_set{
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

  return feature_set;
}

const base::flat_set<CryptAuthFeatureType>&
GetSupportedCryptAuthFeatureTypes() {
  static const base::flat_set<CryptAuthFeatureType> supported_set{
      CryptAuthFeatureType::kBetterTogetherHostSupported,
      CryptAuthFeatureType::kBetterTogetherClientSupported,
      CryptAuthFeatureType::kEasyUnlockHostSupported,
      CryptAuthFeatureType::kEasyUnlockClientSupported,
      CryptAuthFeatureType::kMagicTetherHostSupported,
      CryptAuthFeatureType::kMagicTetherClientSupported,
      CryptAuthFeatureType::kSmsConnectHostSupported,
      CryptAuthFeatureType::kSmsConnectClientSupported};

  return supported_set;
}

const base::flat_set<CryptAuthFeatureType>& GetEnabledCryptAuthFeatureTypes() {
  static const base::flat_set<CryptAuthFeatureType> enabled_set{
      CryptAuthFeatureType::kBetterTogetherHostEnabled,
      CryptAuthFeatureType::kBetterTogetherClientEnabled,
      CryptAuthFeatureType::kEasyUnlockHostEnabled,
      CryptAuthFeatureType::kEasyUnlockClientEnabled,
      CryptAuthFeatureType::kMagicTetherHostEnabled,
      CryptAuthFeatureType::kMagicTetherClientEnabled,
      CryptAuthFeatureType::kSmsConnectHostEnabled,
      CryptAuthFeatureType::kSmsConnectClientEnabled};

  return enabled_set;
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
  }
}

base::Optional<CryptAuthFeatureType> CryptAuthFeatureTypeFromString(
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

  return base::nullopt;
}

// Computes the base64url-encoded, SHA-256 8-byte hash of the
// CryptAuthFeatureType string.
std::string CryptAuthFeatureTypeToGcmHash(CryptAuthFeatureType feature_type) {
  std::string hash_8_bytes(8, 0);
  crypto::SHA256HashString(CryptAuthFeatureTypeToString(feature_type),
                           base::data(hash_8_bytes), 8u);

  std::string hash_base64url;
  base::Base64UrlEncode(hash_8_bytes, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &hash_base64url);

  return hash_base64url;
}

base::Optional<CryptAuthFeatureType> CryptAuthFeatureTypeFromGcmHash(
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
    return base::nullopt;

  return it->second;
}

multidevice::SoftwareFeature CryptAuthFeatureTypeToSoftwareFeature(
    CryptAuthFeatureType feature_type) {
  switch (feature_type) {
    case CryptAuthFeatureType::kBetterTogetherHostSupported:
      FALLTHROUGH;
    case CryptAuthFeatureType::kBetterTogetherHostEnabled:
      return multidevice::SoftwareFeature::kBetterTogetherHost;

    case CryptAuthFeatureType::kBetterTogetherClientSupported:
      FALLTHROUGH;
    case CryptAuthFeatureType::kBetterTogetherClientEnabled:
      return multidevice::SoftwareFeature::kBetterTogetherClient;

    case CryptAuthFeatureType::kEasyUnlockHostSupported:
      FALLTHROUGH;
    case CryptAuthFeatureType::kEasyUnlockHostEnabled:
      return multidevice::SoftwareFeature::kSmartLockHost;

    case CryptAuthFeatureType::kEasyUnlockClientSupported:
      FALLTHROUGH;
    case CryptAuthFeatureType::kEasyUnlockClientEnabled:
      return multidevice::SoftwareFeature::kSmartLockClient;

    case CryptAuthFeatureType::kMagicTetherHostSupported:
      FALLTHROUGH;
    case CryptAuthFeatureType::kMagicTetherHostEnabled:
      return multidevice::SoftwareFeature::kInstantTetheringHost;

    case CryptAuthFeatureType::kMagicTetherClientSupported:
      FALLTHROUGH;
    case CryptAuthFeatureType::kMagicTetherClientEnabled:
      return multidevice::SoftwareFeature::kInstantTetheringClient;

    case CryptAuthFeatureType::kSmsConnectHostSupported:
      FALLTHROUGH;
    case CryptAuthFeatureType::kSmsConnectHostEnabled:
      return multidevice::SoftwareFeature::kMessagesForWebHost;

    case CryptAuthFeatureType::kSmsConnectClientSupported:
      FALLTHROUGH;
    case CryptAuthFeatureType::kSmsConnectClientEnabled:;
      return multidevice::SoftwareFeature::kMessagesForWebClient;
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
  }
}

std::ostream& operator<<(std::ostream& stream,
                         CryptAuthFeatureType feature_type) {
  stream << CryptAuthFeatureTypeToString(feature_type);
  return stream;
}

}  // namespace device_sync

}  // namespace chromeos

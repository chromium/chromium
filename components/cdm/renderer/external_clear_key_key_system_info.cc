// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/renderer/external_clear_key_key_system_info.h"

#include "base/notreached.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_names.h"

namespace cdm {

const char kExternalClearKeyKeySystem[] = "org.chromium.externalclearkey";
const char kExternalClearKeyInvalidKeySystem[] =
    "org.chromium.externalclearkey.invalid";

ExternalClearKeySystemInfo::ExternalClearKeySystemInfo() = default;

ExternalClearKeySystemInfo::~ExternalClearKeySystemInfo() = default;

std::string ExternalClearKeySystemInfo::GetBaseKeySystemName() const {
  return kExternalClearKeyKeySystem;
}

bool ExternalClearKeySystemInfo::IsSupportedKeySystem(
    const std::string& key_system) const {
  // Supports kExternalClearKeyKeySystem and all its sub key systems, except for
  // the explicitly "invalid" one. See the test
  // EncryptedMediaSupportedTypesExternalClearKeyTest.InvalidKeySystems.
  return (key_system == kExternalClearKeyKeySystem ||
          media::IsSubKeySystemOf(key_system, kExternalClearKeyKeySystem)) &&
         key_system != kExternalClearKeyInvalidKeySystem;
}

bool ExternalClearKeySystemInfo::IsSupportedInitDataType(
    media::EmeInitDataType init_data_type) const {
  switch (init_data_type) {
    case media::EmeInitDataType::CENC:
    case media::EmeInitDataType::WEBM:
    case media::EmeInitDataType::KEYIDS:
      return true;

    case media::EmeInitDataType::UNKNOWN:
      return false;
  }
  NOTREACHED();
  return false;
}

absl::optional<media::EmeConfig>
ExternalClearKeySystemInfo::GetEncryptionSchemeConfigRule(
    media::EncryptionScheme encryption_scheme) const {
  switch (encryption_scheme) {
    case media::EncryptionScheme::kCenc:
    case media::EncryptionScheme::kCbcs:
      return media::EmeConfig::SupportedRule();
    case media::EncryptionScheme::kUnencrypted:
      break;
  }
  NOTREACHED();
  return media::EmeConfig::UnsupportedRule();
}

media::SupportedCodecs ExternalClearKeySystemInfo::GetSupportedCodecs() const {
  return media::EME_CODEC_MP4_ALL | media::EME_CODEC_WEBM_ALL;
}

absl::optional<media::EmeConfig>
ExternalClearKeySystemInfo::GetRobustnessConfigRule(
    const std::string& key_system,
    media::EmeMediaType media_type,
    const std::string& requested_robustness,
    const bool* /*hw_secure_requirement*/) const {
  if (requested_robustness.empty()) {
    return media::EmeConfig::SupportedRule();
  } else {
    return media::EmeConfig::UnsupportedRule();
  }
}

// Persistent license sessions are faked.
absl::optional<media::EmeConfig>
ExternalClearKeySystemInfo::GetPersistentLicenseSessionSupport() const {
  return media::EmeConfig::SupportedRule();
}

media::EmeFeatureSupport ExternalClearKeySystemInfo::GetPersistentStateSupport()
    const {
  return media::EmeFeatureSupport::REQUESTABLE;
}

media::EmeFeatureSupport
ExternalClearKeySystemInfo::GetDistinctiveIdentifierSupport() const {
  return media::EmeFeatureSupport::NOT_SUPPORTED;
}

}  // namespace cdm

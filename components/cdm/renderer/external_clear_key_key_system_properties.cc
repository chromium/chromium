// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/renderer/external_clear_key_key_system_properties.h"

#include "base/notreached.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_names.h"

namespace cdm {

const char kExternalClearKeyKeySystem[] = "org.chromium.externalclearkey";
const char kExternalClearKeyInvalidKeySystem[] =
    "org.chromium.externalclearkey.invalid";

ExternalClearKeyProperties::ExternalClearKeyProperties() = default;

ExternalClearKeyProperties::~ExternalClearKeyProperties() = default;

std::string ExternalClearKeyProperties::GetBaseKeySystemName() const {
  return kExternalClearKeyKeySystem;
}

bool ExternalClearKeyProperties::IsSupportedKeySystem(
    const std::string& key_system) const {
  // Supports kExternalClearKeyKeySystem and all its sub key systems, except for
  // the explicitly "invalid" one. See the test
  // EncryptedMediaSupportedTypesExternalClearKeyTest.InvalidKeySystems.
  return (key_system == kExternalClearKeyKeySystem ||
          media::IsSubKeySystemOf(key_system, kExternalClearKeyKeySystem)) &&
         key_system != kExternalClearKeyInvalidKeySystem;
}

bool ExternalClearKeyProperties::IsSupportedInitDataType(
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

media::EmeConfigRule ExternalClearKeyProperties::GetEncryptionSchemeConfigRule(
    media::EncryptionScheme encryption_scheme) const {
  switch (encryption_scheme) {
    case media::EncryptionScheme::kCenc:
    case media::EncryptionScheme::kCbcs:
      return media::EmeConfigRule::SUPPORTED;
    case media::EncryptionScheme::kUnencrypted:
      break;
  }
  NOTREACHED();
  return media::EmeConfigRule::NOT_SUPPORTED;
}

media::SupportedCodecs ExternalClearKeyProperties::GetSupportedCodecs() const {
  return media::EME_CODEC_MP4_ALL | media::EME_CODEC_WEBM_ALL;
}

media::EmeConfigRule ExternalClearKeyProperties::GetRobustnessConfigRule(
    const std::string& key_system,
    media::EmeMediaType media_type,
    const std::string& requested_robustness,
    const bool* /*hw_secure_requirement*/) const {
  return requested_robustness.empty() ? media::EmeConfigRule::SUPPORTED
                                      : media::EmeConfigRule::NOT_SUPPORTED;
}

// Persistent license sessions are faked.
media::EmeSessionTypeSupport
ExternalClearKeyProperties::GetPersistentLicenseSessionSupport() const {
  return media::EmeSessionTypeSupport::SUPPORTED;
}

media::EmeFeatureSupport ExternalClearKeyProperties::GetPersistentStateSupport()
    const {
  return media::EmeFeatureSupport::REQUESTABLE;
}

media::EmeFeatureSupport
ExternalClearKeyProperties::GetDistinctiveIdentifierSupport() const {
  return media::EmeFeatureSupport::NOT_SUPPORTED;
}

}  // namespace cdm

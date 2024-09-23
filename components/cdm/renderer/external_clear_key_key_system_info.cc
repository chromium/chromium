// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/renderer/external_clear_key_key_system_info.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/notreached.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_names.h"
#include "media/cdm/clear_key_cdm_common.h"

namespace cdm {

ExternalClearKeyKeySystemInfo::ExternalClearKeyKeySystemInfo()
    : ExternalClearKeyKeySystemInfo(
          // Supports kExternalClearKeyKeySystem and all its sub key systems,
          // except for the explicitly "invalid" one. See the test
          // EncryptedMediaSupportedTypesExternalClearKeyTest.InvalidKeySystems.
          media::kExternalClearKeyKeySystem,
          // Excludes kMediaFoundationClearKeyKeySystem to treat MediaFoundation
          // Clear Key key system as a separate one.
          {media::kExternalClearKeyInvalidKeySystem,
#if BUILDFLAG(IS_WIN)
           media::kMediaFoundationClearKeyKeySystem
#endif  // BUILDFLAG(IS_WIN)
          },
          media::EME_CODEC_MP4_ALL | media::EME_CODEC_WEBM_ALL,
          std::nullopt,
          media::EmeFeatureSupport::REQUESTABLE,
          media::EmeFeatureSupport::NOT_SUPPORTED) {
}

ExternalClearKeyKeySystemInfo::ExternalClearKeyKeySystemInfo(
    const std::string& key_system,
    std::vector<std::string> excluded_key_systems,
    media::SupportedCodecs codecs,
    media::EmeConfig::Rule eme_config_rule,
    media::EmeFeatureSupport persistent_state_support,
    media::EmeFeatureSupport distinctive_identifier_support)
    : key_system_(key_system),
      excluded_key_systems_(excluded_key_systems),
      codecs_(codecs),
      eme_config_rule_(eme_config_rule),
      persistent_state_support_(persistent_state_support),
      distinctive_identifier_support_(distinctive_identifier_support) {}

ExternalClearKeyKeySystemInfo::~ExternalClearKeyKeySystemInfo() = default;

std::string ExternalClearKeyKeySystemInfo::GetBaseKeySystemName() const {
  return key_system_;
}

bool ExternalClearKeyKeySystemInfo::IsSupportedKeySystem(
    const std::string& key_system) const {
  return (key_system == key_system_ ||
          media::IsSubKeySystemOf(key_system, key_system_)) &&
         !base::Contains(excluded_key_systems_, key_system);
}

bool ExternalClearKeyKeySystemInfo::IsSupportedInitDataType(
    media::EmeInitDataType init_data_type) const {
  switch (init_data_type) {
    case media::EmeInitDataType::CENC:
    case media::EmeInitDataType::WEBM:
    case media::EmeInitDataType::KEYIDS:
      return true;

    case media::EmeInitDataType::UNKNOWN:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

std::optional<media::EmeConfig>
ExternalClearKeyKeySystemInfo::GetEncryptionSchemeConfigRule(
    media::EncryptionScheme encryption_scheme) const {
  switch (encryption_scheme) {
    case media::EncryptionScheme::kCenc:
    case media::EncryptionScheme::kCbcs:
      return media::EmeConfig::SupportedRule();
    case media::EncryptionScheme::kUnencrypted:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return media::EmeConfig::UnsupportedRule();
}

media::SupportedCodecs ExternalClearKeyKeySystemInfo::GetSupportedCodecs()
    const {
  return codecs_;
}

// On Windows, MediaFoundation Clear Key CDM requires HW secure codecs. We
// need this method to pretent to require this for testing purposes.
media::SupportedCodecs
ExternalClearKeyKeySystemInfo::GetSupportedHwSecureCodecs() const {
  return codecs_;
}

std::optional<media::EmeConfig>
ExternalClearKeyKeySystemInfo::GetRobustnessConfigRule(
    const std::string& key_system,
    media::EmeMediaType media_type,
    const std::string& requested_robustness,
    const bool* /*hw_secure_requirement*/) const {
  if (eme_config_rule_.has_value()) {
    return eme_config_rule_;
  }

  if (requested_robustness.empty()) {
    return media::EmeConfig::SupportedRule();
  } else {
    return media::EmeConfig::UnsupportedRule();
  }
}

// Persistent license sessions are faked.
std::optional<media::EmeConfig>
ExternalClearKeyKeySystemInfo::GetPersistentLicenseSessionSupport() const {
  return media::EmeConfig::SupportedRule();
}

media::EmeFeatureSupport
ExternalClearKeyKeySystemInfo::GetPersistentStateSupport() const {
  return persistent_state_support_;
}

media::EmeFeatureSupport
ExternalClearKeyKeySystemInfo::GetDistinctiveIdentifierSupport() const {
  return distinctive_identifier_support_;
}

}  // namespace cdm

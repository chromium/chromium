// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/renderer/android_key_system_info.h"

#include <utility>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

#if !BUILDFLAG(IS_ANDROID)
#error This file should only be built when Android is enabled.
#endif

using media::EmeConfig;
using media::EmeConfigRuleState;
using media::EmeFeatureSupport;
using media::EmeInitDataType;
using media::EmeMediaType;
using media::EncryptionScheme;
using media::SupportedCodecs;

namespace cdm {

AndroidKeySystemInfo::AndroidKeySystemInfo(
    const std::string& name,
    SupportedCodecs sw_secure_codecs,
    base::flat_set<EncryptionScheme> sw_secure_encryption_schemes,
    SupportedCodecs hw_secure_codecs,
    base::flat_set<EncryptionScheme> hw_secure_encryption_schemes)
    : name_(name),
      sw_secure_codecs_(sw_secure_codecs),
      sw_secure_encryption_schemes_(std::move(sw_secure_encryption_schemes)),
      hw_secure_codecs_(hw_secure_codecs),
      hw_secure_encryption_schemes_(std::move(hw_secure_encryption_schemes)) {
  DCHECK_NE(name, kWidevineKeySystem)
      << "Use WidevineKeySystemInfo for Widevine";
}

AndroidKeySystemInfo::~AndroidKeySystemInfo() = default;

std::string AndroidKeySystemInfo::GetBaseKeySystemName() const {
  return name_;
}

bool AndroidKeySystemInfo::IsSupportedInitDataType(
    EmeInitDataType init_data_type) const {
  // Here we assume that support for a container implies support for the
  // associated initialization data type. KeySystems handles validating
  // |init_data_type| x |container| pairings.
  switch (init_data_type) {
    case EmeInitDataType::WEBM:
      return (sw_secure_codecs_ & media::EME_CODEC_WEBM_ALL) != 0;
    case EmeInitDataType::CENC:
      return (sw_secure_codecs_ & media::EME_CODEC_MP4_ALL) != 0;
    case EmeInitDataType::KEYIDS:
    case EmeInitDataType::UNKNOWN:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

EmeConfig::Rule AndroidKeySystemInfo::GetEncryptionSchemeConfigRule(
    EncryptionScheme encryption_scheme) const {
  bool is_sw_secure_supported =
      sw_secure_encryption_schemes_.contains(encryption_scheme);
  bool is_hw_secure_supported =
      hw_secure_encryption_schemes_.contains(encryption_scheme);
  if (is_sw_secure_supported && is_hw_secure_supported) {
    return EmeConfig::SupportedRule();
  } else if (is_sw_secure_supported && !is_hw_secure_supported) {
    return EmeConfig{.hw_secure_codecs = EmeConfigRuleState::kNotAllowed};
  } else if (!is_sw_secure_supported && is_hw_secure_supported) {
    return EmeConfig{.hw_secure_codecs = EmeConfigRuleState::kRequired};
  } else {
    return EmeConfig::UnsupportedRule();
  }
}

SupportedCodecs AndroidKeySystemInfo::GetSupportedCodecs() const {
  return sw_secure_codecs_;
}

SupportedCodecs AndroidKeySystemInfo::GetSupportedHwSecureCodecs() const {
  return hw_secure_codecs_;
}

EmeConfig::Rule AndroidKeySystemInfo::GetRobustnessConfigRule(
    const std::string& /*key_system*/,
    EmeMediaType /*media_type*/,
    const std::string& requested_robustness,
    const bool* /*hw_secure_requirement*/) const {
  // For non-Widevine key systems, we don't know what `robustness` specifies.
  // `hw_secure_requirement` is ignored here because it's a temporary solution
  // until a larger refactoring of the key system logic is done. It also does
  // not need to account for it here because if it does introduce an
  // incompatibility at this point, it will still be caught by the rule logic
  // in KeySystemConfigSelector: crbug.com/1204284
  if (requested_robustness.empty()) {
    return EmeConfig::SupportedRule();
  } else {
    return EmeConfig::UnsupportedRule();
  }
}

EmeConfig::Rule AndroidKeySystemInfo::GetPersistentLicenseSessionSupport()
    const {
  return EmeConfig::UnsupportedRule();
}

EmeFeatureSupport AndroidKeySystemInfo::GetPersistentStateSupport() const {
  return EmeFeatureSupport::ALWAYS_ENABLED;
}

EmeFeatureSupport AndroidKeySystemInfo::GetDistinctiveIdentifierSupport()
    const {
  return EmeFeatureSupport::ALWAYS_ENABLED;
}

}  // namespace cdm

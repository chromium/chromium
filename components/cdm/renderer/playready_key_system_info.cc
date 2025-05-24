// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/renderer/playready_key_system_info.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "components/cdm/common/buildflags.h"
#include "components/cdm/common/playready_cdm_common.h"

#if !BUILDFLAG(ENABLE_PLAYREADY)
#error This file should only be built when PlayReady is enabled.
#endif

using media::EmeConfig;
using media::EmeConfigRuleState;
using media::EmeFeatureSupport;
using media::EmeInitDataType;
using media::EmeMediaType;
using media::SupportedCodecs;

namespace cdm {

PlayReadyKeySystemInfo::PlayReadyKeySystemInfo(
    media::SupportedCodecs hw_secure_codecs,
    base::flat_set<media::EncryptionScheme> hw_secure_encryption_schemes)
    : hw_secure_codecs_(hw_secure_codecs),
      hw_secure_encryption_schemes_(std::move(hw_secure_encryption_schemes)) {}

PlayReadyKeySystemInfo::~PlayReadyKeySystemInfo() = default;

std::string PlayReadyKeySystemInfo::GetBaseKeySystemName() const {
  return kPlayReadyKeySystemRecommendationDefault;
}

bool PlayReadyKeySystemInfo::IsSupportedKeySystem(
    const std::string& key_system) const {
  return IsPlayReadyKeySystem(key_system);
}

bool PlayReadyKeySystemInfo::ShouldUseBaseKeySystemName() const {
  return true;
}

bool PlayReadyKeySystemInfo::IsSupportedInitDataType(
    EmeInitDataType init_data_type) const {
  // Here we assume that support for a container imples support for the
  // associated initialization data type. KeySystems handles validating
  // |init_data_type| x |container| pairings.

  // To make KeySystemConfigSelector::GetSupportedConfiguration work correctly,
  // use the hardware secure codecs since there are no supported software codecs
  // in Chromium. If software secure codecs (aka. codecs_) is used here when
  // the keysystem is "com.microsoft.playready.recommendation", then this will
  // always return false which is not correct when robustness=3000.
  const media::SupportedCodecs codecs = hw_secure_codecs_;

  if (init_data_type == EmeInitDataType::WEBM) {
    return (codecs & media::EME_CODEC_WEBM_ALL) != 0;
  }
  if (init_data_type == EmeInitDataType::CENC) {
    return (codecs & media::EME_CODEC_MP4_ALL) != 0;
  }
  if (init_data_type == EmeInitDataType::KEYIDS) {
    return true;
  }

  return false;
}

EmeConfig::Rule PlayReadyKeySystemInfo::GetEncryptionSchemeConfigRule(
    media::EncryptionScheme encryption_scheme) const {
  if (hw_secure_encryption_schemes_.count(encryption_scheme)) {
    return EmeConfig{.hw_secure_codecs = EmeConfigRuleState::kRequired};
  }

  return EmeConfig::UnsupportedRule();
}

SupportedCodecs PlayReadyKeySystemInfo::GetSupportedCodecs() const {
  return media::EME_CODEC_NONE;
}

SupportedCodecs PlayReadyKeySystemInfo::GetSupportedHwSecureCodecs() const {
  return hw_secure_codecs_;
}

// `hw_secure_requirement` is not used here because the
// implementation only supports hardware secure PlayReady
// key systems. Software secure is not supported.
EmeConfig::Rule PlayReadyKeySystemInfo::GetRobustnessConfigRule(
    const std::string& key_system,
    EmeMediaType media_type,
    const std::string& requested_robustness,
    const bool* hw_secure_requirement) const {
  if (IsPlayReadyHwSecureKeySystem(key_system) &&
      (requested_robustness.empty() || requested_robustness == "3000")) {
    return EmeConfig{.hw_secure_codecs = EmeConfigRuleState::kRequired};
  }

  // Passing the robustness value of "3000" with the recommendation
  // key system also implies hardware secure PlayReady.
  if (key_system == kPlayReadyKeySystemRecommendationDefault &&
      requested_robustness == "3000") {
    return EmeConfig{.hw_secure_codecs = EmeConfigRuleState::kRequired};
  }

  // Software secure PlayReady is not supported in Chromium.
  return EmeConfig::UnsupportedRule();
}

EmeConfig::Rule PlayReadyKeySystemInfo::GetPersistentLicenseSessionSupport()
    const {
  return EmeConfig::UnsupportedRule();
}

EmeFeatureSupport PlayReadyKeySystemInfo::GetPersistentStateSupport() const {
  return EmeFeatureSupport::ALWAYS_ENABLED;
}

EmeFeatureSupport PlayReadyKeySystemInfo::GetDistinctiveIdentifierSupport()
    const {
  return EmeFeatureSupport::ALWAYS_ENABLED;
}

}  // namespace cdm

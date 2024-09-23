// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_RENDERER_EXTERNAL_CLEAR_KEY_KEY_SYSTEM_INFO_H_
#define COMPONENTS_CDM_RENDERER_EXTERNAL_CLEAR_KEY_KEY_SYSTEM_INFO_H_

#include <string>
#include <vector>

#include "build/build_config.h"
#include "media/base/key_system_info.h"
#include "media/media_buildflags.h"

namespace cdm {

// KeySystemInfo implementation for external Clear Key key systems.
class ExternalClearKeyKeySystemInfo : public media::KeySystemInfo {
 public:
  ExternalClearKeyKeySystemInfo();
  ExternalClearKeyKeySystemInfo(
      const std::string& key_system,
      std::vector<std::string> excluded_key_systems,
      media::SupportedCodecs codecs,
      media::EmeConfig::Rule eme_config_rule,
      media::EmeFeatureSupport persistent_state_support,
      media::EmeFeatureSupport distinctive_identifier_support);
  ~ExternalClearKeyKeySystemInfo() override;

  std::string GetBaseKeySystemName() const override;
  bool IsSupportedKeySystem(const std::string& key_system) const override;
  bool IsSupportedInitDataType(
      media::EmeInitDataType init_data_type) const override;
  media::EmeConfig::Rule GetEncryptionSchemeConfigRule(
      media::EncryptionScheme encryption_scheme) const override;
  media::SupportedCodecs GetSupportedCodecs() const override;
  media::SupportedCodecs GetSupportedHwSecureCodecs() const override;
  media::EmeConfig::Rule GetRobustnessConfigRule(
      const std::string& key_system,
      media::EmeMediaType media_type,
      const std::string& requested_robustness,
      const bool* hw_secure_requirement) const override;
  media::EmeConfig::Rule GetPersistentLicenseSessionSupport() const override;
  media::EmeFeatureSupport GetPersistentStateSupport() const override;
  media::EmeFeatureSupport GetDistinctiveIdentifierSupport() const override;

 private:
  const std::string key_system_ = std::string();
  const std::vector<std::string> excluded_key_systems_ = {};
  const media::SupportedCodecs codecs_ = media::EME_CODEC_NONE;
  const media::EmeConfig::Rule eme_config_rule_ =
      media::EmeConfig::UnsupportedRule();
  const media::EmeFeatureSupport persistent_state_support_ =
      media::EmeFeatureSupport::NOT_SUPPORTED;
  const media::EmeFeatureSupport distinctive_identifier_support_ =
      media::EmeFeatureSupport::NOT_SUPPORTED;
};

}  // namespace cdm

#endif  // COMPONENTS_CDM_RENDERER_EXTERNAL_CLEAR_KEY_KEY_SYSTEM_INFO_H_

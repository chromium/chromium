// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_RENDERER_EXTERNAL_CLEAR_KEY_KEY_SYSTEM_INFO_H_
#define COMPONENTS_CDM_RENDERER_EXTERNAL_CLEAR_KEY_KEY_SYSTEM_INFO_H_

#include <string>

#include "build/build_config.h"
#include "media/base/key_system_info.h"
#include "media/media_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cdm {

// KeySystemInfo implementation for external Clear Key key systems.
class ExternalClearKeySystemInfo : public media::KeySystemInfo {
 public:
  ExternalClearKeySystemInfo();
  ~ExternalClearKeySystemInfo() override;

  std::string GetBaseKeySystemName() const override;
  bool IsSupportedKeySystem(const std::string& key_system) const override;
  bool IsSupportedInitDataType(
      media::EmeInitDataType init_data_type) const override;
  media::EmeConfig::Rule GetEncryptionSchemeConfigRule(
      media::EncryptionScheme encryption_scheme) const override;
  media::SupportedCodecs GetSupportedCodecs() const override;
  media::EmeConfig::Rule GetRobustnessConfigRule(
      const std::string& key_system,
      media::EmeMediaType media_type,
      const std::string& requested_robustness,
      const bool* hw_secure_requirement) const override;
  media::EmeConfig::Rule GetPersistentLicenseSessionSupport() const override;
  media::EmeFeatureSupport GetPersistentStateSupport() const override;
  media::EmeFeatureSupport GetDistinctiveIdentifierSupport() const override;
};

}  // namespace cdm

#endif  // COMPONENTS_CDM_RENDERER_EXTERNAL_CLEAR_KEY_KEY_SYSTEM_INFO_H_

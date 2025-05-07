// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_RENDERER_PLAYREADY_KEY_SYSTEM_INFO_H_
#define COMPONENTS_CDM_RENDERER_PLAYREADY_KEY_SYSTEM_INFO_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "media/base/key_system_info.h"

namespace cdm {

// Implementation of KeySystemInfo for all PlayReady key systems.
class PlayReadyKeySystemInfo : public media::KeySystemInfo {
 public:
  PlayReadyKeySystemInfo(
      media::SupportedCodecs hw_secure_codecs,
      base::flat_set<media::EncryptionScheme> hw_secure_encryption_schemes);
  ~PlayReadyKeySystemInfo() override;

  std::string GetBaseKeySystemName() const override;
  bool IsSupportedKeySystem(const std::string& key_system) const override;
  bool ShouldUseBaseKeySystemName() const override;
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
  media::EmeConfig GetPersistentUsageRecordSessionSupport() const;
  media::EmeFeatureSupport GetPersistentStateSupport() const override;
  media::EmeFeatureSupport GetDistinctiveIdentifierSupport() const override;

 private:
  const media::SupportedCodecs hw_secure_codecs_;
  const base::flat_set<media::EncryptionScheme> hw_secure_encryption_schemes_;
};

}  // namespace cdm

#endif  // COMPONENTS_CDM_RENDERER_PLAYREADY_KEY_SYSTEM_INFO_H_

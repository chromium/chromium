// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_RENDERER_ANDROID_KEY_SYSTEM_INFO_H_
#define COMPONENTS_CDM_RENDERER_ANDROID_KEY_SYSTEM_INFO_H_

#include <string>

#include "base/containers/flat_set.h"
#include "media/base/eme_constants.h"
#include "media/base/encryption_scheme.h"
#include "media/base/key_system_info.h"

namespace cdm {

// Implementation of KeySystemInfo for Android (non-Widevine) key systems.
// Assumes that platform key systems support no features but can and will
// make use of persistence and identifiers.
class AndroidKeySystemInfo : public media::KeySystemInfo {
 public:
  AndroidKeySystemInfo(
      const std::string& name,
      media::SupportedCodecs sw_secure_codecs,
      base::flat_set<media::EncryptionScheme> sw_secure_encryption_schemes,
      media::SupportedCodecs hw_secure_codecs,
      base::flat_set<media::EncryptionScheme> hw_secure_encryption_schemes);
  ~AndroidKeySystemInfo() override;

  std::string GetBaseKeySystemName() const override;
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
  const std::string name_;
  const media::SupportedCodecs sw_secure_codecs_;
  const base::flat_set<media::EncryptionScheme> sw_secure_encryption_schemes_;
  const media::SupportedCodecs hw_secure_codecs_;
  const base::flat_set<media::EncryptionScheme> hw_secure_encryption_schemes_;
};

}  // namespace cdm

#endif  // COMPONENTS_CDM_RENDERER_ANDROID_KEY_SYSTEM_INFO_H_

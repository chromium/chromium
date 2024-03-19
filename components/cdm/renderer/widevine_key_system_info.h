// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_RENDERER_WIDEVINE_KEY_SYSTEM_INFO_H_
#define COMPONENTS_CDM_RENDERER_WIDEVINE_KEY_SYSTEM_INFO_H_

#include <string>

#include "base/containers/flat_set.h"
#include "media/base/content_decryption_module.h"
#include "media/base/key_system_info.h"

namespace cdm {

// Implementation of KeySystemInfo for Widevine key system.
class WidevineKeySystemInfo : public media::KeySystemInfo {
 public:
  // Robustness values understood by the Widevine key system.
  // Note: GetRobustnessConfigRule is dependent on the order of these.
  enum class Robustness {
    INVALID,
    EMPTY,
    SW_SECURE_CRYPTO,
    SW_SECURE_DECODE,
    HW_SECURE_CRYPTO,
    HW_SECURE_DECODE,
    HW_SECURE_ALL,
  };

  WidevineKeySystemInfo(
      media::SupportedCodecs codecs,
      base::flat_set<media::EncryptionScheme> encryption_schemes,
      base::flat_set<media::CdmSessionType> session_types,
      media::SupportedCodecs hw_secure_codecs,
      base::flat_set<media::EncryptionScheme> hw_secure_encryption_schemes,
      base::flat_set<media::CdmSessionType> hw_secure_session_types,
      Robustness max_audio_robustness,
      Robustness max_video_robustness,
      media::EmeFeatureSupport persistent_state_support,
      media::EmeFeatureSupport distinctive_identifier_support);
  ~WidevineKeySystemInfo() override;

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
  media::EmeFeatureSupport GetPersistentStateSupport() const override;
  media::EmeFeatureSupport GetDistinctiveIdentifierSupport() const override;

#if BUILDFLAG(IS_WIN)
  // `is_experimental_` is used to differentiate between
  // `kWidevineExperimentKeySystem` and `kWidevineKeySystem`. Please refer to
  // b/219818166 for more information.
  void set_experimental() { is_experimental_ = true; }

  // `is_experimental_two_` is used to differentiate between
  // `kWidevineExperiment2KeySystem` and `kWidevineKeySystem`. Please refer to
  // b/268749229 for more information.
  void set_experimental_two() { is_experimental_two_ = true; }
#endif  // BUILDFLAG(IS_WIN)

 private:
  const media::SupportedCodecs codecs_;
  const base::flat_set<media::EncryptionScheme> encryption_schemes_;
  const base::flat_set<media::CdmSessionType> session_types_;
  const media::SupportedCodecs hw_secure_codecs_;
  const base::flat_set<media::EncryptionScheme> hw_secure_encryption_schemes_;
  const base::flat_set<media::CdmSessionType> hw_secure_session_types_;
  const Robustness max_audio_robustness_;
  const Robustness max_video_robustness_;
  const media::EmeFeatureSupport persistent_state_support_;
  const media::EmeFeatureSupport distinctive_identifier_support_;
#if BUILDFLAG(IS_WIN)
  bool is_experimental_ = false;
  bool is_experimental_two_ = false;
#endif  // BUILDFLAG(IS_WIN)
};

}  // namespace cdm

#endif  // COMPONENTS_CDM_RENDERER_WIDEVINE_KEY_SYSTEM_INFO_H_

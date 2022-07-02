// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/renderer/widevine_key_system_properties.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

#if !BUILDFLAG(ENABLE_WIDEVINE)
#error This file should only be built when Widevine is enabled.
#endif

using media::CdmSessionType;
using media::EmeConfigRule;
using media::EmeFeatureSupport;
using media::EmeInitDataType;
using media::EmeMediaType;
using media::EncryptionScheme;
using media::SupportedCodecs;
using Robustness = cdm::WidevineKeySystemProperties::Robustness;

namespace cdm {
namespace {

Robustness ConvertRobustness(const std::string& robustness) {
  if (robustness.empty())
    return Robustness::EMPTY;
  if (robustness == "SW_SECURE_CRYPTO")
    return Robustness::SW_SECURE_CRYPTO;
  if (robustness == "SW_SECURE_DECODE")
    return Robustness::SW_SECURE_DECODE;
  if (robustness == "HW_SECURE_CRYPTO")
    return Robustness::HW_SECURE_CRYPTO;
  if (robustness == "HW_SECURE_DECODE")
    return Robustness::HW_SECURE_DECODE;
  if (robustness == "HW_SECURE_ALL")
    return Robustness::HW_SECURE_ALL;
  return Robustness::INVALID;
}

#if BUILDFLAG(IS_WIN)
bool IsHardwareSecurityEnabledForKeySystem(const std::string& key_system) {
  return (key_system == kWidevineKeySystem &&
          base::FeatureList::IsEnabled(media::kHardwareSecureDecryption)) ||
         (key_system == kWidevineExperimentKeySystem &&
          base::FeatureList::IsEnabled(
              media::kHardwareSecureDecryptionExperiment));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

WidevineKeySystemProperties::WidevineKeySystemProperties(
    SupportedCodecs codecs,
    base::flat_set<EncryptionScheme> encryption_schemes,
    base::flat_set<CdmSessionType> session_types,
    SupportedCodecs hw_secure_codecs,
    base::flat_set<EncryptionScheme> hw_secure_encryption_schemes,
    base::flat_set<CdmSessionType> hw_secure_session_types,
    Robustness max_audio_robustness,
    Robustness max_video_robustness,
    EmeFeatureSupport persistent_state_support,
    EmeFeatureSupport distinctive_identifier_support)
    : codecs_(codecs),
      encryption_schemes_(std::move(encryption_schemes)),
      session_types_(std::move(session_types)),
      hw_secure_codecs_(hw_secure_codecs),
      hw_secure_encryption_schemes_(std::move(hw_secure_encryption_schemes)),
      hw_secure_session_types_(std::move(hw_secure_session_types)),
      max_audio_robustness_(max_audio_robustness),
      max_video_robustness_(max_video_robustness),
      persistent_state_support_(persistent_state_support),
      distinctive_identifier_support_(distinctive_identifier_support) {}

WidevineKeySystemProperties::~WidevineKeySystemProperties() = default;

std::string WidevineKeySystemProperties::GetBaseKeySystemName() const {
  return kWidevineKeySystem;
}

bool WidevineKeySystemProperties::IsSupportedKeySystem(
    const std::string& key_system) const {
#if BUILDFLAG(IS_WIN)
  if (key_system == kWidevineExperimentKeySystem &&
      base::FeatureList::IsEnabled(
          media::kHardwareSecureDecryptionExperiment)) {
    return true;
  }
#endif  // BUILDFLAG(IS_WIN)

  return key_system == kWidevineKeySystem;
}

bool WidevineKeySystemProperties::ShouldUseBaseKeySystemName() const {
  // Internally Widevine CDM only supports kWidevineKeySystem.
  return true;
}

bool WidevineKeySystemProperties::IsSupportedInitDataType(
    EmeInitDataType init_data_type) const {
  // Here we assume that support for a container implies support for the
  // associated initialization data type. KeySystems handles validating
  // |init_data_type| x |container| pairings.
  if (init_data_type == EmeInitDataType::WEBM)
    return (codecs_ & media::EME_CODEC_WEBM_ALL) != 0;
  if (init_data_type == EmeInitDataType::CENC)
    return (codecs_ & media::EME_CODEC_MP4_ALL) != 0;

  return false;
}

EmeConfigRule WidevineKeySystemProperties::GetEncryptionSchemeConfigRule(
    EncryptionScheme encryption_scheme) const {
  bool is_supported = encryption_schemes_.contains(encryption_scheme);
  bool is_hw_secure_supported =
      hw_secure_encryption_schemes_.contains(encryption_scheme);

  if (is_supported && is_hw_secure_supported)
    return EmeConfigRule::SUPPORTED;
  else if (is_supported && !is_hw_secure_supported)
    return EmeConfigRule::HW_SECURE_CODECS_NOT_ALLOWED;
  else if (!is_supported && is_hw_secure_supported)
    return EmeConfigRule::HW_SECURE_CODECS_REQUIRED;
  else
    return EmeConfigRule::NOT_SUPPORTED;
}

SupportedCodecs WidevineKeySystemProperties::GetSupportedCodecs() const {
  return codecs_;
}

SupportedCodecs WidevineKeySystemProperties::GetSupportedHwSecureCodecs()
    const {
  return hw_secure_codecs_;
}

EmeConfigRule WidevineKeySystemProperties::GetRobustnessConfigRule(
    const std::string& key_system,
    EmeMediaType media_type,
    const std::string& requested_robustness,
    const bool* hw_secure_requirement) const {
  Robustness robustness = ConvertRobustness(requested_robustness);
  if (robustness == Robustness::INVALID)
    return EmeConfigRule::NOT_SUPPORTED;

  Robustness max_robustness = Robustness::INVALID;
  switch (media_type) {
    case EmeMediaType::AUDIO:
      max_robustness = max_audio_robustness_;
      break;
    case EmeMediaType::VIDEO:
      max_robustness = max_video_robustness_;
      break;
  }

  // We can compare robustness levels whenever they are not HW_SECURE_CRYPTO
  // and SW_SECURE_DECODE in some order. If they are exactly those two then the
  // robustness requirement is not supported.
  if ((max_robustness == Robustness::HW_SECURE_CRYPTO &&
       robustness == Robustness::SW_SECURE_DECODE) ||
      (max_robustness == Robustness::SW_SECURE_DECODE &&
       robustness == Robustness::HW_SECURE_CRYPTO) ||
      robustness > max_robustness) {
    return EmeConfigRule::NOT_SUPPORTED;
  }

  [[maybe_unused]] bool hw_secure_codecs_required =
      hw_secure_requirement && *hw_secure_requirement;

#if BUILDFLAG(IS_CHROMEOS)
  // Hardware security requires HWDRM or remote attestation, both of these
  // require an identifier.
  if (robustness >= Robustness::HW_SECURE_CRYPTO || hw_secure_codecs_required) {
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kLacrosUseChromeosProtectedMedia)) {
      return EmeConfigRule::IDENTIFIER_REQUIRED;
    }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    return EmeConfigRule::IDENTIFIER_AND_HW_SECURE_CODECS_REQUIRED;
#else
    return EmeConfigRule::IDENTIFIER_REQUIRED;
#endif
  }

  // For video, recommend remote attestation if HW_SECURE_ALL is available,
  // regardless of the value of |robustness|, because it enables hardware
  // accelerated decoding.
  // TODO(sandersd): Only do this when hardware accelerated decoding is
  // available for the requested codecs.
  if (media_type == EmeMediaType::VIDEO &&
      max_robustness == Robustness::HW_SECURE_ALL) {
    return EmeConfigRule::IDENTIFIER_RECOMMENDED;
  }
#elif BUILDFLAG(IS_ANDROID)
  // On Android, require hardware secure codecs for SW_SECURE_DECODE and above.
  if (robustness >= Robustness::SW_SECURE_DECODE || hw_secure_codecs_required)
    return EmeConfigRule::HW_SECURE_CODECS_REQUIRED;
#elif BUILDFLAG(IS_WIN)
  if (robustness >= Robustness::HW_SECURE_CRYPTO) {
    // On Windows, hardware security uses MediaFoundation-based CDM which
    // requires identifier and persistent state.
    return IsHardwareSecurityEnabledForKeySystem(key_system)
               ? EmeConfigRule::
                     IDENTIFIER_PERSISTENCE_AND_HW_SECURE_CODECS_REQUIRED
               : EmeConfigRule::NOT_SUPPORTED;
  } else if (robustness < Robustness::HW_SECURE_CRYPTO) {
    // On Windows, when software security is queried, explicitly not allow
    // hardware secure codecs to prevent robustness level upgrade, for stability
    // and compatibility reasons. See https://crbug.com/1327043.
    return EmeConfigRule::HW_SECURE_CODECS_NOT_ALLOWED;
  }
#else
  // On other platforms, require hardware secure codecs for HW_SECURE_CRYPTO and
  // above.
  if (robustness >= Robustness::HW_SECURE_CRYPTO)
    return EmeConfigRule::HW_SECURE_CODECS_REQUIRED;
#endif  // BUILDFLAG(IS_CHROMEOS)

  return EmeConfigRule::SUPPORTED;
}

EmeConfigRule WidevineKeySystemProperties::GetPersistentLicenseSessionSupport()
    const {
  bool is_supported =
      session_types_.contains(CdmSessionType::kPersistentLicense);

#if BUILDFLAG(IS_CHROMEOS)
  // The logic around hardware/software security support is complicated on
  // ChromeOS. This code is to preserve the original logic, by deciding the
  // support only based on `is_supported` and ignore `is_hw_secure_supported`.
  // Note: On ChromeOS, platform verification (similar to CDM host verification)
  // is required for persistent license support, which requires identifier.
  // TODO(crbug.com/1324262): Fix the logic after refactoring EmeConfigRule.
  return is_supported ? EmeConfigRule::IDENTIFIER_AND_PERSISTENCE_REQUIRED
                      : EmeConfigRule::NOT_SUPPORTED;
#else   // BUILDFLAG(IS_CHROMEOS)
  bool is_hw_secure_supported =
      hw_secure_session_types_.contains(CdmSessionType::kPersistentLicense);

  // Per GetPersistentLicenseSessionSupport() API, there's no need to specify
  // the PERSISTENCE requirement here, which is implicitly assumed and enforced
  // by `KeySystemConfigSelector`.
  if (is_supported && is_hw_secure_supported) {
    return EmeConfigRule::SUPPORTED;
  } else if (is_supported && !is_hw_secure_supported) {
    return EmeConfigRule::HW_SECURE_CODECS_NOT_ALLOWED;
  } else if (!is_supported && is_hw_secure_supported) {
    return EmeConfigRule::HW_SECURE_CODECS_REQUIRED;
  } else {
    return EmeConfigRule::NOT_SUPPORTED;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

EmeFeatureSupport WidevineKeySystemProperties::GetPersistentStateSupport()
    const {
  return persistent_state_support_;
}

EmeFeatureSupport WidevineKeySystemProperties::GetDistinctiveIdentifierSupport()
    const {
  return distinctive_identifier_support_;
}

}  // namespace cdm

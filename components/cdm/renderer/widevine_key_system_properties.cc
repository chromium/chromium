// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/renderer/widevine_key_system_properties.h"

#include "base/feature_list.h"
#include "media/base/media_switches.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

#if !BUILDFLAG(ENABLE_WIDEVINE)
#error This file should only be built when Widevine is enabled.
#endif

using media::EmeConfigRule;
using media::EmeFeatureSupport;
using media::EmeInitDataType;
using media::EmeMediaType;
using media::EmeSessionTypeSupport;
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

}  // namespace

WidevineKeySystemProperties::WidevineKeySystemProperties(
    media::SupportedCodecs codecs,
    base::flat_set<media::EncryptionScheme> encryption_schemes,
    media::SupportedCodecs hw_secure_codecs,
    base::flat_set<media::EncryptionScheme> hw_secure_encryption_schemes,
    Robustness max_audio_robustness,
    Robustness max_video_robustness,
    media::EmeSessionTypeSupport persistent_license_support,
    media::EmeSessionTypeSupport persistent_release_message_support,
    media::EmeFeatureSupport persistent_state_support,
    media::EmeFeatureSupport distinctive_identifier_support)
    : codecs_(codecs),
      encryption_schemes_(std::move(encryption_schemes)),
      hw_secure_codecs_(hw_secure_codecs),
      hw_secure_encryption_schemes_(std::move(hw_secure_encryption_schemes)),
      max_audio_robustness_(max_audio_robustness),
      max_video_robustness_(max_video_robustness),
      persistent_license_support_(persistent_license_support),
      persistent_release_message_support_(persistent_release_message_support),
      persistent_state_support_(persistent_state_support),
      distinctive_identifier_support_(distinctive_identifier_support) {}

WidevineKeySystemProperties::~WidevineKeySystemProperties() = default;

std::string WidevineKeySystemProperties::GetKeySystemName() const {
  return kWidevineKeySystem;
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
    media::EncryptionScheme encryption_scheme) const {
  bool is_supported = encryption_schemes_.count(encryption_scheme);
  bool is_hw_secure_supported =
      hw_secure_encryption_schemes_.count(encryption_scheme);

  if (is_supported && is_hw_secure_supported)
    return EmeConfigRule::SUPPORTED;
  else if (is_supported && !is_hw_secure_supported)
    return EmeConfigRule::HW_SECURE_CODECS_NOT_ALLOWED;
  else if (!is_supported && is_hw_secure_supported)
    return EmeConfigRule::HW_SECURE_CODECS_REQUIRED;
  else
    return EmeConfigRule::NOT_SUPPORTED;
}

static SupportedCodecs OverrideAv1SupportIfNeeded(SupportedCodecs codecs) {
  auto result = codecs;

  // Enable AV1 if force-support is enabled.
  if (base::FeatureList::IsEnabled(media::kWidevineAv1ForceSupportForTesting))
    result |= media::EME_CODEC_AV1;

  // Disable AV1 if the master switch kWidevineAv1 is disabled.
  if (!base::FeatureList::IsEnabled(media::kWidevineAv1))
    result &= ~media::EME_CODEC_AV1;

  return result;
}

SupportedCodecs WidevineKeySystemProperties::GetSupportedCodecs() const {
  return OverrideAv1SupportIfNeeded(codecs_);
}

SupportedCodecs WidevineKeySystemProperties::GetSupportedHwSecureCodecs()
    const {
  return OverrideAv1SupportIfNeeded(hw_secure_codecs_);
}

EmeConfigRule WidevineKeySystemProperties::GetRobustnessConfigRule(
    EmeMediaType media_type,
    const std::string& requested_robustness) const {
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

#if defined(OS_CHROMEOS)
  // Hardware security requires remote attestation.
  if (robustness >= Robustness::HW_SECURE_CRYPTO)
    return EmeConfigRule::IDENTIFIER_REQUIRED;

  // For video, recommend remote attestation if HW_SECURE_ALL is available,
  // regardless of the value of |robustness|, because it enables hardware
  // accelerated decoding.
  // TODO(sandersd): Only do this when hardware accelerated decoding is
  // available for the requested codecs.
  if (media_type == EmeMediaType::VIDEO &&
      max_robustness == Robustness::HW_SECURE_ALL) {
    return EmeConfigRule::IDENTIFIER_RECOMMENDED;
  }
#elif defined(OS_ANDROID)
  // On Android, require hardware secure codecs for SW_SECURE_DECODE and above.
  if (robustness >= Robustness::SW_SECURE_DECODE) {
    return EmeConfigRule::HW_SECURE_CODECS_REQUIRED;
  }
#else
  // On Linux/Mac/Win, require hardware secure codecs for HW_SECURE_CRYPTO and
  // above.
  if (robustness >= Robustness::HW_SECURE_CRYPTO) {
    return EmeConfigRule::HW_SECURE_CODECS_REQUIRED;
  }
#endif  // defined(OS_CHROMEOS)

  // TODO(crbug.com/848532): Handle HW_SECURE* levels for Windows.
  return EmeConfigRule::SUPPORTED;
}

EmeSessionTypeSupport
WidevineKeySystemProperties::GetPersistentLicenseSessionSupport() const {
  return persistent_license_support_;
}

EmeSessionTypeSupport
WidevineKeySystemProperties::GetPersistentUsageRecordSessionSupport() const {
  return persistent_release_message_support_;
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

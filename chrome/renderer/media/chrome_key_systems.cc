// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_key_systems.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "components/cdm/renderer/external_clear_key_key_system_properties.h"
#include "components/cdm/renderer/widevine_key_system_properties.h"
#include "content/public/renderer/render_thread.h"
#include "media/base/decrypt_config.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_properties.h"
#include "media/cdm/cdm_capability.h"
#include "media/media_buildflags.h"
#include "third_party/widevine/cdm/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/cdm/renderer/android_key_systems.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS) || BUILDFLAG(IS_WIN)
#include "base/feature_list.h"
#include "content/public/renderer/key_system_support.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#if BUILDFLAG(ENABLE_WIDEVINE)
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
#if BUILDFLAG(ENABLE_PLATFORM_HEVC) && BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) && BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(ENABLE_WIDEVINE)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS) || BUILDFLAG(IS_WIN)

using media::EmeFeatureSupport;
using media::EmeSessionTypeSupport;
using media::KeySystemProperties;
using media::KeySystemPropertiesVector;
using media::SupportedCodecs;

namespace {

#if BUILDFLAG(ENABLE_LIBRARY_CDMS) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_WIDEVINE)
SupportedCodecs GetVP9Codecs(
    const std::vector<media::VideoCodecProfile>& profiles) {
  if (profiles.empty()) {
    // If no profiles are specified, then all are supported.
    return media::EME_CODEC_VP9_PROFILE0 | media::EME_CODEC_VP9_PROFILE2;
  }

  SupportedCodecs supported_vp9_codecs = media::EME_CODEC_NONE;
  for (const auto& profile : profiles) {
    switch (profile) {
      case media::VP9PROFILE_PROFILE0:
        supported_vp9_codecs |= media::EME_CODEC_VP9_PROFILE0;
        break;
      case media::VP9PROFILE_PROFILE2:
        supported_vp9_codecs |= media::EME_CODEC_VP9_PROFILE2;
        break;
      default:
        DVLOG(1) << "Unexpected " << GetCodecName(media::VideoCodec::kVP9)
                 << " profile: " << GetProfileName(profile);
        break;
    }
  }

  return supported_vp9_codecs;
}

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
SupportedCodecs GetHevcCodecs(
    const std::vector<media::VideoCodecProfile>& profiles) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLacrosEnablePlatformHevc)) {
    return media::EME_CODEC_NONE;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // If no profiles are specified, then all are supported.
  if (profiles.empty()) {
    return media::EME_CODEC_HEVC_PROFILE_MAIN |
           media::EME_CODEC_HEVC_PROFILE_MAIN10;
  }

  SupportedCodecs supported_hevc_codecs = media::EME_CODEC_NONE;
  for (const auto& profile : profiles) {
    switch (profile) {
      case media::HEVCPROFILE_MAIN:
        supported_hevc_codecs |= media::EME_CODEC_HEVC_PROFILE_MAIN;
        break;
      case media::HEVCPROFILE_MAIN10:
        supported_hevc_codecs |= media::EME_CODEC_HEVC_PROFILE_MAIN10;
        break;
      default:
        DVLOG(1) << "Unexpected " << GetCodecName(media::VideoCodec::kHEVC)
                 << " profile: " << GetProfileName(profile);
        break;
    }
  }

  return supported_hevc_codecs;
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)

SupportedCodecs GetSupportedCodecs(const media::CdmCapability& capability) {
  SupportedCodecs supported_codecs = media::EME_CODEC_NONE;

  for (const auto& codec : capability.audio_codecs) {
    switch (codec) {
      case media::AudioCodec::kOpus:
        supported_codecs |= media::EME_CODEC_OPUS;
        break;
      case media::AudioCodec::kVorbis:
        supported_codecs |= media::EME_CODEC_VORBIS;
        break;
      case media::AudioCodec::kFLAC:
        supported_codecs |= media::EME_CODEC_FLAC;
        break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      case media::AudioCodec::kAAC:
        supported_codecs |= media::EME_CODEC_AAC;
        break;
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
      case media::AudioCodec::kDTS:
        supported_codecs |= media::EME_CODEC_DTS;
        supported_codecs |= media::EME_CODEC_DTSXP2;
        break;
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
      default:
        DVLOG(1) << "Unexpected supported codec: " << GetCodecName(codec);
        break;
    }
  }

  // For compatibility with older CDMs different profiles are only used
  // with some video codecs.
  for (const auto& codec : capability.video_codecs) {
    switch (codec.first) {
      case media::VideoCodec::kVP8:
        supported_codecs |= media::EME_CODEC_VP8;
        break;
      case media::VideoCodec::kVP9:
        supported_codecs |= GetVP9Codecs(codec.second);
        break;
      case media::VideoCodec::kAV1:
        supported_codecs |= media::EME_CODEC_AV1;
        break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      case media::VideoCodec::kH264:
        supported_codecs |= media::EME_CODEC_AVC1;
        break;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
      case media::VideoCodec::kHEVC:
        supported_codecs |= GetHevcCodecs(codec.second);
        break;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
      default:
        DVLOG(1) << "Unexpected supported codec: " << GetCodecName(codec.first);
        break;
    }
  }

  return supported_codecs;
}

// Returns persistent-license session support.
EmeSessionTypeSupport GetPersistentLicenseSupport(bool supported_by_the_cdm) {
  // Do not support persistent-license if the process cannot persist data.
  // TODO(crbug.com/457487): Have a better plan on this. See bug for details.
  if (ChromeRenderThreadObserver::is_incognito_process()) {
    DVLOG(2) << __func__ << ": Not supported in incognito process.";
    return EmeSessionTypeSupport::NOT_SUPPORTED;
  }

  if (!supported_by_the_cdm) {
    DVLOG(2) << __func__ << ": Not supported by the CDM.";
    return EmeSessionTypeSupport::NOT_SUPPORTED;
  }

// On ChromeOS, platform verification is similar to CDM host verification.
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION) || BUILDFLAG(IS_CHROMEOS)
  bool cdm_host_verification_potentially_supported = true;
#else
  bool cdm_host_verification_potentially_supported = false;
#endif

  // If we are sure CDM host verification is NOT supported, we should not
  // support persistent-license.
  if (!cdm_host_verification_potentially_supported) {
    DVLOG(2) << __func__ << ": Not supported without CDM host verification.";
    return EmeSessionTypeSupport::NOT_SUPPORTED;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, platform verification (similar to CDM host verification)
  // requires identifier to be allowed.
  // TODO(jrummell): Currently the ChromeOS CDM does not require storage ID
  // to support persistent license. Update this logic when the new CDM requires
  // storage ID.
  return EmeSessionTypeSupport::SUPPORTED_WITH_IDENTIFIER;
#elif BUILDFLAG(ENABLE_CDM_STORAGE_ID)
  // On other platforms, we require storage ID to support persistent license.
  return EmeSessionTypeSupport::SUPPORTED;
#else
  // Storage ID not implemented, so no support for persistent license.
  DVLOG(2) << __func__ << ": Not supported without CDM storage ID.";
  return EmeSessionTypeSupport::NOT_SUPPORTED;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool AddWidevine(const media::mojom::KeySystemCapabilityPtr& capability,
                 KeySystemPropertiesVector* key_systems) {
  // Codecs and encryption schemes.
  SupportedCodecs codecs = media::EME_CODEC_NONE;
  SupportedCodecs hw_secure_codecs = media::EME_CODEC_NONE;
  base::flat_set<::media::EncryptionScheme> encryption_schemes;
  base::flat_set<::media::EncryptionScheme> hw_secure_encryption_schemes;
  bool cdm_supports_persistent_license = false;

  if (capability->sw_secure_capability) {
    codecs = GetSupportedCodecs(capability->sw_secure_capability.value());
    encryption_schemes = capability->sw_secure_capability->encryption_schemes;
    if (!base::Contains(capability->sw_secure_capability->session_types,
                        media::CdmSessionType::kTemporary)) {
      DVLOG(1) << "Temporary sessions must be supported.";
      return false;
    }

    cdm_supports_persistent_license =
        base::Contains(capability->sw_secure_capability->session_types,
                       media::CdmSessionType::kPersistentLicense);

    DVLOG(2) << "Software secure Widevine supported";
  }

  if (capability->hw_secure_capability) {
    hw_secure_codecs =
        GetSupportedCodecs(capability->hw_secure_capability.value());
    hw_secure_encryption_schemes =
        capability->hw_secure_capability->encryption_schemes;
    if (!base::Contains(capability->hw_secure_capability->session_types,
                        media::CdmSessionType::kTemporary)) {
      DVLOG(1) << "Temporary sessions must be supported.";
      return false;
    }

    // TODO(b/186035558): With a single flag we can't distinguish persistent
    // session support between software and hardware CDMs. This should be
    // fixed so that if there is both a software and a hardware CDM, persistent
    // session support can be different between the versions.
    DVLOG(2) << "Hardware secure Widevine supported";
  }

  // Robustness.
  using Robustness = cdm::WidevineKeySystemProperties::Robustness;
  auto max_audio_robustness = Robustness::SW_SECURE_CRYPTO;
  auto max_video_robustness = Robustness::SW_SECURE_DECODE;

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, we support HW_SECURE_ALL even without hardware secure codecs.
  // See WidevineKeySystemProperties::GetRobustnessConfigRule().
  max_audio_robustness = Robustness::HW_SECURE_ALL;
  max_video_robustness = Robustness::HW_SECURE_ALL;
#else
  if (media::IsHardwareSecureDecryptionEnabled()) {
    max_audio_robustness = Robustness::HW_SECURE_CRYPTO;
    max_video_robustness = Robustness::HW_SECURE_ALL;
  }
#endif

  auto persistent_license_support =
      GetPersistentLicenseSupport(cdm_supports_persistent_license);

  // Others.
  auto persistent_state_support = EmeFeatureSupport::REQUESTABLE;
  auto distinctive_identifier_support = EmeFeatureSupport::NOT_SUPPORTED;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  distinctive_identifier_support = EmeFeatureSupport::REQUESTABLE;
#endif

  key_systems->emplace_back(new cdm::WidevineKeySystemProperties(
      codecs, encryption_schemes, hw_secure_codecs,
      hw_secure_encryption_schemes, max_audio_robustness, max_video_robustness,
      persistent_license_support, persistent_state_support,
      distinctive_identifier_support));
  return true;
}
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

const char kExternalClearKeyKeySystem[] = "org.chromium.externalclearkey";

void AddExternalClearKey(
    const media::mojom::KeySystemCapabilityPtr& /*capability*/,
    KeySystemPropertiesVector* key_systems) {
  DVLOG(1) << __func__;

  if (!base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting)) {
    DLOG(ERROR) << "ExternalClearKey supported despite not enabled.";
    return;
  }

  // TODO(xhwang): Actually use `capability` to determine capabilities.
  key_systems->push_back(std::make_unique<cdm::ExternalClearKeyProperties>());
}

void OnKeySystemSupportUpdated(
    media::GetSupportedKeySystemsCB cb,
    content::KeySystemCapabilityPtrMap key_system_capabilities) {
  KeySystemPropertiesVector key_systems;
  for (const auto& entry : key_system_capabilities) {
    const auto& key_system = entry.first;
    const auto& capability = entry.second;
#if BUILDFLAG(ENABLE_WIDEVINE)
    if (key_system == kWidevineKeySystem) {
      AddWidevine(capability, &key_systems);
      continue;
    }
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

    if (key_system == kExternalClearKeyKeySystem) {
      AddExternalClearKey(capability, &key_systems);
      continue;
    }

    DLOG(ERROR) << "Unrecognized key system: " << key_system;
  }

  cb.Run(std::move(key_systems));
}

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS) || BUILDFLAG(IS_WIN)

}  // namespace

void GetChromeKeySystems(media::GetSupportedKeySystemsCB cb) {
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_WIDEVINE)
  KeySystemPropertiesVector key_systems;
  cdm::AddAndroidWidevine(&key_systems);
  std::move(cb).Run(std::move(key_systems));
  return;
#elif BUILDFLAG(ENABLE_LIBRARY_CDMS) || BUILDFLAG(IS_WIN)
  content::ObserveKeySystemSupportUpdate(
      base::BindRepeating(&OnKeySystemSupportUpdated, std::move(cb)));
  return;
#else
  std::move(cb).Run({});
  return;
#endif
}

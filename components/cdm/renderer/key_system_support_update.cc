// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/renderer/key_system_support_update.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/cdm/renderer/external_clear_key_key_system_info.h"
#include "content/public/renderer/key_system_support.h"
#include "content/public/renderer/render_frame.h"
#include "media/base/audio_codecs.h"
#include "media/base/cdm_capability.h"
#include "media/base/content_decryption_module.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_capability.h"
#include "media/base/key_system_info.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "media/media_buildflags.h"
#include "third_party/widevine/cdm/buildflags.h"

#if BUILDFLAG(ENABLE_WIDEVINE)
#include "components/cdm/renderer/widevine_key_system_info.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

#if BUILDFLAG(IS_ANDROID)
#include "components/cdm/renderer/android_key_system_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

using media::CdmSessionType;
using media::EmeFeatureSupport;
using media::KeySystemInfo;
using media::KeySystemInfos;
using media::SupportedCodecs;

namespace cdm {

namespace {

#if BUILDFLAG(ENABLE_WIDEVINE) || BUILDFLAG(IS_ANDROID)
SupportedCodecs GetVP9Codecs(
    const base::flat_set<media::VideoCodecProfile>& profiles) {
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
    const base::flat_set<media::VideoCodecProfile>& profiles) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLacrosEnablePlatformHevc)) {
    return media::EME_CODEC_NONE;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(media::kPlatformHEVCDecoderSupport)) {
    return media::EME_CODEC_NONE;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
// Dolby Vision HEVC-based profiles are only be supported when HEVC is enabled.
// However, this is enforced elsewhere, as DV profiles for both AVC and HEVC
// are returned here.
SupportedCodecs GetDolbyVisionCodecs(
    const base::flat_set<media::VideoCodecProfile>& profiles) {
  // If no profiles are specified, then all are supported.
  if (profiles.empty()) {
    return media::EME_CODEC_DOLBY_VISION_AVC |
           media::EME_CODEC_DOLBY_VISION_HEVC;
  }

  SupportedCodecs supported_dv_codecs = media::EME_CODEC_NONE;
  for (const auto& profile : profiles) {
    switch (profile) {
      case media::DOLBYVISION_PROFILE0:
        supported_dv_codecs |= media::EME_CODEC_DOLBY_VISION_PROFILE0;
        break;
      case media::DOLBYVISION_PROFILE5:
        supported_dv_codecs |= media::EME_CODEC_DOLBY_VISION_PROFILE5;
        break;
      case media::DOLBYVISION_PROFILE7:
        supported_dv_codecs |= media::EME_CODEC_DOLBY_VISION_PROFILE7;
        break;
      case media::DOLBYVISION_PROFILE8:
        supported_dv_codecs |= media::EME_CODEC_DOLBY_VISION_PROFILE8;
        break;
      case media::DOLBYVISION_PROFILE9:
        supported_dv_codecs |= media::EME_CODEC_DOLBY_VISION_PROFILE9;
        break;
      default:
        DVLOG(1) << "Unexpected "
                 << GetCodecName(media::VideoCodec::kDolbyVision)
                 << " profile: " << GetProfileName(profile);
        break;
    }
  }

  return supported_dv_codecs;
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)

SupportedCodecs GetSupportedCodecs(const media::CdmCapability& capability,
                                   bool requires_clear_lead_support = true) {
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
        supported_codecs |= media::EME_CODEC_DTSE;
        supported_codecs |= media::EME_CODEC_DTSXP2;
        break;
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
      case media::AudioCodec::kAC3:
        supported_codecs |= media::EME_CODEC_AC3;
        break;
      case media::AudioCodec::kEAC3:
        supported_codecs |= media::EME_CODEC_EAC3;
        break;
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
      case media::AudioCodec::kAC4:
        supported_codecs |= media::EME_CODEC_AC4;
        break;
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
      default:
        DVLOG(1) << "Unexpected supported codec: " << GetCodecName(codec);
        break;
    }
  }

  // For compatibility with older CDMs different profiles are only used
  // with some video codecs.
  for (const auto& [codec, video_codec_info] : capability.video_codecs) {
    if (requires_clear_lead_support && !video_codec_info.supports_clear_lead) {
      continue;
    }
    switch (codec) {
      case media::VideoCodec::kVP8:
        supported_codecs |= media::EME_CODEC_VP8;
        break;
      case media::VideoCodec::kVP9:
        supported_codecs |= GetVP9Codecs(video_codec_info.supported_profiles);
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
        supported_codecs |= GetHevcCodecs(video_codec_info.supported_profiles);
        break;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
      case media::VideoCodec::kDolbyVision:
        supported_codecs |=
            GetDolbyVisionCodecs(video_codec_info.supported_profiles);
        break;
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
      default:
        DVLOG(1) << "Unexpected supported codec: " << GetCodecName(codec);
        break;
    }
  }

  return supported_codecs;
}
#endif  // BUILDFLAG(ENABLE_WIDEVINE) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_WIDEVINE)

// Returns whether persistent-license session can be supported.
bool CanSupportPersistentLicense() {
  // Do not support persistent-license if the process cannot persist data.

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, platform verification is similar to CDM host verification
  // and is always checked, so persistent licenses are allowed.
  // TODO(jrummell): Currently the ChromeOS CDM does not require storage ID
  // to support persistent license. Update this logic when the new CDM requires
  // storage ID.
  return true;

#elif BUILDFLAG(IS_ANDROID)
  // Since we do not control the implementation of the MediaDrm API on Android,
  // we assume that it can and will make use of persistence no matter whether
  // persistence-based features are supported or not.
  return true;

#elif BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION) && \
    BUILDFLAG(ENABLE_CDM_STORAGE_ID)
  // On other platforms, persistent licenses are only supported if CDM host
  // verification and CDM storage ID are available.
  return true;

#else
  DVLOG_IF(2, !BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION))
      << __func__ << ": Not supported without CDM host verification.";
  DVLOG_IF(2, !BUILDFLAG(ENABLE_CDM_STORAGE_ID))
      << __func__ << ": Not supported without CDM storage ID.";
  return false;

#endif  // BUILDFLAG(IS_CHROMEOS)
}

// Remove `kPersistentLicense` support if it's not supported by the platform.
base::flat_set<CdmSessionType> UpdatePersistentLicenseSupport(
    bool can_persist_data,
    base::flat_set<CdmSessionType> session_types) {
  if (!can_persist_data || !CanSupportPersistentLicense()) {
    session_types.erase(CdmSessionType::kPersistentLicense);
  }
  return session_types;
}

void AddWidevine(const media::KeySystemCapability& capability,
                 bool can_persist_data,
                 KeySystemInfos* key_systems) {
#if BUILDFLAG(IS_ANDROID)
  // When using MediaDrm, we assume it'll always try to persist some data.
  // If we are in incognito mode and MediaDrm were to persist data, we are
  // somewhat violating the incognito assumption, so don't allow this.
  if (!can_persist_data) {
    DVLOG(2) << __func__ << ": Persistent data not supported.";
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // Codecs and encryption schemes.
  SupportedCodecs codecs = media::EME_CODEC_NONE;
  SupportedCodecs hw_secure_codecs = media::EME_CODEC_NONE;
#if BUILDFLAG(IS_WIN)
  SupportedCodecs hw_secure_codecs_clear_lead_support_not_required =
      media::EME_CODEC_NONE;
#endif
  base::flat_set<::media::EncryptionScheme> encryption_schemes;
  base::flat_set<::media::EncryptionScheme> hw_secure_encryption_schemes;
  base::flat_set<CdmSessionType> session_types;
  base::flat_set<CdmSessionType> hw_secure_session_types;

  if (capability.sw_secure_capability) {
    codecs = GetSupportedCodecs(capability.sw_secure_capability.value());
    encryption_schemes = capability.sw_secure_capability->encryption_schemes;
    session_types = UpdatePersistentLicenseSupport(
        can_persist_data, capability.sw_secure_capability->session_types);
    if (!base::Contains(session_types, CdmSessionType::kTemporary)) {
      DVLOG(1) << "Temporary sessions must be supported.";
      return;
    }
    DVLOG(2) << "Software secure Widevine supported";
  } else {
    DVLOG(2) << "Software secure Widevine NOT supported";
  }

  if (capability.hw_secure_capability) {
    // For the default Widevine key system, we support a codec only when it
    // supports clear lead, unless `force_support_clear_lead` is set to true.
    const bool force_support_clear_lead =
        media::kHardwareSecureDecryptionForceSupportClearLead.Get();
    hw_secure_codecs = GetSupportedCodecs(
        capability.hw_secure_capability.value(), !force_support_clear_lead);
#if BUILDFLAG(IS_WIN)
    // For the experimental Widevine key system, we do not have to filter the
    // hardware secure codecs by whether they support clear lead or not.
    hw_secure_codecs_clear_lead_support_not_required =
        GetSupportedCodecs(capability.hw_secure_capability.value(),
                           /*requires_clear_lead_support=*/false);
#endif  // BUILDFLAG(IS_WIN)

    hw_secure_encryption_schemes =
        capability.hw_secure_capability->encryption_schemes;
    hw_secure_session_types = UpdatePersistentLicenseSupport(
        can_persist_data, capability.hw_secure_capability->session_types);
    if (!base::Contains(hw_secure_session_types, CdmSessionType::kTemporary)) {
      DVLOG(1) << "Temporary sessions must be supported.";
      return;
    }
    DVLOG(2) << "Hardware secure Widevine supported";
  } else {
    DVLOG(2) << "Hardware secure Widevine NOT supported";
  }

#if BUILDFLAG(IS_ANDROID)
  // It doesn't make sense to support hw secure codecs but not regular codecs.
  if (codecs == media::EME_CODEC_NONE) {
    DCHECK(hw_secure_codecs == media::EME_CODEC_NONE);
    DVLOG(3) << __func__ << " Widevine NOT supported.";
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // Robustness.
  using Robustness = WidevineKeySystemInfo::Robustness;
  auto max_audio_robustness = Robustness::SW_SECURE_CRYPTO;
  auto max_video_robustness = Robustness::SW_SECURE_DECODE;
#if BUILDFLAG(IS_WIN)
  auto max_experimental_audio_robustness = Robustness::SW_SECURE_CRYPTO;
  auto max_experimental_video_robustness = Robustness::SW_SECURE_DECODE;
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, we support HW_SECURE_ALL even without hardware secure codecs.
  // See WidevineKeySystemInfo::GetRobustnessConfigRule().
  max_audio_robustness = Robustness::HW_SECURE_ALL;
  max_video_robustness = Robustness::HW_SECURE_ALL;
#elif BUILDFLAG(IS_ANDROID)
  // On Android we support hardware secure if possible.
  max_audio_robustness = Robustness::HW_SECURE_CRYPTO;
  max_video_robustness = Robustness::HW_SECURE_ALL;
#else
  // The hardware secure robustness for the two keys systems are guarded by
  // different flags. The audio and video robustness should be set differently
  // for the experimental and normal key system.
  if (base::FeatureList::IsEnabled(media::kHardwareSecureDecryption)) {
    max_audio_robustness = Robustness::HW_SECURE_CRYPTO;
    max_video_robustness = Robustness::HW_SECURE_ALL;
  }
#if BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(
          media::kHardwareSecureDecryptionExperiment)) {
    max_experimental_audio_robustness = Robustness::HW_SECURE_CRYPTO;
    max_experimental_video_robustness = Robustness::HW_SECURE_ALL;
  }
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Others.
  auto persistent_state_support = EmeFeatureSupport::REQUESTABLE;
  auto distinctive_identifier_support = EmeFeatureSupport::NOT_SUPPORTED;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  distinctive_identifier_support = EmeFeatureSupport::REQUESTABLE;
#elif BUILDFLAG(IS_ANDROID)
  // Since we do not control the implementation of the MediaDrm API on Android,
  // we assume that it can and will make use of persistence no matter whether
  // persistence-based features are supported or not.
  persistent_state_support = EmeFeatureSupport::ALWAYS_ENABLED;
  distinctive_identifier_support = EmeFeatureSupport::ALWAYS_ENABLED;
#endif

  key_systems->emplace_back(std::make_unique<WidevineKeySystemInfo>(
      codecs, encryption_schemes, session_types, hw_secure_codecs,
      hw_secure_encryption_schemes, hw_secure_session_types,
      max_audio_robustness, max_video_robustness, persistent_state_support,
      distinctive_identifier_support));

#if BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(
          media::kHardwareSecureDecryptionExperiment)) {
    // Register another WidevineKeySystemInfo on Windows only for
    // `kWidevineExperimentKeySystem`. The default WidevineKeySystemInfo
    // above requires clear lead to be supported. This is not required for
    // the experimental key system because content providers using the
    // experimental key system would not serve clear lead content.
    auto experimental_key_system_info = std::make_unique<WidevineKeySystemInfo>(
        codecs, encryption_schemes, session_types,
        hw_secure_codecs_clear_lead_support_not_required,
        hw_secure_encryption_schemes, hw_secure_session_types,
        max_experimental_audio_robustness, max_experimental_video_robustness,
        persistent_state_support, distinctive_identifier_support);
    experimental_key_system_info->set_experimental();

    key_systems->emplace_back(std::move(experimental_key_system_info));

    // Register another WidevineKeySystemInfo on Windows only for
    // `kWidevineExperiment2KeySystem`. This key system is the same as the
    // experimental key system above except clear lead support is required.
    auto experimental_two_key_system_info =
        std::make_unique<WidevineKeySystemInfo>(
            codecs, encryption_schemes, session_types, hw_secure_codecs,
            hw_secure_encryption_schemes, hw_secure_session_types,
            max_experimental_audio_robustness,
            max_experimental_video_robustness, persistent_state_support,
            distinctive_identifier_support);
    experimental_two_key_system_info->set_experimental_two();

    key_systems->emplace_back(std::move(experimental_two_key_system_info));
  }
#endif  // BUILDFLAG(IS_WIN)
}
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

void AddExternalClearKey(const media::KeySystemCapability& /*capability*/,
                         KeySystemInfos* key_systems) {
  DVLOG(1) << __func__;

  if (!base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting)) {
    DLOG(ERROR) << "ExternalClearKey supported despite not enabled.";
    return;
  }

  // TODO(xhwang): Actually use `capability` to determine capabilities.
  key_systems->push_back(std::make_unique<ExternalClearKeyKeySystemInfo>());
}

#if BUILDFLAG(IS_WIN)
void AddMediaFoundationClearKey(
    const media::KeySystemCapability& /*capability*/,
    KeySystemInfos* key_systems) {
  DVLOG(1) << __func__;

  if (!base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting)) {
    DLOG(ERROR) << "ExternalClearKey supported despite not enabled.";
    return;
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  // TODO(crbug.com/40890911): Remove this hard-codeded supported codecs so that
  // real hardware capabilities can be checked.
  key_systems->push_back(std::make_unique<ExternalClearKeyKeySystemInfo>(
      media::kMediaFoundationClearKeyKeySystem, std::vector<std::string>(),
      // MediaFoundation Clear Key Key System uses Windows Media Foundation's
      // decoders. H264 ("avc1.64001E") for video and MP4 AAC ("mp4a.40.2") for
      // audio are always supported. VideoCodec::kH264 is an EME_CODEC_AVC1.
      // AudioCodec::kAAC is an EME_CODEC_AAC. DolbyVision Profile 5
      // ("dvh1.05.06") and 8.1/8.4 ("dvhe.08.07") are also always supported.
      media::EME_CODEC_AVC1 | media::EME_CODEC_AAC |
          media::EME_CODEC_DOLBY_VISION_HEVC,
      // On Windows, MediaFoundation Clear Key CDM requires identifier,
      // persistent state and HW secure codecs. We pretent to require these for
      // testing purposes.
      media::EmeConfig{
          .identifier = media::EmeConfigRuleState::kRequired,
          .persistence = media::EmeConfigRuleState::kRequired,
          .hw_secure_codecs = media::EmeConfigRuleState::kRequired},
      EmeFeatureSupport::ALWAYS_ENABLED, EmeFeatureSupport::ALWAYS_ENABLED));
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
void AddAndroidPlatformKeySystem(const std::string& key_system,
                                 const media::KeySystemCapability& capability,
                                 bool can_persist_data,
                                 KeySystemInfos* key_systems) {
  DCHECK_NE(key_system, kWidevineKeySystem);

  // When using MediaDrm, we assume it'll always try to persist some data.
  // If we don't allow data to be persisted, then skip this key system.
  if (!can_persist_data) {
    DVLOG(2) << __func__ << ": Key system " << key_system
             << " not supported in incognito process.";
    return;
  }

  // Codecs and encryption schemes.
  SupportedCodecs sw_secure_codecs = media::EME_CODEC_NONE;
  SupportedCodecs hw_secure_codecs = media::EME_CODEC_NONE;
  base::flat_set<::media::EncryptionScheme> sw_secure_encryption_schemes;
  base::flat_set<::media::EncryptionScheme> hw_secure_encryption_schemes;

  if (capability.sw_secure_capability) {
    sw_secure_codecs =
        GetSupportedCodecs(capability.sw_secure_capability.value());
    sw_secure_encryption_schemes =
        capability.sw_secure_capability->encryption_schemes;
    DVLOG(2) << "Software secure " << key_system << " supported";
  }

  if (capability.hw_secure_capability) {
    hw_secure_codecs =
        GetSupportedCodecs(capability.hw_secure_capability.value());
    hw_secure_encryption_schemes =
        capability.hw_secure_capability->encryption_schemes;
    DVLOG(2) << "Hardware secure " << key_system << " supported";
  }

  key_systems->push_back(std::make_unique<AndroidKeySystemInfo>(
      key_system, sw_secure_codecs, sw_secure_encryption_schemes,
      hw_secure_codecs, hw_secure_encryption_schemes));
}
#endif  // BUILDFLAG(IS_ANDROID)

void OnKeySystemSupportUpdated(
    bool can_persist_data,
    media::GetSupportedKeySystemsCB cb,
    content::KeySystemCapabilities key_system_capabilities) {
  KeySystemInfos key_systems;
  for (const auto& [key_system, capability] : key_system_capabilities) {
#if BUILDFLAG(ENABLE_WIDEVINE)
    if (key_system == kWidevineKeySystem) {
      AddWidevine(capability, can_persist_data, &key_systems);
      continue;
    }
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

    if (key_system == media::kExternalClearKeyKeySystem) {
      AddExternalClearKey(capability, &key_systems);
      continue;
    }

#if BUILDFLAG(IS_WIN)
    if (key_system == media::kMediaFoundationClearKeyKeySystem) {
      AddMediaFoundationClearKey(capability, &key_systems);
      continue;
    }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
    AddAndroidPlatformKeySystem(key_system, capability, can_persist_data,
                                &key_systems);
#else
    DLOG(ERROR) << "Unrecognized key system: " << key_system;
#endif  // BUILDFLAG(IS_ANDROID)
  }

  cb.Run(std::move(key_systems));
}

}  // namespace

std::unique_ptr<media::KeySystemSupportRegistration>
GetSupportedKeySystemsUpdates(content::RenderFrame* render_frame,
                              bool can_persist_data,
                              media::GetSupportedKeySystemsCB cb) {
  return content::ObserveKeySystemSupportUpdate(
      render_frame, base::BindRepeating(&OnKeySystemSupportUpdated,
                                        can_persist_data, std::move(cb)));
}

}  // namespace cdm

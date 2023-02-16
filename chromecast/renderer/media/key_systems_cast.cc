// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/media/key_systems_cast.h"

#include <string>

#include "base/check.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/media/base/key_systems_common.h"
#include "components/cdm/renderer/android_key_systems.h"
#include "media/base/content_decryption_module.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_info.h"
#include "media/media_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/widevine/cdm/buildflags.h"

#if BUILDFLAG(ENABLE_WIDEVINE)
#include "components/cdm/renderer/widevine_key_system_info.h"
#endif

using ::media::CdmSessionType;
using ::media::EmeConfig;
using ::media::EmeConfigRuleState;
using ::media::EmeFeatureSupport;
using ::media::EmeInitDataType;
using ::media::EmeMediaType;
using ::media::EncryptionScheme;
using ::media::SupportedCodecs;

namespace chromecast {
namespace media {
namespace {

#if BUILDFLAG(ENABLE_PLAYREADY)
class PlayReadyKeySystemInfo : public ::media::KeySystemInfo {
 public:
  PlayReadyKeySystemInfo(SupportedCodecs supported_non_secure_codecs,
                         SupportedCodecs supported_secure_codecs,
                         bool persistent_license_support)
      : supported_non_secure_codecs_(supported_non_secure_codecs),
#if BUILDFLAG(IS_ANDROID)
        supported_secure_codecs_(supported_secure_codecs),
#endif  // BUILDFLAG(IS_ANDROID)
        persistent_license_support_(persistent_license_support) {
  }

  std::string GetKeySystemName() const override {
    return media::kChromecastPlayreadyKeySystem;
  }

  bool IsSupportedInitDataType(EmeInitDataType init_data_type) const override {
    return init_data_type == EmeInitDataType::CENC;
  }

  SupportedCodecs GetSupportedCodecs() const override {
    return supported_non_secure_codecs_;
  }

#if BUILDFLAG(IS_ANDROID)
  SupportedCodecs GetSupportedHwSecureCodecs() const override {
    return supported_secure_codecs_;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  EmeConfig::Rule GetRobustnessConfigRule(
      const std::string& key_system,
      EmeMediaType media_type,
      const std::string& requested_robustness,
      const bool* /*hw_secure_requirement*/) const override {
    // `hw_secure_requirement` is ignored here because it's a temporary solution
    // until a larger refactoring of the key system logic is done. It also does
    // not need to account for it here because if it does introduce an
    // incompatibility at this point, it will still be caught by the rule logic
    // in KeySystemConfigSelector: crbug.com/1204284
    if (requested_robustness.empty()) {
#if BUILDFLAG(IS_ANDROID)
      return EmeConfig{.hw_secure_codecs = EmeConfigRuleState::kRequired};
#else
      return media::EmeConfig::SupportedRule();
#endif  // BUILDFLAG(IS_ANDROID)
    }

    // Cast-specific PlayReady implementation does not currently recognize or
    // support non-empty robustness strings.
    return media::EmeConfig::UnsupportedRule();
  }

  EmeConfig::Rule GetPersistentLicenseSessionSupport() const override {
    if (persistent_license_support_) {
      return media::EmeConfig::SupportedRule();
    } else {
      return media::EmeConfig::UnsupportedRule();
    }
  }

  EmeFeatureSupport GetPersistentStateSupport() const override {
    return EmeFeatureSupport::ALWAYS_ENABLED;
  }
  EmeFeatureSupport GetDistinctiveIdentifierSupport() const override {
    return EmeFeatureSupport::ALWAYS_ENABLED;
  }

  EmeConfig::Rule GetEncryptionSchemeConfigRule(
      EncryptionScheme encryption_scheme) const override {
    if (encryption_scheme == EncryptionScheme::kCenc) {
      return media::EmeConfig::SupportedRule();
    } else {
      return media::EmeConfig::UnsupportedRule();
    }
  }

 private:
  const SupportedCodecs supported_non_secure_codecs_;
#if BUILDFLAG(IS_ANDROID)
  const SupportedCodecs supported_secure_codecs_;
#endif  // BUILDFLAG(IS_ANDROID)
  const bool persistent_license_support_;
};
#endif  // BUILDFLAG(ENABLE_PLAYREADY)

#if BUILDFLAG(USE_CHROMECAST_CDMS) || BUILDFLAG(ENABLE_LIBRARY_CDMS)
SupportedCodecs GetCastEmeSupportedCodecs() {
  SupportedCodecs codecs = ::media::EME_CODEC_AAC | ::media::EME_CODEC_AVC1 |
                           ::media::EME_CODEC_VP9_PROFILE0 |
                           ::media::EME_CODEC_VP9_PROFILE2 |
                           ::media::EME_CODEC_VP8;

#if !BUILDFLAG(DISABLE_SECURE_FLAC_OPUS_DECODING)
  codecs |= ::media::EME_CODEC_FLAC | ::media::EME_CODEC_OPUS;
#endif  // BUILDFLAG(DISABLE_SECURE_FLAC_OPUS_DECODING)

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  codecs |= ::media::EME_CODEC_HEVC_PROFILE_MAIN;
  codecs |= ::media::EME_CODEC_HEVC_PROFILE_MAIN10;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)

#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
  codecs |= ::media::EME_CODEC_DOLBY_VISION_AVC;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  codecs |= ::media::EME_CODEC_DOLBY_VISION_HEVC;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)

#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
  codecs |= ::media::EME_CODEC_AC3 | ::media::EME_CODEC_EAC3;
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)

#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  codecs |= ::media::EME_CODEC_DTS | ::media::EME_CODEC_DTSE |
            ::media::EME_CODEC_DTSXP2;
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)

#if BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
  codecs |= ::media::EME_CODEC_MPEG_H_AUDIO;
#endif  // BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)

  return codecs;
}

void AddCmaKeySystems(::media::KeySystemInfos* key_system_infos,
                      bool enable_persistent_license_support,
                      bool enable_playready) {
  // |codecs| may not be used if Widevine and Playready aren't supported.
  [[maybe_unused]] SupportedCodecs codecs = GetCastEmeSupportedCodecs();

#if BUILDFLAG(ENABLE_PLAYREADY)
  if (enable_playready) {
    key_system_infos->emplace_back(new PlayReadyKeySystemInfo(
        codecs, codecs, enable_persistent_license_support));
  }
#endif  // BUILDFLAG(ENABLE_PLAYREADY)

#if BUILDFLAG(ENABLE_WIDEVINE)
  using Robustness = cdm::WidevineKeySystemInfo::Robustness;

  const base::flat_set<EncryptionScheme> kEncryptionSchemes = {
      EncryptionScheme::kCenc, EncryptionScheme::kCbcs};

  const base::flat_set<CdmSessionType> kSessionTypes = {
      CdmSessionType::kTemporary, CdmSessionType::kPersistentLicense};

  key_system_infos->emplace_back(new cdm::WidevineKeySystemInfo(
      codecs,                        // Regular codecs.
      kEncryptionSchemes,            // Encryption schemes.
      kSessionTypes,                 // Session types.
      codecs,                        // Hardware secure codecs.
      kEncryptionSchemes,            // Hardware secure encryption schemes.
      kSessionTypes,                 // Hardware secure session types.
      Robustness::HW_SECURE_CRYPTO,  // Max audio robustness.
      Robustness::HW_SECURE_ALL,     // Max video robustness.
      // Note: On Chromecast, all CDMs may have persistent state.
      EmeFeatureSupport::ALWAYS_ENABLED,    // Persistent state.
      EmeFeatureSupport::ALWAYS_ENABLED));  // Distinctive identifier.
#endif                                      // BUILDFLAG(ENABLE_WIDEVINE)
}
#elif BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(ENABLE_PLAYREADY)
void AddCastPlayreadyKeySystemAndroid(
    ::media::KeySystemInfos* key_system_infos) {
  DCHECK(key_system_infos);
  SupportedKeySystemResponse response =
      cdm::QueryKeySystemSupport(kChromecastPlayreadyKeySystem);

  if (response.non_secure_codecs == ::media::EME_CODEC_NONE)
    return;

  key_system_infos->emplace_back(new PlayReadyKeySystemInfo(
      response.non_secure_codecs, response.secure_codecs,
      false /* persistent_license_support */));
}
#endif  // BUILDFLAG(ENABLE_PLAYREADY)

void AddCastAndroidKeySystems(
    ::media::KeySystemInfos* key_system_infos,
    bool enable_playready) {
#if BUILDFLAG(ENABLE_PLAYREADY)
  if (enable_playready) {
    AddCastPlayreadyKeySystemAndroid(key_system_infos);
  }
#endif  // BUILDFLAG(ENABLE_PLAYREADY)

#if BUILDFLAG(ENABLE_WIDEVINE)
  cdm::AddAndroidWidevine(key_system_infos);
#endif  // BUILDFLAG(ENABLE_WIDEVINE)
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

// TODO(yucliu): Split CMA/Android logics into their own files.
void AddChromecastKeySystems(
    ::media::KeySystemInfos* key_system_infos,
    bool enable_persistent_license_support,
    bool enable_playready) {
#if BUILDFLAG(USE_CHROMECAST_CDMS) || BUILDFLAG(ENABLE_LIBRARY_CDMS)
  AddCmaKeySystems(key_system_infos, enable_persistent_license_support,
                   enable_playready);
#elif BUILDFLAG(IS_ANDROID)
  AddCastAndroidKeySystems(key_system_infos, enable_playready);
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace media
}  // namespace chromecast

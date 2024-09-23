// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/media/key_systems_cast.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/media/base/key_systems_common.h"
#include "media/base/content_decryption_module.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_info.h"
#include "media/media_buildflags.h"
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
                      bool enable_persistent_license_support) {
  // |codecs| may not be used if Widevine and Playready aren't supported.
  [[maybe_unused]] SupportedCodecs codecs = GetCastEmeSupportedCodecs();

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
#endif  // BUILDFLAG(USE_CHROMECAST_CDMS) || BUILDFLAG(ENABLE_LIBRARY_CDMS)

}  // namespace

void AddChromecastKeySystems(
    ::media::KeySystemInfos* key_system_infos,
    bool enable_persistent_license_support) {
#if BUILDFLAG(USE_CHROMECAST_CDMS) || BUILDFLAG(ENABLE_LIBRARY_CDMS)
  AddCmaKeySystems(key_system_infos, enable_persistent_license_support);
#endif  // BUILDFLAG(USE_CHROMECAST_CDMS) || BUILDFLAG(ENABLE_LIBRARY_CDMS)
}

}  // namespace media
}  // namespace chromecast

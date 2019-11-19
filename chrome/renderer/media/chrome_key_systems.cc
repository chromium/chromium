// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_key_systems.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "components/cdm/renderer/external_clear_key_key_system_properties.h"
#include "components/cdm/renderer/widevine_key_system_properties.h"
#include "content/public/renderer/render_thread.h"
#include "media/base/decrypt_config.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_properties.h"
#include "media/media_buildflags.h"
#include "third_party/widevine/cdm/buildflags.h"

#if defined(OS_ANDROID)
#include "components/cdm/renderer/android_key_systems.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "base/feature_list.h"
#include "content/public/renderer/key_system_support.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#if BUILDFLAG(ENABLE_WIDEVINE)
#include "third_party/widevine/cdm/widevine_cdm_common.h"
// TODO(crbug.com/663554): Needed for WIDEVINE_CDM_MIN_GLIBC_VERSION.
// component updated CDM on all desktop platforms and remove this.
#include "widevine_cdm_version.h" // In SHARED_INTERMEDIATE_DIR.
// The following must be after widevine_cdm_version.h.
#if defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
#include <gnu/libc-version.h>
#include "base/version.h"
#endif  // defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
#endif  // BUILDFLAG(ENABLE_WIDEVINE)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

using media::EmeFeatureSupport;
using media::EmeSessionTypeSupport;
using media::KeySystemProperties;
using media::SupportedCodecs;

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

// External Clear Key (used for testing).
static void AddExternalClearKey(
    std::vector<std::unique_ptr<KeySystemProperties>>* concrete_key_systems) {
  // TODO(xhwang): Move these into an array so we can use a for loop to add
  // supported key systems below.
  static const char kExternalClearKeyKeySystem[] =
      "org.chromium.externalclearkey";
  static const char kExternalClearKeyDecryptOnlyKeySystem[] =
      "org.chromium.externalclearkey.decryptonly";
  static const char kExternalClearKeyMessageTypeTestKeySystem[] =
      "org.chromium.externalclearkey.messagetypetest";
  static const char kExternalClearKeyFileIOTestKeySystem[] =
      "org.chromium.externalclearkey.fileiotest";
  static const char kExternalClearKeyOutputProtectionTestKeySystem[] =
      "org.chromium.externalclearkey.outputprotectiontest";
  static const char kExternalClearKeyPlatformVerificationTestKeySystem[] =
      "org.chromium.externalclearkey.platformverificationtest";
  static const char kExternalClearKeyInitializeFailKeySystem[] =
      "org.chromium.externalclearkey.initializefail";
  static const char kExternalClearKeyCrashKeySystem[] =
      "org.chromium.externalclearkey.crash";
  static const char kExternalClearKeyVerifyCdmHostTestKeySystem[] =
      "org.chromium.externalclearkey.verifycdmhosttest";
  static const char kExternalClearKeyStorageIdTestKeySystem[] =
      "org.chromium.externalclearkey.storageidtest";
  static const char kExternalClearKeyDifferentGuidTestKeySystem[] =
      "org.chromium.externalclearkey.differentguid";
  static const char kExternalClearKeyCdmProxyKeySystem[] =
      "org.chromium.externalclearkey.cdmproxy";

  // TODO(xhwang): Actually use |capability| to determine capabilities.
  media::mojom::KeySystemCapabilityPtr capability;
  if (!content::IsKeySystemSupported(kExternalClearKeyKeySystem, &capability)) {
    DVLOG(1) << "External Clear Key not supported";
    return;
  }

  concrete_key_systems->emplace_back(
      new cdm::ExternalClearKeyProperties(kExternalClearKeyKeySystem));

  // Add support of decrypt-only mode in ClearKeyCdm.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyDecryptOnlyKeySystem));

  // A key system that triggers various types of messages in ClearKeyCdm.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyMessageTypeTestKeySystem));

  // A key system that triggers the FileIO test in ClearKeyCdm.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyFileIOTestKeySystem));

  // A key system that triggers the output protection test in ClearKeyCdm.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyOutputProtectionTestKeySystem));

  // A key system that triggers the platform verification test in ClearKeyCdm.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyPlatformVerificationTestKeySystem));

  // A key system that Chrome thinks is supported by ClearKeyCdm, but actually
  // will be refused by ClearKeyCdm. This is to test the CDM initialization
  // failure case.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyInitializeFailKeySystem));

  // A key system that triggers a crash in ClearKeyCdm.
  concrete_key_systems->emplace_back(
      new cdm::ExternalClearKeyProperties(kExternalClearKeyCrashKeySystem));

  // A key system that triggers the verify host files test in ClearKeyCdm.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyVerifyCdmHostTestKeySystem));

  // A key system that fetches the Storage ID in ClearKeyCdm.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyStorageIdTestKeySystem));

  // A key system that is registered with a different CDM GUID.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyDifferentGuidTestKeySystem));

  // A key system that requires the use of CdmProxy.
  concrete_key_systems->emplace_back(
      new cdm::ExternalClearKeyProperties(kExternalClearKeyCdmProxyKeySystem));
}

#if BUILDFLAG(ENABLE_WIDEVINE)
static SupportedCodecs GetSupportedCodecs(
    const std::vector<media::VideoCodec>& supported_video_codecs,
    bool supports_vp9_profile2,
    bool is_secure) {
  SupportedCodecs supported_codecs = media::EME_CODEC_NONE;

  // Audio codecs are always supported because the CDM only does decrypt-only
  // for audio. The only exception is when |is_secure| is true and there's no
  // secure video decoder available, which is a signal that secure hardware
  // decryption is not available either.
  // TODO(sandersd): Distinguish these from those that are directly supported,
  // as those may offer a higher level of protection.
  if (!supported_video_codecs.empty() || !is_secure) {
    supported_codecs |= media::EME_CODEC_OPUS;
    supported_codecs |= media::EME_CODEC_VORBIS;
    supported_codecs |= media::EME_CODEC_FLAC;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    supported_codecs |= media::EME_CODEC_AAC;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  }

  // Video codecs are determined by what was registered for the CDM.
  for (const auto& codec : supported_video_codecs) {
    switch (codec) {
      case media::VideoCodec::kCodecVP8:
        supported_codecs |= media::EME_CODEC_VP8;
        break;
      case media::VideoCodec::kCodecVP9:
        supported_codecs |= media::EME_CODEC_VP9_PROFILE0;
        if (supports_vp9_profile2)
          supported_codecs |= media::EME_CODEC_VP9_PROFILE2;
        break;
      case media::VideoCodec::kCodecAV1:
        supported_codecs |= media::EME_CODEC_AV1;
        break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      case media::VideoCodec::kCodecH264:
        supported_codecs |= media::EME_CODEC_AVC1;
        break;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
      default:
        DVLOG(1) << "Unexpected supported codec: " << GetCodecName(codec);
        break;
    }
  }

  return supported_codecs;
}

// Returns persistent-license session support.
static EmeSessionTypeSupport GetPersistentLicenseSupport(
    bool supported_by_the_cdm) {
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
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION) || defined(OS_CHROMEOS)
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

#if defined(OS_CHROMEOS)
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
#endif  // defined(OS_CHROMEOS)
}

static void AddWidevine(
    std::vector<std::unique_ptr<KeySystemProperties>>* concrete_key_systems) {
#if defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
  base::Version glibc_version(gnu_get_libc_version());
  DCHECK(glibc_version.IsValid());
  if (glibc_version < base::Version(WIDEVINE_CDM_MIN_GLIBC_VERSION))
    return;
#endif  // defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)

  media::mojom::KeySystemCapabilityPtr capability;
  if (!content::IsKeySystemSupported(kWidevineKeySystem, &capability)) {
    DVLOG(1) << "Widevine CDM is not currently available.";
    return;
  }

  // Codecs and encryption schemes.
  auto codecs = GetSupportedCodecs(capability->video_codecs,
                                   capability->supports_vp9_profile2,
                                   /*is_secure=*/false);
  const auto& encryption_schemes = capability->encryption_schemes;
  // TODO(xhwang): Investigate whether hardware VP9 profile 2 is supported.
  auto hw_secure_codecs = GetSupportedCodecs(capability->hw_secure_video_codecs,
                                             /*supports_vp9_profile2=*/false,
                                             /*is_secure=*/true);
  const auto& hw_secure_encryption_schemes =
      capability->hw_secure_encryption_schemes;

  // Robustness.
  using Robustness = cdm::WidevineKeySystemProperties::Robustness;
  auto max_audio_robustness = Robustness::SW_SECURE_CRYPTO;
  auto max_video_robustness = Robustness::SW_SECURE_DECODE;

#if defined(OS_CHROMEOS)
  // On ChromeOS, we support HW_SECURE_ALL even without hardware secure codecs.
  // See WidevineKeySystemProperties::GetRobustnessConfigRule().
  max_audio_robustness = Robustness::HW_SECURE_ALL;
  max_video_robustness = Robustness::HW_SECURE_ALL;
#else
  if (base::FeatureList::IsEnabled(media::kHardwareSecureDecryption)) {
    max_audio_robustness = Robustness::HW_SECURE_CRYPTO;
    max_video_robustness = Robustness::HW_SECURE_ALL;
  }
#endif

  // Session types.
  bool cdm_supports_temporary_session = base::Contains(
      capability->session_types, media::CdmSessionType::kTemporary);
  if (!cdm_supports_temporary_session) {
    DVLOG(1) << "Temporary session must be supported.";
    return;
  }

  bool cdm_supports_persistent_license = base::Contains(
      capability->session_types, media::CdmSessionType::kPersistentLicense);
  auto persistent_license_support =
      GetPersistentLicenseSupport(cdm_supports_persistent_license);

  // TODO(xhwang): Check more conditions as needed.
  auto persistent_usage_record_support =
      base::Contains(capability->session_types,
                     media::CdmSessionType::kPersistentUsageRecord)
          ? EmeSessionTypeSupport::SUPPORTED
          : EmeSessionTypeSupport::NOT_SUPPORTED;

  // Others.
  auto persistent_state_support = EmeFeatureSupport::REQUESTABLE;
  auto distinctive_identifier_support = EmeFeatureSupport::NOT_SUPPORTED;
#if defined(OS_CHROMEOS)
  distinctive_identifier_support = EmeFeatureSupport::REQUESTABLE;
#endif

  concrete_key_systems->emplace_back(new cdm::WidevineKeySystemProperties(
      codecs, encryption_schemes, hw_secure_codecs,
      hw_secure_encryption_schemes, max_audio_robustness, max_video_robustness,
      persistent_license_support, persistent_usage_record_support,
      persistent_state_support, distinctive_identifier_support));
}
#endif  // BUILDFLAG(ENABLE_WIDEVINE)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

void AddChromeKeySystems(
    std::vector<std::unique_ptr<KeySystemProperties>>* key_systems_properties) {
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  if (base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting))
    AddExternalClearKey(key_systems_properties);

#if BUILDFLAG(ENABLE_WIDEVINE)
  AddWidevine(key_systems_properties);
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if defined(OS_ANDROID)
  cdm::AddAndroidWidevine(key_systems_properties);
#endif  // defined(OS_ANDROID)
}

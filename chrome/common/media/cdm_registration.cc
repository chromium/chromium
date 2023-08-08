// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media/cdm_registration.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/cdm_info.h"
#include "media/cdm/cdm_capability.h"
#include "media/cdm/cdm_type.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "third_party/widevine/cdm/buildflags.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "base/command_line.h"
#include "media/base/media_switches.h"
#include "media/cdm/cdm_paths.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_WIDEVINE)
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#include "base/native_library.h"
#include "chrome/common/chrome_paths.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/no_destructor.h"
#include "chrome/common/media/component_widevine_cdm_hint_file_linux.h"
#include "components/cdm/common/cdm_manifest.h"
#include "media/cdm/supported_audio_codecs.h"
// Needed for WIDEVINE_CDM_MIN_GLIBC_VERSION. This file is in
// SHARED_INTERMEDIATE_DIR.
#include "widevine_cdm_version.h"  // nogncheck
// The following must be after widevine_cdm_version.h.
#if defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
#include <gnu/libc-version.h>
#include "base/version.h"
#endif  // defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

#if BUILDFLAG(IS_ANDROID)
#include "components/cdm/common/android_cdm_registration.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

using Robustness = content::CdmInfo::Robustness;

#if BUILDFLAG(ENABLE_WIDEVINE)
#if (BUILDFLAG(BUNDLE_WIDEVINE_CDM) ||            \
     BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
// Create a CdmInfo for a Widevine CDM, using |version|, |cdm_library_path|, and
// |capability|.
std::unique_ptr<content::CdmInfo> CreateWidevineCdmInfo(
    const base::Version& version,
    const base::FilePath& cdm_library_path,
    media::CdmCapability capability) {
  return std::make_unique<content::CdmInfo>(
      kWidevineKeySystem, Robustness::kSoftwareSecure, std::move(capability),
      /*supports_sub_key_systems=*/false, kWidevineCdmDisplayName,
      kWidevineCdmType, version, cdm_library_path);
}

// On desktop Linux and ChromeOS, given |cdm_base_path| that points to a folder
// containing the Widevine CDM and associated files, read the manifest included
// in that directory and create a CdmInfo. If that is successful, return the
// CdmInfo. If not, return nullptr.
std::unique_ptr<content::CdmInfo> CreateCdmInfoFromWidevineDirectory(
    const base::FilePath& cdm_base_path) {
  // Library should be inside a platform specific directory.
  auto cdm_library_path =
      media::GetPlatformSpecificDirectory(cdm_base_path)
          .Append(base::GetNativeLibraryName(kWidevineCdmLibraryName));
  if (!base::PathExists(cdm_library_path)) {
    DLOG(ERROR) << __func__ << " no directory: " << cdm_library_path;
    return nullptr;
  }

  // Manifest should be at the top level.
  auto manifest_path = cdm_base_path.Append(FILE_PATH_LITERAL("manifest.json"));
  base::Version version;
  media::CdmCapability capability;
  if (!ParseCdmManifestFromPath(manifest_path, &version, &capability)) {
    DLOG(ERROR) << __func__ << " no manifest: " << manifest_path;
    return nullptr;
  }

  return CreateWidevineCdmInfo(version, cdm_library_path,
                               std::move(capability));
}
#endif  // (BUILDFLAG(BUNDLE_WIDEVINE_CDM) ||
        // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)) && (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS))

#if BUILDFLAG(BUNDLE_WIDEVINE_CDM) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
// On Linux/ChromeOS we have to preload the CDM since it uses the zygote
// sandbox. On Windows and Mac, the bundled CDM is handled by the component
// updater.

// This code checks to see if the Widevine CDM was bundled with Chrome. If one
// can be found and looks valid, it returns the CdmInfo for the CDM. Otherwise
// it returns nullptr.
content::CdmInfo* GetBundledWidevine() {
  // We only want to do this on the first call, as if Widevine wasn't bundled
  // with Chrome (or it was deleted/removed) it won't be loaded into the zygote.
  static base::NoDestructor<std::unique_ptr<content::CdmInfo>> s_cdm_info(
      []() -> std::unique_ptr<content::CdmInfo> {
        base::FilePath install_dir;
        CHECK(base::PathService::Get(chrome::DIR_BUNDLED_WIDEVINE_CDM,
                                     &install_dir));
        return CreateCdmInfoFromWidevineDirectory(install_dir);
      }());
  return s_cdm_info->get();
}
#endif  // BUILDFLAG(BUNDLE_WIDEVINE_CDM) &&
        // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
// This code checks to see if a component updated Widevine CDM can be found. If
// there is one and it looks valid, return the CdmInfo for that CDM. Otherwise
// return nullptr.
content::CdmInfo* GetComponentUpdatedWidevine() {
  // We only want to do this on the first call, as the component updater may run
  // and download a new version once Chrome has been running for a while. Since
  // the first returned version will be the one loaded into the zygote, we want
  // to return the same thing on subsequent calls.
  static base::NoDestructor<std::unique_ptr<content::CdmInfo>> s_cdm_info(
      []() -> std::unique_ptr<content::CdmInfo> {
        auto install_dir = GetLatestComponentUpdatedWidevineCdmDirectory();
        if (install_dir.empty()) {
          return nullptr;
        }

        return CreateCdmInfoFromWidevineDirectory(install_dir);
      }());
  return s_cdm_info->get();
}
#endif  // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT) &&
        // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))

void AddSoftwareSecureWidevine(std::vector<content::CdmInfo>* cdms) {
  DVLOG(1) << __func__;

#if BUILDFLAG(IS_ANDROID)
  // On Android Widevine is done by MediaDrm, and should be supported on all
  // devices. Register Widevine without any capabilities so that it will be
  // checked the first time some page attempts to play protected content.
  cdms->emplace_back(
      kWidevineKeySystem, Robustness::kSoftwareSecure, absl::nullopt,
      /*supports_sub_key_systems=*/false, kWidevineCdmDisplayName,
      kWidevineCdmType, base::Version(), base::FilePath());

#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#if defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
  base::Version glibc_version(gnu_get_libc_version());
  DCHECK(glibc_version.IsValid());
  if (glibc_version < base::Version(WIDEVINE_CDM_MIN_GLIBC_VERSION)) {
    LOG(WARNING) << "Widevine not registered because glibc version is too low";
    return;
  }
#endif  // defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)

  // The Widevine CDM on Linux needs to be registered (and loaded) before the
  // zygote is locked down. The CDM can be found from the version bundled with
  // Chrome (if BUNDLE_WIDEVINE_CDM = true) and/or the version downloaded by
  // the component updater (if ENABLE_WIDEVINE_CDM_COMPONENT = true). If two
  // versions exist, take the one with the higher version number.
  //
  // Note that the component updater will detect the bundled version, and if
  // there is no newer version available, select the bundled version. In this
  // case both versions will be the same and point to the same directory, so
  // it doesn't matter which one is loaded.
  content::CdmInfo* bundled_widevine = nullptr;
#if BUILDFLAG(BUNDLE_WIDEVINE_CDM)
  bundled_widevine = GetBundledWidevine();
#endif

  content::CdmInfo* updated_widevine = nullptr;
#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
  updated_widevine = GetComponentUpdatedWidevine();
#endif

  // If only a bundled version is available, or both are available and the
  // bundled version is not less than the updated version, register the
  // bundled version. If only the updated version is available, or both are
  // available and the updated version is greater, then register the updated
  // version. If neither are available, then nothing is registered.
  if (bundled_widevine &&
      (!updated_widevine ||
       bundled_widevine->version >= updated_widevine->version)) {
    VLOG(1) << "Registering bundled Widevine " << bundled_widevine->version;
    cdms->push_back(*bundled_widevine);
  } else if (updated_widevine) {
    VLOG(1) << "Registering component updated Widevine "
            << updated_widevine->version;
    cdms->push_back(*updated_widevine);
  } else {
    VLOG(1) << "Widevine enabled but no library found";
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void AddHardwareSecureWidevine(std::vector<content::CdmInfo>* cdms) {
  DVLOG(1) << __func__;

#if BUILDFLAG(IS_ANDROID)
  // On Android Widevine is done by MediaDrm, and should be supported on all
  // devices. Register Widevine without any capabilities so that it will be
  // checked the first time some page attempts to play protected content.
  cdms->emplace_back(
      kWidevineKeySystem, Robustness::kHardwareSecure, absl::nullopt,
      /*supports_sub_key_systems=*/false, kWidevineCdmDisplayName,
      kWidevineCdmType, base::Version(), base::FilePath());

#elif BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLacrosUseChromeosProtectedMedia)) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  media::CdmCapability capability;

  // The following audio formats are supported for decrypt-only.
  capability.audio_codecs = media::GetCdmSupportedAudioCodecs();

  // We currently support VP9, H264 and HEVC video formats with
  // decrypt-and-decode. Not specifying any profiles to indicate that all
  // relevant profiles should be considered supported.
  const media::VideoCodecInfo kAllProfiles;
  capability.video_codecs.emplace(media::VideoCodec::kVP9, kAllProfiles);
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  capability.video_codecs.emplace(media::VideoCodec::kH264, kAllProfiles);
#endif
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLacrosEnablePlatformHevc)) {
    capability.video_codecs.emplace(media::VideoCodec::kHEVC, kAllProfiles);
  }
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(media::kPlatformHEVCDecoderSupport)) {
    capability.video_codecs.emplace(media::VideoCodec::kHEVC, kAllProfiles);
  }
#else
  capability.video_codecs.emplace(media::VideoCodec::kHEVC, kAllProfiles);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#endif
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_AV1)
  capability.video_codecs.emplace(media::VideoCodec::kAV1, kAllProfiles);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLacrosUseChromeosProtectedAv1)) {
    capability.video_codecs.emplace(media::VideoCodec::kAV1, kAllProfiles);
  }
#endif

  // Both encryption schemes are supported on ChromeOS.
  capability.encryption_schemes.insert(media::EncryptionScheme::kCenc);
  capability.encryption_schemes.insert(media::EncryptionScheme::kCbcs);

  // Both temporary and persistent sessions are supported on ChromeOS.
  capability.session_types.insert(media::CdmSessionType::kTemporary);
  capability.session_types.insert(media::CdmSessionType::kPersistentLicense);

  cdms->push_back(
      content::CdmInfo(kWidevineKeySystem, Robustness::kHardwareSecure,
                       std::move(capability), content::kChromeOsCdmType));
#endif  // BUILDFLAG(IS_ANDROID)
}

void AddWidevine(std::vector<content::CdmInfo>* cdms) {
  AddSoftwareSecureWidevine(cdms);
  AddHardwareSecureWidevine(cdms);
}
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
void AddExternalClearKey(std::vector<content::CdmInfo>* cdms) {
  // Register Clear Key CDM if specified in command line.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::FilePath clear_key_cdm_path =
      command_line->GetSwitchValuePath(switches::kClearKeyCdmPathForTesting);
  if (clear_key_cdm_path.empty() || !base::PathExists(clear_key_cdm_path)) {
    return;
  }

  // Supported codecs are hard-coded in ExternalClearKeyKeySystemInfo.
  media::CdmCapability capability(
      {}, {}, {media::EncryptionScheme::kCenc, media::EncryptionScheme::kCbcs},
      {media::CdmSessionType::kTemporary,
       media::CdmSessionType::kPersistentLicense});

  // Register media::kExternalClearKeyDifferentCdmTypeTestKeySystem first
  // separately. Otherwise, it'll be treated as a sub-key-system of normal
  // media::kExternalClearKeyKeySystem. See MultipleCdmTypes test in
  // ECKEncryptedMediaTest.
  cdms->push_back(content::CdmInfo(
      media::kExternalClearKeyDifferentCdmTypeTestKeySystem,
      Robustness::kSoftwareSecure, capability,
      /*supports_sub_key_systems=*/false, media::kClearKeyCdmDisplayName,
      media::kClearKeyCdmDifferentCdmType, base::Version("0.1.0.0"),
      clear_key_cdm_path));

  cdms->push_back(content::CdmInfo(
      media::kExternalClearKeyKeySystem, Robustness::kSoftwareSecure,
      capability,
      /*supports_sub_key_systems=*/true, media::kClearKeyCdmDisplayName,
      media::kClearKeyCdmType, base::Version("0.1.0.0"), clear_key_cdm_path));
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(IS_WIN)
void AddMediaFoundationClearKey(std::vector<content::CdmInfo>* cdms) {
  if (!base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting)) {
    return;
  }

  // Register MediaFoundation Clear Key CDM if specified in feature list.
  base::FilePath clear_key_cdm_path = base::FilePath::FromASCII(
      media::kMediaFoundationClearKeyCdmPathForTesting.Get());
  if (clear_key_cdm_path.empty() || !base::PathExists(clear_key_cdm_path)) {
    return;
  }

  // Supported codecs are hard-coded in ExternalClearKeyKeySystemInfo.
  media::CdmCapability capability(
      {}, {}, {media::EncryptionScheme::kCenc, media::EncryptionScheme::kCbcs},
      {media::CdmSessionType::kTemporary});

  cdms->push_back(
      content::CdmInfo(media::kMediaFoundationClearKeyKeySystem,
                       Robustness::kHardwareSecure, capability,
                       /*supports_sub_key_systems=*/false,
                       media::kMediaFoundationClearKeyCdmDisplayName,
                       media::kMediaFoundationClearKeyCdmType,
                       base::Version("0.1.0.0"), clear_key_cdm_path));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

void RegisterCdmInfo(std::vector<content::CdmInfo>* cdms) {
  DVLOG(1) << __func__;
  DCHECK(cdms);
  DCHECK(cdms->empty());

#if BUILDFLAG(ENABLE_WIDEVINE)
  AddWidevine(cdms);
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  AddExternalClearKey(cdms);
#endif

#if BUILDFLAG(IS_WIN)
  AddMediaFoundationClearKey(cdms);
#endif

#if BUILDFLAG(IS_ANDROID)
  cdm::AddOtherAndroidCdms(cdms);
#endif  // BUILDFLAG(IS_ANDROID)

  DVLOG(3) << __func__ << " done with " << cdms->size() << " cdms";
}

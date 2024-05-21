// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media/cdm_registration.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/cdm_info.h"
#include "media/base/cdm_capability.h"
#include "media/base/media_switches.h"
#include "media/cdm/cdm_type.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "third_party/widevine/cdm/buildflags.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/base/video_codecs.h"
#include "media/cdm/supported_audio_codecs.h"
#endif

#if BUILDFLAG(ENABLE_WIDEVINE)
#include "components/cdm/common/cdm_manifest.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/native_library.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/media/component_widevine_cdm_hint_file_linux.h"
#include "media/cdm/cdm_paths.h"  // nogncheck
// Needed for WIDEVINE_CDM_MIN_GLIBC_VERSION. This file is in
// SHARED_INTERMEDIATE_DIR.
#include "widevine_cdm_version.h"  // nogncheck
// The following must be after widevine_cdm_version.h.
#if defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
#include <gnu/libc-version.h>
#endif  // defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/metrics/histogram_functions.h"
#include "content/public/common/content_switches.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

#if BUILDFLAG(IS_ANDROID)
#include "components/cdm/common/android_cdm_registration.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

using Robustness = content::CdmInfo::Robustness;

#if BUILDFLAG(ENABLE_WIDEVINE)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LacrosBundledWidevine {
  kNone = 0,
  kLacros = 1,
  kAsh = 2,
  kMaxValue = kAsh,
};

// Record which Widevine CDM was loaded for Lacros.
void ReportLacrosUMA(LacrosBundledWidevine value) {
  // Only recorded by the browser process. If recorded by the pre-zygote
  // process, then all processes will end up with this value reported.
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty()) {
    base::UmaHistogramEnumeration("Media.EME.Widevine.LacrosBundledCdm", value);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
// sandbox. On Windows and Mac, CDM registration is handled by Component
// Update (as the CDM can be loaded only when needed).

// This code checks to see if the Widevine CDM was bundled with Chrome. If one
// can be found and looks valid, it returns the CdmInfo for the CDM. Otherwise
// it returns nullptr.
std::unique_ptr<content::CdmInfo> GetBundledWidevine() {
  // Ideally this would cache the result, as the bundled Widevine CDM is either
  // there or it's not. However, RegisterCdmInfo() will be called by different
  // processes (the pre-zygote process and the browser process), so caching it
  // as a static variable ends up with multiple copies anyways.
  base::FilePath install_dir;
  if (!base::PathService::Get(chrome::DIR_BUNDLED_WIDEVINE_CDM, &install_dir)) {
    return nullptr;
  }

  return CreateCdmInfoFromWidevineDirectory(install_dir);
}
#endif  // BUILDFLAG(BUNDLE_WIDEVINE_CDM) &&
        // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// ChromeOS Lacros should use the Widevine CDM bundled with ChromeOS Ash.
// This is determined by using command line arguments passed when Ash
// launches Lacros.
std::unique_ptr<content::CdmInfo> GetAshBundledWidevine() {
  if (base::FeatureList::IsEnabled(media::kLacrosUseAshWidevine)) {
    const auto* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kCrosWidevineBundledDir)) {
      ReportLacrosUMA(LacrosBundledWidevine::kAsh);
      base::FilePath install_dir =
          command_line->GetSwitchValuePath(switches::kCrosWidevineBundledDir);
      return CreateCdmInfoFromWidevineDirectory(install_dir);
    }
  }

#if BUILDFLAG(BUNDLE_WIDEVINE_CDM)
  // As there will be a transition period where Lacros runs on older versions of
  // Ash that do not set the command line argument, use the Widevine CDM bundled
  // with Lacros if available.
  // TODO(b/332962687): Remove Lacros bundled Widevine CDM once all versions of
  // Ash updated to set the command line argument.
  ReportLacrosUMA(LacrosBundledWidevine::kLacros);
  return GetBundledWidevine();
#else
  ReportLacrosUMA(LacrosBundledWidevine::kNone);
  return nullptr;
#endif  // BUILDFLAG(BUNDLE_WIDEVINE_CDM)
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if (BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT) &&             \
     (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH))) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
// This code checks to see if Component Updater picked a version of the Widevine
// CDM to be used last time it ran. (Component Updater may choose the bundled
// CDM if there is not a new version available for download.) If there is one
// and it looks valid, return the CdmInfo for that CDM. Otherwise return
// nullptr.
//
// On ChromeOS Lacros, Component Update for the Widevine CDM is disabled.
// However, as Lacros uses the Widevine CDM available to ChromeOS Ash, this code
// is needed to check to see if the Ash Widevine CDM has been updated.
std::unique_ptr<content::CdmInfo> GetHintedWidevine() {
  // Ideally this would cache the result, as Component Update may run and
  // download a new version once Chrome has been running for a while. However,
  // RegisterCdmInfo() will be called by different processes (the pre-zygote
  // process and the browser process), so caching it as a static variable ends
  // up with multiple copies anyways. As long as this is called before the
  // Component Update process for the Widevine CDM runs, it should return the
  // same version so what is loaded in the zygote is the same as what ends up
  // registered in the browser process. (This function also ends up being called
  // by tests, so caching the result means that we can't change what the test
  // pretends Component Update returns.)
  // TODO(crbug.com/324117290): Investigate if the pre-zygote data can be used
  // by the browser process so that RegisterCdmInfo() is only called once.
  auto install_dir = GetHintedWidevineCdmDirectory();
  if (install_dir.empty()) {
    DVLOG(1) << __func__ << ": no version available";
    return nullptr;
  }

  return CreateCdmInfoFromWidevineDirectory(install_dir);
}
#endif  // (BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT) && (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_ASH))) || BUILDFLAG(IS_CHROMEOS_LACROS)

void AddSoftwareSecureWidevine(std::vector<content::CdmInfo>* cdms) {
  DVLOG(1) << __func__;

#if BUILDFLAG(IS_ANDROID)
  // On Android Widevine is done by MediaDrm, and should be supported on all
  // devices. Register Widevine without any capabilities so that it will be
  // checked the first time some page attempts to play protected content.
  cdms->emplace_back(
      kWidevineKeySystem, Robustness::kSoftwareSecure, std::nullopt,
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

  // The Widevine CDM on Linux/ChromeOS needs to be registered (and loaded)
  // before the zygote is locked down. The CDM can be found from the version
  // bundled with Chrome (if BUNDLE_WIDEVINE_CDM = true) and/or the version
  // selected by Component Update (if ENABLE_WIDEVINE_CDM_COMPONENT = true).
  //
  // If both settings are set, then there are several scenarios that need to
  // be handled:
  // 1. First launch. There will only be a bundled CDM as Component Update
  //    hasn't run, so load the bundled CDM.
  // 2. Subsequent launches. Component Update should have run and updated the
  //    hint file. It could have selected the bundled version as the desired
  //    CDM, or downloaded a different version that should be used instead.
  //    In case of a version downgrade the bundled CDM version is saved so
  //    that we can detect the downgrade. Generally we should use the version
  //    selected by Component Update.
  // 3. New version of Chrome, containing a different bundled CDM. For this
  //    case we should select the CDM with the higher version.
  //
  // Note that Component Update will detect the bundled version, and if there is
  // no newer version available, select the bundled version. In this case both
  // versions will be the same and point to the same directory, so it doesn't
  // matter which one is loaded. In the case of a version downgrade, the CDM
  // selected by Component Update may have a lower version than the bundled CDM.
  // We should still use the version selected by Component Update (except for
  // case #3 above).
  std::unique_ptr<content::CdmInfo> bundled_widevine = nullptr;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bundled_widevine = GetAshBundledWidevine();
#elif BUILDFLAG(BUNDLE_WIDEVINE_CDM)
  bundled_widevine = GetBundledWidevine();
#endif

  // The hinted Widevine CDM is the CDM selected by Component Update. It may be
  // the bundled CDM if it matches the version Component Update determines that
  // should be used.
  std::unique_ptr<content::CdmInfo> hinted_widevine;
#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  hinted_widevine = GetHintedWidevine();
#endif

  if (bundled_widevine && !hinted_widevine) {
    // Only a bundled version is available, so use it.
    VLOG(1) << "Registering bundled Widevine " << bundled_widevine->version;
    cdms->push_back(*bundled_widevine);
  } else if (!bundled_widevine && hinted_widevine) {
    // Only a component updated version is available, so use it.
    VLOG(1) << "Registering hinted Widevine " << hinted_widevine->version;
    cdms->push_back(*hinted_widevine);
  } else if (!bundled_widevine && !hinted_widevine) {
    VLOG(1) << "Widevine enabled but no library found";
  } else {
    // Both a bundled CDM and a hinted CDM found, so choose between them.
    base::Version bundled_version = bundled_widevine->version;
    base::Version hinted_version = hinted_widevine->version;
    DVLOG(1) << __func__ << " bundled: " << bundled_version;
    DVLOG(1) << __func__ << " hinted: " << hinted_version;

    bool choose_bundled;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Downgrading doesn't work on Lacros, so choose the highest version CDM,
    // preferring the bundled CDM over the hinted CDM if the versions are the
    // same. See bug for details.
    // TODO(b/329869597): Get this working on Lacros.
    choose_bundled = bundled_version >= hinted_version;
#else
    // On all other platforms (Linux and ChromeOS Ash) we want to pick the
    // hinted version, except in the case the bundled CDM is newer than the
    // hinted CDM and is different than the previously bundled CDM.
    choose_bundled =
        bundled_version > hinted_version &&
        bundled_version != GetBundledVersionDuringLastComponentUpdate();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    if (choose_bundled) {
      VLOG(1) << "Choosing bundled Widevine " << bundled_version << " from "
              << bundled_widevine->path;
      cdms->push_back(*bundled_widevine);
    } else {
      VLOG(1) << "Choosing hinted Widevine " << hinted_version << " from "
              << hinted_widevine->path;
      cdms->push_back(*hinted_widevine);
    }
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
      kWidevineKeySystem, Robustness::kHardwareSecure, std::nullopt,
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

#if BUILDFLAG(ENABLE_WIDEVINE) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH))
std::vector<content::CdmInfo> GetSoftwareSecureWidevine() {
  std::vector<content::CdmInfo> cdms;
  AddSoftwareSecureWidevine(&cdms);
  return cdms;
}
#endif  // BUILDFLAG(ENABLE_WIDEVINE) && (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_ASH))

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media/cdm_registration.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "media/cdm/cdm_capability.h"
#include "third_party/widevine/cdm/buildflags.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "base/command_line.h"
#include "media/base/media_switches.h"
#include "media/cdm/cdm_paths.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_WIDEVINE)
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    BUILDFLAG(ENABLE_MEDIA_FOUNDATION_WIDEVINE_CDM)
#include "base/native_library.h"
#include "chrome/common/chrome_paths.h"
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) ||
        // BUILDFLAG(ENABLE_MEDIA_FOUNDATION_WIDEVINE_CDM)
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "base/no_destructor.h"
#include "components/cdm/common/cdm_manifest.h"
#include "media/cdm/supported_audio_codecs.h"
// TODO(crbug.com/663554): Needed for WIDEVINE_CDM_VERSION_STRING. Support
// component updated CDM on all desktop platforms and remove this.
// This file is In SHARED_INTERMEDIATE_DIR.
#include "widevine_cdm_version.h"  // nogncheck
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/common/media/component_widevine_cdm_hint_file_linux.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

namespace {

using Robustness = content::CdmInfo::Robustness;

#if BUILDFLAG(ENABLE_WIDEVINE)
#if (BUILDFLAG(BUNDLE_WIDEVINE_CDM) ||            \
     BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)) && \
    (defined(OS_LINUX) || defined(OS_CHROMEOS))
// Create a CdmInfo for a Widevine CDM, using |version|, |cdm_library_path|, and
// |capability|.
std::unique_ptr<content::CdmInfo> CreateWidevineCdmInfo(
    const base::Version& version,
    const base::FilePath& cdm_library_path,
    media::CdmCapability capability) {
  return std::make_unique<content::CdmInfo>(
      kWidevineKeySystem, Robustness::kSoftwareSecure, std::move(capability),
      /*supports_sub_key_systems=*/false, kWidevineCdmDisplayName,
      kWidevineCdmGuid, version, cdm_library_path, kWidevineCdmFileSystemId);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// On desktop Linux, given |cdm_base_path| that points to a folder containing
// the Widevine CDM and associated files, read the manifest included in that
// directory and create a CdmInfo. If that is successful, return the CdmInfo. If
// not, return nullptr.
std::unique_ptr<content::CdmInfo> CreateCdmInfoFromWidevineDirectory(
    const base::FilePath& cdm_base_path) {
  // Library should be inside a platform specific directory.
  auto cdm_library_path =
      media::GetPlatformSpecificDirectory(cdm_base_path)
          .Append(base::GetNativeLibraryName(kWidevineCdmLibraryName));
  if (!base::PathExists(cdm_library_path))
    return nullptr;

  // Manifest should be at the top level.
  auto manifest_path = cdm_base_path.Append(FILE_PATH_LITERAL("manifest.json"));
  base::Version version;
  media::CdmCapability capability;
  if (!ParseCdmManifestFromPath(manifest_path, &version, &capability))
    return nullptr;

  return CreateWidevineCdmInfo(version, cdm_library_path,
                               std::move(capability));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // (BUILDFLAG(BUNDLE_WIDEVINE_CDM) ||
        // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)) && (defined(OS_LINUX) ||
        // defined(OS_CHROMEOS))

#if BUILDFLAG(BUNDLE_WIDEVINE_CDM) && \
    (defined(OS_LINUX) || defined(OS_CHROMEOS))
// On Linux/ChromeOS we have to preload the CDM since it uses the zygote
// sandbox. On Windows and Mac, the bundled CDM is handled by the component
// updater.

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<content::CdmInfo> CreateCdmInfoForChromeOS(
    const base::FilePath& install_dir) {
  // On ChromeOS the Widevine CDM library is in the component directory and
  // does not have a manifest.
  // TODO(crbug.com/971433): Move Widevine CDM to a separate folder in the
  // component directory so that the manifest can be included.
  auto cdm_library_path =
      install_dir.Append(base::GetNativeLibraryName(kWidevineCdmLibraryName));
  if (!base::PathExists(cdm_library_path))
    return nullptr;

  // As there is no manifest, set |capability| as if it came from one. These
  // values must match the CDM that is being bundled with Chrome.
  media::CdmCapability capability;

  // Note that desktop CDMs only support decryption of audio content,
  // no decoding. Manifest does not contain any audio codecs, as decoding
  // will be done by the browser. So use the standard set of audio codecs
  // supported.
  capability.audio_codecs = media::GetCdmSupportedAudioCodecs();

  // Add the supported codecs as if they came from the component manifest.
  capability.video_codecs.push_back(media::VideoCodec::kCodecVP8);
  capability.video_codecs.push_back(media::VideoCodec::kCodecVP9);
  capability.video_codecs.push_back(media::VideoCodec::kCodecAV1);
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  capability.video_codecs.push_back(media::VideoCodec::kCodecH264);
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  // Both encryption schemes are supported on ChromeOS.
  capability.encryption_schemes.insert(media::EncryptionScheme::kCenc);
  capability.encryption_schemes.insert(media::EncryptionScheme::kCbcs);

  // Both temporary and persistent sessions are supported on ChromeOS.
  capability.session_types.insert(media::CdmSessionType::kTemporary);
  capability.session_types.insert(media::CdmSessionType::kPersistentLicense);

  return CreateWidevineCdmInfo(base::Version(WIDEVINE_CDM_VERSION_STRING),
                               cdm_library_path, std::move(capability));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
        // On ChromeOS the Widevine CDM library is in the component directory
        // (returned above) and does not have a manifest.
        // TODO(crbug.com/971433): Move Widevine CDM to a separate folder in
        // the component directory so that the manifest can be included.
        return CreateCdmInfoForChromeOS(install_dir);
#else
        // On desktop Linux the MANIFEST is bundled with the CDM.
        return CreateCdmInfoFromWidevineDirectory(install_dir);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      }());
  return s_cdm_info->get();
}
#endif  // BUILDFLAG(BUNDLE_WIDEVINE_CDM) && (defined(OS_LINUX) ||
        // defined(OS_CHROMEOS))

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT) && \
    (defined(OS_LINUX) || defined(OS_CHROMEOS))
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
        if (install_dir.empty())
          return nullptr;

        return CreateCdmInfoFromWidevineDirectory(install_dir);
      }());
  return s_cdm_info->get();
}
#endif  // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT) && (defined(OS_LINUX) ||
        // defined(OS_CHROMEOS))

void AddSoftwareSecureWidevine(std::vector<content::CdmInfo>* cdms) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
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
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)
}

void AddHardwareSecureWidevine(std::vector<content::CdmInfo>* cdms) {
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  media::CdmCapability capability;

  // The following audio formats are supported for decrypt-only.
  capability.audio_codecs = media::GetCdmSupportedAudioCodecs();

  // We currently support VP9, H264 and HEVC video formats with
  // decrypt-and-decode.
  capability.video_codecs.push_back(media::VideoCodec::kCodecVP9);
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  capability.video_codecs.push_back(media::VideoCodec::kCodecH264);
#endif
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  capability.video_codecs.push_back(media::VideoCodec::kCodecHEVC);
#endif

  // Both encryption schemes are supported on ChromeOS.
  capability.encryption_schemes.insert(media::EncryptionScheme::kCenc);
  capability.encryption_schemes.insert(media::EncryptionScheme::kCbcs);

  // Both temporary and persistent sessions are supported on ChromeOS.
  capability.session_types.insert(media::CdmSessionType::kTemporary);
  capability.session_types.insert(media::CdmSessionType::kPersistentLicense);

  // TODO(xhwang): Specify kChromeOsCdmFileSystemId here and update
  // MediaInterfaceProxy to use it.

  cdms->push_back(content::CdmInfo(
      kWidevineKeySystem, Robustness::kHardwareSecure, std::move(capability)));
#elif BUILDFLAG(ENABLE_MEDIA_FOUNDATION_WIDEVINE_CDM)
  // TODO(hmchen): Remove this after the Windows CDM is component updated.
  base::FilePath install_dir;
  if (!base::PathService::Get(chrome::DIR_BUNDLED_WIDEVINE_CDM, &install_dir))
    return;

  auto widevine_cdm_path = install_dir.AppendASCII(
      base::GetNativeLibraryName(kMediaFoundationWidevineCdmLibraryName));
  if (!base::PathExists(widevine_cdm_path))
    return;

  // Register Widevine hardware secure support for lazy initialization.
  // TODO(xhwang): Get the version from the DLL.
  VLOG(1) << "Registering " << kMediaFoundationWidevineCdmDisplayName;
  cdms->push_back(content::CdmInfo(
      kWidevineKeySystem, Robustness::kHardwareSecure, absl::nullopt,
      /*supports_sub_key_systems=*/false,
      kMediaFoundationWidevineCdmDisplayName, kMediaFoundationWidevineCdmGuid,
      base::Version(), widevine_cdm_path,
      /*file_system_id=*/""));
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
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
  if (clear_key_cdm_path.empty() || !base::PathExists(clear_key_cdm_path))
    return;

  // TODO(crbug.com/764480): Remove these after we have a central place for
  // External Clear Key (ECK) related information.
  // Normal External Clear Key key system.
  const char kExternalClearKeyKeySystem[] = "org.chromium.externalclearkey";
  // A variant of ECK key system that has a different GUID.
  const char kExternalClearKeyDifferentGuidTestKeySystem[] =
      "org.chromium.externalclearkey.differentguid";

  // Supported codecs are hard-coded in ExternalClearKeyProperties.
  media::CdmCapability capability(
      {}, {}, {media::EncryptionScheme::kCenc, media::EncryptionScheme::kCbcs},
      {media::CdmSessionType::kTemporary,
       media::CdmSessionType::kPersistentLicense});

  // Register kExternalClearKeyDifferentGuidTestKeySystem first separately.
  // Otherwise, it'll be treated as a sub-key-system of normal
  // kExternalClearKeyKeySystem. See MultipleCdmTypes test in
  // ECKEncryptedMediaTest.
  cdms->push_back(content::CdmInfo(
      kExternalClearKeyDifferentGuidTestKeySystem, Robustness::kSoftwareSecure,
      capability,
      /*supports_sub_key_systems=*/false, media::kClearKeyCdmDisplayName,
      media::kClearKeyCdmDifferentGuid, base::Version("0.1.0.0"),
      clear_key_cdm_path, media::kClearKeyCdmFileSystemId));

  cdms->push_back(content::CdmInfo(
      kExternalClearKeyKeySystem, Robustness::kSoftwareSecure, capability,
      /*supports_sub_key_systems=*/true, media::kClearKeyCdmDisplayName,
      media::kClearKeyCdmGuid, base::Version("0.1.0.0"), clear_key_cdm_path,
      media::kClearKeyCdmFileSystemId));
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

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
}

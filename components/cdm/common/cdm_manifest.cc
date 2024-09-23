// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/common/cdm_manifest.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "base/version.h"
#include "content/public/common/cdm_info.h"
#include "media/base/cdm_capability.h"
#include "media/base/content_decryption_module.h"
#include "media/base/decrypt_config.h"
#include "media/base/video_codecs.h"
#include "media/cdm/supported_audio_codecs.h"
#include "media/cdm/supported_cdm_versions.h"
#include "media/media_buildflags.h"

#if !BUILDFLAG(ENABLE_LIBRARY_CDMS)
#error This file should only be compiled when library CDMs are enabled
#endif

namespace {

// The CDM manifest includes several custom values, all beginning with "x-cdm-".
// They are:
//   x-cdm-module-versions
//   x-cdm-interface-versions
//   x-cdm-host-versions
//   x-cdm-codecs
//   x-cdm-persistent-license-support
//   x-cdm-supported-encryption-schemes
// What they represent is listed below. They should never have non-backwards
// compatible changes. All values are strings. All values that are lists are
// delimited by commas. No trailing commas. For example, "1,2,4".
const char kCdmValueDelimiter[] = ",";

// This field in the manifest is parsed by component updater code (e.g.
// `ComponentInstaller::FindPreinstallation()`) as well as in this file.
const char kCdmVersion[] = "version";

// The following entries are required.
//  Interface versions are lists of integers (e.g. "1" or "1,2,4").
//  All match the interface versions from content_decryption_module.h that the
//  CDM supports.
//    Matches CDM_MODULE_VERSION.
const char kCdmModuleVersionsName[] = "x-cdm-module-versions";
//    Matches supported ContentDecryptionModule_* version(s).
const char kCdmInterfaceVersionsName[] = "x-cdm-interface-versions";
//    Matches supported Host_* version(s).
const char kCdmHostVersionsName[] = "x-cdm-host-versions";
//  The codecs list is a list of simple codec names (e.g. "vp8,vorbis").
const char kCdmCodecsListName[] = "x-cdm-codecs";
//  Whether persistent license is supported by the CDM: "true" or "false".
const char kCdmPersistentLicenseSupportName[] =
    "x-cdm-persistent-license-support";
//  The list of supported encryption schemes (e.g. ["cenc","cbcs"]).
const char kCdmSupportedEncryptionSchemesName[] =
    "x-cdm-supported-encryption-schemes";

// The following strings are used to specify supported codecs in the
// parameter |kCdmCodecsListName|.
const char kCdmSupportedCodecVp8[] = "vp8";
// Supports at least VP9 profile 0 and profile 2.
const char kCdmSupportedCodecVp9[] = "vp09";
const char kCdmSupportedCodecAv1[] = "av01";
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
const char kCdmSupportedCodecAvc1[] = "avc1";
#endif

// The following strings are used to specify supported encryption schemes in
// the parameter |kCdmSupportedEncryptionSchemesName|.
const char kCdmSupportedEncryptionSchemeCenc[] = "cenc";
const char kCdmSupportedEncryptionSchemeCbcs[] = "cbcs";

typedef bool (*VersionCheckFunc)(int version);

// Returns whether the CDM's API version, as specified in the manifest by
// |version_name|, is supported in this Chrome binary and not disabled at run
// time by calling |version_check_func|. If the manifest entry contains multiple
// values, each one is checked sequentially, and if any one is supported, this
// function returns true. If all values in the manifest entry are not supported,
// then return false.
bool CheckForCompatibleVersion(const base::Value::Dict& manifest,
                               const std::string version_name,
                               VersionCheckFunc version_check_func) {
  auto* version_string = manifest.FindString(version_name);
  if (!version_string) {
    DVLOG(1) << "CDM manifest missing " << version_name;
    return false;
  }

  DVLOG_IF(1, version_string->empty())
      << "CDM manifest has empty " << version_name;

  for (std::string_view ver_str :
       base::SplitStringPiece(*version_string, kCdmValueDelimiter,
                              base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    int version = 0;
    if (base::StringToInt(ver_str, &version) && version_check_func(version))
      return true;
  }

  DVLOG(1) << "CDM manifest has no supported " << version_name << " in '"
           << *version_string << "'";
  return false;
}

// Returns true and updates |encryption_schemes| if the appropriate manifest
// entry is valid. Returns false and does not modify |encryption_schemes| if the
// manifest entry is incorrectly formatted. It is assumed that all CDMs support
// 'cenc', so if the manifest entry is missing, the result will indicate support
// for 'cenc' only. Incorrect types in the manifest entry will log the error and
// fail. Unrecognized values will be reported but otherwise ignored.
bool GetEncryptionSchemes(
    const base::Value::Dict& manifest,
    base::flat_set<media::EncryptionScheme>* encryption_schemes) {
  DCHECK(encryption_schemes);

  const base::Value* value = manifest.Find(kCdmSupportedEncryptionSchemesName);
  if (!value) {
    // No manifest entry found, so assume only 'cenc' supported for backwards
    // compatibility.
    encryption_schemes->insert(media::EncryptionScheme::kCenc);
    return true;
  }

  if (!value->is_list()) {
    DLOG(ERROR) << "CDM manifest entry " << kCdmSupportedEncryptionSchemesName
                << " is not a list.";
    return false;
  }

  base::flat_set<media::EncryptionScheme> result;
  for (const auto& item : value->GetList()) {
    if (!item.is_string()) {
      DLOG(ERROR) << "Unrecognized item type in CDM manifest entry "
                  << kCdmSupportedEncryptionSchemesName;
      return false;
    }

    const std::string& scheme = item.GetString();
    if (scheme == kCdmSupportedEncryptionSchemeCenc) {
      result.insert(media::EncryptionScheme::kCenc);
    } else if (scheme == kCdmSupportedEncryptionSchemeCbcs) {
      result.insert(media::EncryptionScheme::kCbcs);
    } else {
      DLOG(WARNING) << "Unrecognized encryption scheme '" << scheme
                    << "' in CDM manifest entry "
                    << kCdmSupportedEncryptionSchemesName;
    }
  }

  // As the manifest entry exists, it must specify at least one valid value.
  if (result.empty())
    return false;

  encryption_schemes->swap(result);
  return true;
}

// Returns true and updates |audio_codecs| with the full set of audio
// codecs that support decryption.
bool GetAudioCodecs(const base::Value::Dict& manifest,
                    base::flat_set<media::AudioCodec>* audio_codecs) {
  DCHECK(audio_codecs);

  // Note that desktop CDMs only support decryption of audio content,
  // no decoding. Manifest does not contain any audio codecs, so return
  // the standard set of audio codecs supported only if there is at least
  // one encryption scheme specified.
  base::flat_set<media::EncryptionScheme> encryption_schemes;
  if (!GetEncryptionSchemes(manifest, &encryption_schemes)) {
    return false;
  }

  DCHECK(!encryption_schemes.empty());
  *audio_codecs = media::GetCdmSupportedAudioCodecs();
  return true;
}

// Returns true and updates |video_codecs| if the appropriate manifest entry is
// valid. Returns false and does not modify |video_codecs| if the manifest entry
// is incorrectly formatted.
bool GetVideoCodecs(const base::Value::Dict& manifest,
                    media::CdmCapability::VideoCodecMap* video_codecs) {
  DCHECK(video_codecs);

  const base::Value* value = manifest.Find(kCdmCodecsListName);
  if (!value) {
    DLOG(WARNING) << "CDM manifest is missing codecs.";
    return true;
  }

  if (!value->is_string()) {
    DLOG(ERROR) << "CDM manifest entry " << kCdmCodecsListName
                << " is not a string.";
    return false;
  }

  const std::string& codecs = value->GetString();
  if (codecs.empty()) {
    DLOG(WARNING) << "CDM manifest has empty codecs list.";
    return true;
  }

  const std::vector<std::string_view> supported_codecs =
      base::SplitStringPiece(codecs, kCdmValueDelimiter, base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);

  // As the manifest string does not include profiles, specify {} to indicate
  // that all relevant profiles should be considered supported.
  media::CdmCapability::VideoCodecMap result;
  const media::VideoCodecInfo kAllProfiles;
  for (const auto& codec : supported_codecs) {
    if (codec == kCdmSupportedCodecVp8) {
      result.emplace(media::VideoCodec::kVP8, kAllProfiles);
    } else if (codec == kCdmSupportedCodecVp9) {
      result.emplace(media::VideoCodec::kVP9, kAllProfiles);
    } else if (codec == kCdmSupportedCodecAv1) {
      result.emplace(media::VideoCodec::kAV1, kAllProfiles);
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    } else if (codec == kCdmSupportedCodecAvc1) {
      result.emplace(media::VideoCodec::kH264, kAllProfiles);
#endif
    }
  }

  video_codecs->swap(result);
  return true;
}

// Returns true and updates |session_types| if the appropriate manifest entry is
// valid. Returns false if the manifest entry is incorrectly formatted.
bool GetSessionTypes(const base::Value::Dict& manifest,
                     base::flat_set<media::CdmSessionType>* session_types) {
  DCHECK(session_types);

  bool is_persistent_license_supported = false;
  const base::Value* value = manifest.Find(kCdmPersistentLicenseSupportName);
  if (value) {
    if (!value->is_bool())
      return false;
    is_persistent_license_supported = value->GetBool();
  }

  // Temporary session is always supported.
  session_types->insert(media::CdmSessionType::kTemporary);

  if (is_persistent_license_supported)
    session_types->insert(media::CdmSessionType::kPersistentLicense);

  return true;
}

bool GetVersion(const base::Value::Dict& manifest, base::Version* version) {
  auto* version_string = manifest.FindString(kCdmVersion);
  if (!version_string) {
    DLOG(ERROR) << "CDM manifest missing " << kCdmVersion;
    return false;
  }

  *version = base::Version(*version_string);
  if (!version->IsValid()) {
    DLOG(ERROR) << "CDM manifest version " << version_string << " is invalid.";
    return false;
  }

  return true;
}

}  // namespace

bool IsCdmManifestCompatibleWithChrome(const base::Value::Dict& manifest) {
  return CheckForCompatibleVersion(manifest, kCdmModuleVersionsName,
                                   media::IsSupportedCdmModuleVersion) &&
         CheckForCompatibleVersion(
             manifest, kCdmInterfaceVersionsName,
             media::IsSupportedAndEnabledCdmInterfaceVersion) &&
         CheckForCompatibleVersion(manifest, kCdmHostVersionsName,
                                   media::IsSupportedCdmHostVersion);
}

bool ParseCdmManifest(const base::Value::Dict& manifest,
                      media::CdmCapability* capability) {
  return GetAudioCodecs(manifest, &capability->audio_codecs) &&
         GetVideoCodecs(manifest, &capability->video_codecs) &&
         GetEncryptionSchemes(manifest, &capability->encryption_schemes) &&
         GetSessionTypes(manifest, &capability->session_types);
}

bool ParseCdmManifestFromPath(const base::FilePath& manifest_path,
                              base::Version* version,
                              media::CdmCapability* capability) {
  JSONFileValueDeserializer deserializer(manifest_path);
  int error_code;
  std::string error_message;
  std::unique_ptr<base::Value> manifest =
      deserializer.Deserialize(&error_code, &error_message);
  if (!manifest || !manifest->is_dict()) {
    DLOG(ERROR) << "Could not deserialize CDM manifest from " << manifest_path
                << ". Error: " << error_code << " / " << error_message;
    return false;
  }
  base::Value::Dict& manifest_dict = manifest->GetDict();

  return IsCdmManifestCompatibleWithChrome(manifest_dict) &&
         GetVersion(manifest_dict, version) &&
         ParseCdmManifest(manifest_dict, capability);
}

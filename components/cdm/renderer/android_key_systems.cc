// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/renderer/android_key_systems.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "components/cdm/renderer/android_key_system_info.h"
#include "content/public/renderer/render_thread.h"
#include "media/base/content_decryption_module.h"
#include "media/base/eme_constants.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#if BUILDFLAG(ENABLE_WIDEVINE)
#include "components/cdm/renderer/widevine_key_system_info.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

using media::CdmSessionType;
using media::EmeFeatureSupport;
using media::EncryptionScheme;
using media::KeySystemInfo;
#if BUILDFLAG(ENABLE_WIDEVINE)
using Robustness = cdm::WidevineKeySystemInfo::Robustness;
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

namespace cdm {

SupportedKeySystemResponse QueryKeySystemSupport(
    const std::string& key_system) {
  SupportedKeySystemRequest request;
  SupportedKeySystemResponse response;

  request.key_system = key_system;
  request.codecs = media::EME_CODEC_ALL;
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_QueryKeySystemSupport(request, &response));

  DCHECK(!(response.non_secure_codecs & ~media::EME_CODEC_ALL))
      << "unrecognized codec";
  DCHECK(!(response.secure_codecs & ~media::EME_CODEC_ALL))
      << "unrecognized codec";
  return response;
}

#if BUILDFLAG(ENABLE_WIDEVINE)
void AddAndroidWidevine(
    std::vector<std::unique_ptr<KeySystemInfo>>* key_systems) {
  // TODO(crbug.com/853336): Use media.mojom.KeySystemSupport instead of
  // separate IPC.
  auto response = QueryKeySystemSupport(kWidevineKeySystem);

  auto codecs = response.non_secure_codecs;

  // On Android, ".secure" codecs are all hardware secure codecs.
  auto hw_secure_codecs = response.secure_codecs;

  if (codecs == media::EME_CODEC_NONE) {
    // It doesn't make sense to support hw secure codecs but not regular codecs.
    DCHECK(hw_secure_codecs == media::EME_CODEC_NONE);
    DVLOG(3) << __func__ << " Widevine NOT supported.";
    return;
  }

  DVLOG(3) << __func__ << " Widevine supported.";

  base::flat_set<EncryptionScheme> encryption_schemes = {
      EncryptionScheme::kCenc};
  if (response.is_cbcs_encryption_supported) {
    encryption_schemes.insert(EncryptionScheme::kCbcs);
  }

  base::flat_set<CdmSessionType> session_types = {CdmSessionType::kTemporary};
  if (response.is_persistent_license_supported) {
    session_types.insert(CdmSessionType::kPersistentLicense);
  }

  // Since we do not control the implementation of the MediaDrm API on Android,
  // we assume that it can and will make use of persistence no matter whether
  // persistence-based features are supported or not.
  key_systems->emplace_back(new WidevineKeySystemInfo(
      codecs,                             // Regular codecs.
      encryption_schemes,                 // Encryption schemes.
      session_types,                      // Session types.
      hw_secure_codecs,                   // Hardware secure codecs.
      encryption_schemes,                 // Hardware secure encryption schemes.
      session_types,                      // Hardware secure Session types.
      Robustness::HW_SECURE_CRYPTO,       // Max audio robustness.
      Robustness::HW_SECURE_ALL,          // Max video robustness.
      EmeFeatureSupport::ALWAYS_ENABLED,  // Persistent state.
      EmeFeatureSupport::ALWAYS_ENABLED));  // Distinctive identifier.
}
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

void AddAndroidPlatformKeySystems(
    std::vector<std::unique_ptr<KeySystemInfo>>* key_systems) {
  // TODO(crbug.com/853336): Update media.mojom.KeySystemSupport to handle this
  // case and use it instead.

  std::vector<std::string> key_system_names;
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_GetPlatformKeySystemNames(&key_system_names));

  const base::flat_set<media::EncryptionScheme> encryption_schemes = {
      EncryptionScheme::kCenc};
  for (const auto& key_system : key_system_names) {
    SupportedKeySystemResponse response = QueryKeySystemSupport(key_system);
    if (response.non_secure_codecs != media::EME_CODEC_NONE) {
      key_systems->emplace_back(std::make_unique<AndroidKeySystemInfo>(
          key_system, response.non_secure_codecs, encryption_schemes,
          media::EME_CODEC_NONE, encryption_schemes));
    }
  }
}

}  // namespace cdm

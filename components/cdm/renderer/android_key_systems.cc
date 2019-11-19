// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/renderer/android_key_systems.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/renderer/render_thread.h"
#include "media/base/eme_constants.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#if BUILDFLAG(ENABLE_WIDEVINE)
#include "components/cdm/renderer/widevine_key_system_properties.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

using media::EmeConfigRule;
using media::EmeFeatureSupport;
using media::EmeInitDataType;
using media::EmeSessionTypeSupport;
using media::KeySystemProperties;
using media::SupportedCodecs;
#if BUILDFLAG(ENABLE_WIDEVINE)
using Robustness = cdm::WidevineKeySystemProperties::Robustness;
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

namespace cdm {

namespace {

// Implementation of KeySystemProperties for platform-supported key systems.
// Assumes that platform key systems support no features but can and will
// make use of persistence and identifiers.
class AndroidPlatformKeySystemProperties : public KeySystemProperties {
 public:
  AndroidPlatformKeySystemProperties(const std::string& name,
                                     SupportedCodecs supported_codecs)
      : name_(name), supported_codecs_(supported_codecs) {}

  std::string GetKeySystemName() const override { return name_; }

  bool IsSupportedInitDataType(EmeInitDataType init_data_type) const override {
    // Here we assume that support for a container implies support for the
    // associated initialization data type. KeySystems handles validating
    // |init_data_type| x |container| pairings.
    switch (init_data_type) {
      case EmeInitDataType::WEBM:
        return (supported_codecs_ & media::EME_CODEC_WEBM_ALL) != 0;
      case EmeInitDataType::CENC:
        return (supported_codecs_ & media::EME_CODEC_MP4_ALL) != 0;
      case EmeInitDataType::KEYIDS:
      case EmeInitDataType::UNKNOWN:
        return false;
    }
    NOTREACHED();
    return false;
  }

  EmeConfigRule GetEncryptionSchemeConfigRule(
      media::EncryptionScheme encryption_scheme) const override {
    return encryption_scheme == media::EncryptionScheme::kCenc
               ? EmeConfigRule::SUPPORTED
               : EmeConfigRule::NOT_SUPPORTED;
  }

  SupportedCodecs GetSupportedCodecs() const override {
    return supported_codecs_;
  }

  EmeConfigRule GetRobustnessConfigRule(
      media::EmeMediaType media_type,
      const std::string& requested_robustness) const override {
    return requested_robustness.empty() ? EmeConfigRule::SUPPORTED
                                        : EmeConfigRule::NOT_SUPPORTED;
  }

  EmeSessionTypeSupport GetPersistentLicenseSessionSupport() const override {
    return EmeSessionTypeSupport::NOT_SUPPORTED;
  }
  EmeSessionTypeSupport GetPersistentUsageRecordSessionSupport()
      const override {
    return EmeSessionTypeSupport::NOT_SUPPORTED;
  }
  EmeFeatureSupport GetPersistentStateSupport() const override {
    return EmeFeatureSupport::ALWAYS_ENABLED;
  }
  EmeFeatureSupport GetDistinctiveIdentifierSupport() const override {
    return EmeFeatureSupport::ALWAYS_ENABLED;
  }

 private:
  const std::string name_;
  const SupportedCodecs supported_codecs_;
};

}  // namespace

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
    std::vector<std::unique_ptr<KeySystemProperties>>* concrete_key_systems) {
  // TODO(crbug.com/853336): Use media.mojom.KeySystemSupport instead of
  // separate IPC.
  auto response = QueryKeySystemSupport(kWidevineKeySystem);

  auto codecs = response.non_secure_codecs;

  // On Android, ".secure" codecs are all hardware secure codecs.
  auto hw_secure_codecs = response.secure_codecs;

  // Since we do not control the implementation of the MediaDrm API on Android,
  // we assume that it can and will make use of persistence no matter whether
  // persistence-based features are supported or not.

  const EmeSessionTypeSupport persistent_license_support =
      response.is_persistent_license_supported
          ? EmeSessionTypeSupport::SUPPORTED_WITH_IDENTIFIER
          : EmeSessionTypeSupport::NOT_SUPPORTED;

  if (codecs != media::EME_CODEC_NONE) {
    DVLOG(3) << __func__ << " Widevine supported.";

    base::flat_set<media::EncryptionScheme> encryption_schemes = {
        media::EncryptionScheme::kCenc};
    if (response.is_cbcs_encryption_supported) {
      encryption_schemes.insert(media::EncryptionScheme::kCbcs);
    }

    concrete_key_systems->emplace_back(new WidevineKeySystemProperties(
        codecs,                        // Regular codecs.
        encryption_schemes,            // Encryption schemes.
        hw_secure_codecs,              // Hardware secure codecs.
        encryption_schemes,            // Hardware secure encryption schemes.
        Robustness::HW_SECURE_CRYPTO,  // Max audio robustness.
        Robustness::HW_SECURE_ALL,     // Max video robustness.
        persistent_license_support,    // persistent-license.
        EmeSessionTypeSupport::NOT_SUPPORTED,  // persistent-release-message.
        EmeFeatureSupport::ALWAYS_ENABLED,     // Persistent state.
        EmeFeatureSupport::ALWAYS_ENABLED));   // Distinctive identifier.
  } else {
    // It doesn't make sense to support hw secure codecs but not regular codecs.
    DVLOG(3) << __func__ << " Widevine NOT supported.";
    DCHECK(hw_secure_codecs == media::EME_CODEC_NONE);
  }
}
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

void AddAndroidPlatformKeySystems(
    std::vector<std::unique_ptr<KeySystemProperties>>* concrete_key_systems) {
  // TODO(crbug.com/853336): Update media.mojom.KeySystemSupport to handle this
  // case and use it instead.

  std::vector<std::string> key_system_names;
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_GetPlatformKeySystemNames(&key_system_names));

  for (std::vector<std::string>::const_iterator it = key_system_names.begin();
       it != key_system_names.end(); ++it) {
    SupportedKeySystemResponse response = QueryKeySystemSupport(*it);
    if (response.non_secure_codecs != media::EME_CODEC_NONE) {
      concrete_key_systems->emplace_back(new AndroidPlatformKeySystemProperties(
          *it, response.non_secure_codecs));
    }
  }
}

}  // namespace cdm

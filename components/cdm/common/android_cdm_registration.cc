// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/common/android_cdm_registration.h"

#include "base/logging.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/cdm/cdm_type.h"

#if BUILDFLAG(ENABLE_WIDEVINE)
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
#endif

namespace cdm {

#if BUILDFLAG(ENABLE_WIDEVINE)
void AddAndroidWidevineCdm(std::vector<content::CdmInfo>* cdms) {
  // Widevine is done by MediaDrm, and should be supported on all devices.
  // Register Widevine without any capabilities so that it will be
  // checked the first time it is used.
  cdms->emplace_back(kWidevineKeySystem,
                     content::CdmInfo::Robustness::kSoftwareSecure,
                     std::nullopt, kWidevineCdmType);
  cdms->emplace_back(kWidevineKeySystem,
                     content::CdmInfo::Robustness::kHardwareSecure,
                     std::nullopt, kWidevineCdmType);
}
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

void AddOtherAndroidCdms(std::vector<content::CdmInfo>* cdms) {
  // CdmInfo needs a CdmType, but on Android it is not used as the key system
  // is supported by MediaDrm. Using a random value as something needs to be
  // specified, but must be different than other CdmTypes specified.
  // (On Android the key system is identified by UUID, and that mapping is
  // maintained by MediaDrmBridge.)
  const media::CdmType kAndroidCdmType{0x2e9dabb9c171c28cull,
                                       0xf455252ec70b52adull};

  // MediaDrmBridge returns a list of key systems available on the device
  // that are not Widevine. Register them with no capabilities specified so
  // that lazy evaluation can figure out what is supported when requested.
  // We don't know if either software secure or hardware secure support is
  // available, so register them both. Lazy evaluation will remove them
  // if they aren't supported.
  const auto key_system_names =
      media::MediaDrmBridge::GetPlatformKeySystemNames();
  for (const auto& key_system : key_system_names) {
    DVLOG(3) << __func__ << " key_system:" << key_system;
    cdms->emplace_back(key_system,
                       content::CdmInfo::Robustness::kSoftwareSecure,
                       std::nullopt, kAndroidCdmType);
    cdms->emplace_back(key_system,
                       content::CdmInfo::Robustness::kHardwareSecure,
                       std::nullopt, kAndroidCdmType);
  }
}

}  // namespace cdm

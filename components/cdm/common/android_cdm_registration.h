// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_COMMON_ANDROID_CDM_REGISTRATION_H_
#define COMPONENTS_CDM_COMMON_ANDROID_CDM_REGISTRATION_H_

#include <vector>

#include "content/public/common/cdm_info.h"
#include "third_party/widevine/cdm/buildflags.h"

namespace cdm {

#if BUILDFLAG(ENABLE_WIDEVINE)
// Add Widevine Content Decryption Module, if enabled.
void AddAndroidWidevineCdm(std::vector<content::CdmInfo>* cdms);
#endif

// Add other platform-supported Widevine Content Decryption Modules which are
// not explicitly handled by Chrome.
void AddOtherAndroidCdms(std::vector<content::CdmInfo>* cdms);

}  // namespace cdm

#endif  // COMPONENTS_CDM_COMMON_ANDROID_CDM_REGISTRATION_H_

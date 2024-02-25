// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_CDM_REGISTRATION_H_
#define CHROME_COMMON_MEDIA_CDM_REGISTRATION_H_

#include <vector>

#include "build/build_config.h"
#include "content/public/common/cdm_info.h"
#include "third_party/widevine/cdm/buildflags.h"

// Register CdmInfo for Content Decryption Modules (CDM) supported.
void RegisterCdmInfo(std::vector<content::CdmInfo>* cdms);

#if BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(IS_LINUX)
// Returns the software secure Widevine CDM.
std::vector<content::CdmInfo> GetSoftwareSecureWidevineForTesting();
#endif

#endif  // CHROME_COMMON_MEDIA_CDM_REGISTRATION_H_

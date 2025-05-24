// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_COMMON_PLAYREADY_CDM_COMMON_H_
#define COMPONENTS_CDM_COMMON_PLAYREADY_CDM_COMMON_H_

#include <array>
#include <string>

#include "base/containers/contains.h"
#include "base/stl_util.h"
#include "base/token.h"
#include "media/cdm/cdm_type.h"

inline constexpr char kPlayReadyCdmDisplayName[] =
    "PlayReady Content Decryption Module";

inline constexpr media::CdmType kPlayReadyCdmType{0xCAF6576F591C4162ull,
                                                  0xB70FB8AE9AECD2B9ull};

// PlayReady KeySystem Strings
// https://learn.microsoft.com/en-us/playready/overview/key-system-strings
//
// "com.microsoft.playready.recommendation" without any robustness level
// specified represents a software secure (security level 2000) key system.
// If a robustness of "3000" is specified with this key system string then
// hardware secure (security level 3000) is used. Only hardware secure
// is supported.
//
// "com.microsoft.playready.recommendation.3000" does not require a robustness
// level to be specified. This always represents hardware secure (security
// level 3000). If a robustness of "3000" is specified with this key system
// string then it is ignored.
inline constexpr char kPlayReadyKeySystemRecommendationDefault[] =
    "com.microsoft.playready.recommendation";
inline constexpr char kPlayReadyKeySystemRecommendationHwSecure[] =
    "com.microsoft.playready.recommendation.3000";

inline constexpr std::array<const char*, 2> kPlayReadyKeySystems = {
    kPlayReadyKeySystemRecommendationDefault,
    kPlayReadyKeySystemRecommendationHwSecure};

inline bool IsPlayReadyHwSecureKeySystem(const std::string& key_system) {
  return key_system == kPlayReadyKeySystemRecommendationHwSecure;
}

inline bool IsPlayReadyKeySystem(const std::string& name) {
  return base::Contains(kPlayReadyKeySystems, name);
}

#endif  // COMPONENTS_CDM_COMMON_PLAYREADY_CDM_COMMON_H_

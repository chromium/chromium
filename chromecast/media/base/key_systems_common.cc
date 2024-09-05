// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/key_systems_common.h"

#include <cstddef>

#include "third_party/widevine/cdm/buildflags.h"

#if BUILDFLAG(ENABLE_WIDEVINE)
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
#endif

namespace chromecast {
namespace media {

CastKeySystem GetKeySystemByName(const std::string& key_system_name) {
#if BUILDFLAG(ENABLE_WIDEVINE)
  if (key_system_name == kWidevineKeySystem) {
    return KEY_SYSTEM_WIDEVINE;
  }
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

  return KEY_SYSTEM_NONE;
}

}  // namespace media
}  // namespace chromecast

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_LEGACY_APP_ID_MAPPER_H_
#define CHROMECAST_BASE_LEGACY_APP_ID_MAPPER_H_

#include <stdint.h>

#include <string>

namespace chromecast {

// Calculates the AppID associated with |id| and returns it, or
// |kAppUnknownFallback| if no such AppID exists.
uint32_t MapLegacyAppId(const std::string& id);

}  // namespace chromecast

#endif  // CHROMECAST_BASE_LEGACY_APP_ID_MAPPER_H_

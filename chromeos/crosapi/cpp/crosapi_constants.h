// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_CPP_CROSAPI_CONSTANTS_H_
#define CHROMEOS_CROSAPI_CPP_CROSAPI_CONSTANTS_H_

#include "base/component_export.h"

namespace crosapi {

COMPONENT_EXPORT(CROSAPI) extern const char kLacrosAppIdPrefix[];

COMPONENT_EXPORT(CROSAPI) extern const char kAshDataDir[];
COMPONENT_EXPORT(CROSAPI) extern const char kLacrosUserDataPath[];
COMPONENT_EXPORT(CROSAPI) extern const char kLacrosSharedDataPath[];

COMPONENT_EXPORT(CROSAPI) extern const char kSharedResourcesPackName[];
COMPONENT_EXPORT(CROSAPI) extern const char kSharedChrome100PercentPackName[];
COMPONENT_EXPORT(CROSAPI) extern const char kSharedChrome200PercentPackName[];

COMPONENT_EXPORT(CROSAPI) extern const char kChromeOSReleaseTrack[];

COMPONENT_EXPORT(CROSAPI) extern const char kReleaseChannelCanary[];
COMPONENT_EXPORT(CROSAPI) extern const char kReleaseChannelDev[];
COMPONENT_EXPORT(CROSAPI) extern const char kReleaseChannelBeta[];
COMPONENT_EXPORT(CROSAPI) extern const char kReleaseChannelStable[];
COMPONENT_EXPORT(CROSAPI) extern const char kReleaseChannelLtc[];
COMPONENT_EXPORT(CROSAPI) extern const char kReleaseChannelLts[];

COMPONENT_EXPORT(CROSAPI) extern const char kCrosapiMojoPlatformChannelHandle[];

}  // namespace crosapi

#endif  // CHROMEOS_CROSAPI_CPP_CROSAPI_CONSTANTS_H_

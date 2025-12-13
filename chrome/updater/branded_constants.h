// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_BRANDED_CONSTANTS_H_
#define CHROME_UPDATER_BRANDED_CONSTANTS_H_

#include "chrome/updater/updater_branding.h"

namespace updater {

// App ids.
inline constexpr char kUpdaterAppId[] = UPDATER_APPID;
inline constexpr char kQualificationAppId[] = QUALIFICATION_APPID;
inline constexpr char kPlatformExperienceHelperAppId[] =
    PLATFORM_EXPERIENCE_HELPER_APPID;

// Environment variables.
inline constexpr char kUsageStatsEnabled[] =
    COMPANY_SHORTNAME_UPPERCASE_STRING "_USAGE_STATS_ENABLED";
inline constexpr char kUsageStatsEnabledValueEnabled[] = "1";

inline constexpr char kSetupMutex[] = SETUP_MUTEX;

}  // namespace updater

#endif  // CHROME_UPDATER_BRANDED_CONSTANTS_H_

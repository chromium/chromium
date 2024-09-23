// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/constants.h"

#include "build/build_config.h"
#include "chrome/updater/updater_branding.h"

namespace updater {

// App ids.
const char kUpdaterAppId[] = UPDATER_APPID;
const char kQualificationAppId[] = QUALIFICATION_APPID;

// Environment variables.
const char kUsageStatsEnabled[] =
    COMPANY_SHORTNAME_UPPERCASE_STRING "_USAGE_STATS_ENABLED";
const char kUsageStatsEnabledValueEnabled[] = "1";

const char kSetupMutex[] = SETUP_MUTEX;

#if BUILDFLAG(IS_MAC)
// The user defaults suite name.
const char kUserDefaultsSuiteName[] = MAC_BUNDLE_IDENTIFIER_STRING ".defaults";
#endif  // BUILDFLAG(IS_MAC)

}  // namespace updater

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"

namespace ash::standalone_browser::features {

// On adding a new flag, please sort in the lexicographical order by
// the variable name.

// When this feature flag is enabled together with the LacrosAvailability
// policy, the Chrome app Kiosk session uses Lacros-chrome as the web browser to
// launch Chrome apps. When disabled, the Ash-chrome will be used instead.
BASE_FEATURE(kChromeKioskEnableLacros,
             "ChromeKioskEnableLacros",
             base::FEATURE_ENABLED_BY_DEFAULT);

// A kill switch for lacros chrome apps.
BASE_FEATURE(kLacrosDisableChromeApps,
             "LacrosDisableChromeApps",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Makes LaCrOS allowed for Family Link users.
// With this feature disabled LaCrOS cannot be enabled for Family Link users.
// When this feature is enabled LaCrOS availability is a under control of other
// launch switches.
// Note: Family Link users do not have access to chrome://flags and this feature
// flag is meant to help with development and testing.
BASE_FEATURE(kLacrosForSupervisedUsers,
             "LacrosForSupervisedUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables to use lacros-chrome as the only web browser on ChromeOS.
// This may not be allowed depending on user types and/or policies.
// NOTE: Use crosapi::browser_util::IsLacrosEnabled() instead of checking
// the feature directly.
BASE_FEATURE(kLacrosOnly, "LacrosOnly", base::FEATURE_DISABLED_BY_DEFAULT);

// Emergency switch to turn off profile migration.
BASE_FEATURE(kLacrosProfileMigrationForceOff,
             "LacrosProfileMigrationForceOff",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When this feature flag is enabled together with the LacrosAvailability
// policy, the web (PWA) Kiosk session uses Lacros-chrome as the web browser to
// launch web (PWA) applications. When disabled, the Ash-chrome will be used
// instead.
BASE_FEATURE(kWebKioskEnableLacros,
             "WebKioskEnableLacros",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace ash::standalone_browser::features

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/external_app_install_features.h"

#include "base/feature_list.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"

namespace web_app {

namespace {

// A hard coded list of features available for externally installed apps to
// gate their installation on via their config file settings. See
// |kFeatureName| in external_web_app_utils.h.
constexpr const base::Feature* kExternalAppInstallFeatures[] = {
    &kMigrateDefaultChromeAppToWebAppsGSuite,
    &kMigrateDefaultChromeAppToWebAppsNonGSuite,
};

bool g_always_enabled_for_testing = false;

}  // namespace

// Enables migration of default installed GSuite apps over to their replacement
// web apps.
const base::Feature kMigrateDefaultChromeAppToWebAppsGSuite{
  "MigrateDefaultChromeAppToWebAppsGSuite",
      base::FEATURE_DISABLED_BY_DEFAULT
};

// Enables migration of default installed non-GSuite apps over to their
// replacement web apps.
const base::Feature kMigrateDefaultChromeAppToWebAppsNonGSuite{
  "MigrateDefaultChromeAppToWebAppsNonGSuite",
      base::FEATURE_DISABLED_BY_DEFAULT
};

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Enables migration of default installed web apps over to their replacement
// web apps for Chrome OS beta channel users.
// This flag overrides the value of the kMigrateDefaultChromeAppToWebAppsGSuite
// and kMigrateDefaultChromeAppToWebAppsNonGSuite for Chrome OS beta users.
// Dev/canary/stable channels continue to use the above flags.
// We do this because:
//  - The Chrome OS migration flags used to be default enabled.
//  - Chrome OS beta channel got migrated.
//  - We changed the flags to be default disabled before it went out to stable.
//  - We want to avoid reverse migrating beta users (it will lose icon positions
//    in the shelf/launcher).
//  - Metrics team has advised us to use client side logic instead of a field
//    trial to maintain beta's migrated state.
// Note: This will all go away once the migration is complete.
const base::Feature kMigrateDefaultChromeAppToWebAppsChromeOsBeta{
    "MigrateDefaultChromeAppToWebAppsChromeOsBeta",
    base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

bool IsExternalAppInstallFeatureEnabled(base::StringPiece feature_name) {
  if (g_always_enabled_for_testing)
    return true;

  for (const base::Feature* feature : kExternalAppInstallFeatures) {
    if (feature->name == feature_name) {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
      // See |kMigrateDefaultChromeAppToWebAppsChromeOsBeta| comment above.
      if (chrome::GetChannel() == version_info::Channel::BETA &&
          (feature == &kMigrateDefaultChromeAppToWebAppsGSuite ||
           feature == &kMigrateDefaultChromeAppToWebAppsNonGSuite)) {
        return base::FeatureList::IsEnabled(
            kMigrateDefaultChromeAppToWebAppsChromeOsBeta);
      }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

      return base::FeatureList::IsEnabled(*feature);
    }
  }

  return false;
}

base::AutoReset<bool> SetExternalAppInstallFeatureAlwaysEnabledForTesting() {
  return base::AutoReset<bool>(&g_always_enabled_for_testing, true);
}

}  // namespace web_app

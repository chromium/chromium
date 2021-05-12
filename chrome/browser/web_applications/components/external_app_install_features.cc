// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/external_app_install_features.h"

#include "base/feature_list.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"

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

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
bool IsMigrationFeature(const base::Feature& feature) {
  return &feature == &kMigrateDefaultChromeAppToWebAppsGSuite ||
         &feature == &kMigrateDefaultChromeAppToWebAppsNonGSuite;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

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
// Whether to allow the MigrateDefaultChromeAppToWebAppsGSuite and
// MigrateDefaultChromeAppToWebAppsNonGSuite flags for managed users.
// Without this flag enabled managed users will not undergo the default web app
// migration.
//
// Why have a separate flag?
// Field trials are not able to accurately distinguish managed Chrome OS users.
// Because admin installed Chrome apps conflict with the default web app
// migration we need to maintain separate control over the rollout for mananged
// users.
const base::Feature kAllowDefaultWebAppMigrationForChromeOsManagedUsers{
    "AllowDefaultWebAppMigrationForChromeOsManagedUsers",
    base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

bool IsExternalAppInstallFeatureEnabled(base::StringPiece feature_name,
                                        const Profile& profile) {
  if (g_always_enabled_for_testing)
    return true;

  for (const base::Feature* feature : kExternalAppInstallFeatures) {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
    // See |kAllowDefaultWebAppMigrationForChromeOsManagedUsers| comment above.
    if (base::FeatureList::IsEnabled(*feature) &&
        feature->name == feature_name && IsMigrationFeature(*feature) &&
        profile.GetProfilePolicyConnector()->IsManaged()) {
      return base::FeatureList::IsEnabled(
          kAllowDefaultWebAppMigrationForChromeOsManagedUsers);
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

    if (feature->name == feature_name)
      return base::FeatureList::IsEnabled(*feature);
  }

  return false;
}

base::AutoReset<bool> SetExternalAppInstallFeatureAlwaysEnabledForTesting() {
  return base::AutoReset<bool>(&g_always_enabled_for_testing, true);
}

}  // namespace web_app

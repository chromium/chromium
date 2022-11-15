// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_app_install_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // IS_CHROMEOS

namespace web_app {

namespace {

// A hard coded list of features available for externally installed apps to
// gate their installation on via their config file settings. See
// |kFeatureName| in preinstalled_web_app_utils.h.
// After a feature flag has been shipped and should be cleaned up, move it into
// kShippedPreinstalledAppInstallFeatures to ensure any external installation
// configs that reference it continue to see it as enabled.
constexpr const base::Feature* kPreinstalledAppInstallFeatures[] = {
    &kMigrateDefaultChromeAppToWebAppsGSuite,
    &kMigrateDefaultChromeAppToWebAppsNonGSuite,
#if BUILDFLAG(IS_CHROMEOS)
    &kCursiveManagedStylusPreinstall,
    &kMessagesPreinstall,
#endif
};

constexpr const base::StringPiece kShippedPreinstalledAppInstallFeatures[] = {
    // Enables installing the PWA version of the chrome os calculator instead of
    // the deprecated chrome app.
    "DefaultCalculatorWebApp",
};

bool g_always_enabled_for_testing = false;

struct FeatureWithEnabledFunction {
  const char* const name;
  bool (*enabled_func)();
};

// Features which have a function to be run to determine whether they are
// enabled. Prefer using a base::Feature with |kPreinstalledAppInstallFeatures|
// when possible.
const FeatureWithEnabledFunction
    kPreinstalledAppInstallFeaturesWithEnabledFunctions[] = {
#if BUILDFLAG(IS_CHROMEOS)
        {chromeos::features::kCloudGamingDevice.name,
         &chromeos::features::IsCloudGamingDeviceEnabled}
#endif
};

// Checks if the feature being passed matches any of the migration features
// above.
bool IsMigrationFeature(const base::Feature& feature) {
  return &feature == &kMigrateDefaultChromeAppToWebAppsGSuite ||
         &feature == &kMigrateDefaultChromeAppToWebAppsNonGSuite;
}

}  // namespace

// Enables migration of default installed GSuite apps over to their replacement
// web apps.
BASE_FEATURE(kMigrateDefaultChromeAppToWebAppsGSuite,
             "MigrateDefaultChromeAppToWebAppsGSuite",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables migration of default installed non-GSuite apps over to their
// replacement web apps.
BASE_FEATURE(kMigrateDefaultChromeAppToWebAppsNonGSuite,
             "MigrateDefaultChromeAppToWebAppsNonGSuite",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
// Enables installing the Cursive app on managed devices with a built-in
// stylus-capable screen.
BASE_FEATURE(kCursiveManagedStylusPreinstall,
             "CursiveManagedStylusPreinstall",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables installing the Messages app on unmanaged devices.
BASE_FEATURE(kMessagesPreinstall,
             "MessagesPreinstall",
             base::FEATURE_ENABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_CHROMEOS)

bool IsPreinstalledAppInstallFeatureEnabled(base::StringPiece feature_name,
                                            const Profile& profile) {
  if (g_always_enabled_for_testing)
    return true;

  for (const base::StringPiece& feature :
       kShippedPreinstalledAppInstallFeatures) {
    if (feature == feature_name)
      return true;
  }

  for (const base::Feature* feature : kPreinstalledAppInstallFeatures) {
    if (feature->name == feature_name)
      return base::FeatureList::IsEnabled(*feature);
  }

  for (const auto& feature :
       kPreinstalledAppInstallFeaturesWithEnabledFunctions) {
    if (feature.name == feature_name)
      return feature.enabled_func();
  }

  return false;
}

bool IsAnyChromeAppToWebAppMigrationEnabled(const Profile& profile) {
  for (const base::Feature* feature : kPreinstalledAppInstallFeatures) {
    if (IsMigrationFeature(*feature)) {
      if (IsPreinstalledAppInstallFeatureEnabled(feature->name, profile)) {
        return true;
      }
    }
  }
  return false;
}

base::AutoReset<bool>
SetPreinstalledAppInstallFeatureAlwaysEnabledForTesting() {
  return base::AutoReset<bool>(&g_always_enabled_for_testing, true);
}

}  // namespace web_app

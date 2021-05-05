// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/preinstalled_app_install_features.h"

#include "base/feature_list.h"

namespace web_app {

namespace {

// A hard coded list of features available for externally installed apps to
// gate their installation on via their config file settings. See
// |kFeatureName| in preinstalled_web_app_utils.h.
constexpr const base::Feature* kPreinstalledAppInstallFeatures[] = {
    &kMigrateDefaultChromeAppToWebAppsGSuite,
    &kMigrateDefaultChromeAppToWebAppsNonGSuite,
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
    &kDefaultChatWebApp,
    &kDefaultMeetWebApp,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
};

bool g_always_enabled_for_testing = false;

}  // namespace

// Enables migration of default installed GSuite apps over to their replacement
// web apps.
const base::Feature kMigrateDefaultChromeAppToWebAppsGSuite {
  "MigrateDefaultChromeAppToWebAppsGSuite",
      base::FEATURE_DISABLED_BY_DEFAULT
};

// Enables migration of default installed non-GSuite apps over to their
// replacement web apps.
const base::Feature kMigrateDefaultChromeAppToWebAppsNonGSuite {
  "MigrateDefaultChromeAppToWebAppsNonGSuite",
      base::FEATURE_DISABLED_BY_DEFAULT
};

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Enables default installing the Chat web app.
const base::Feature kDefaultChatWebApp{"DefaultChatWebApp",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables default installing the Meet web app.
const base::Feature kDefaultMeetWebApp{"DefaultMeetWebApp",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

bool IsPreinstalledAppInstallFeatureEnabled(base::StringPiece feature_name) {
  if (g_always_enabled_for_testing)
    return true;

  for (const base::Feature* feature : kPreinstalledAppInstallFeatures) {
    if (feature->name == feature_name)
      return base::FeatureList::IsEnabled(*feature);
  }

  return false;
}

base::AutoReset<bool>
SetPreinstalledAppInstallFeatureAlwaysEnabledForTesting() {
  return base::AutoReset<bool>(&g_always_enabled_for_testing, true);
}

}  // namespace web_app

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_APP_INSTALL_FEATURES_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_APP_INSTALL_FEATURES_H_

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/strings/string_piece_forward.h"
#include "build/build_config.h"

class Profile;

namespace web_app {

extern const base::Feature kMigrateDefaultChromeAppToWebAppsGSuite;

extern const base::Feature kMigrateDefaultChromeAppToWebAppsNonGSuite;

extern const base::Feature kDefaultCalculatorWebApp;

#if BUILDFLAG(IS_CHROMEOS)
extern const base::Feature kAllowDefaultWebAppMigrationForChromeOsManagedUsers;

extern const base::Feature kCursiveStylusPreinstall;

extern const base::Feature kCursiveManagedStylusPreinstall;

extern const base::Feature kMessagesPreinstall;
#endif  // BUILDFLAG(IS_CHROMEOS)

// Returns the base::Feature in |kPreinstalledAppInstallFeatures| that
// corresponds to |feature_name|. Used by external app install configs to gate
// installation on features listed in |kPreinstalledAppInstallFeatures|.
bool IsPreinstalledAppInstallFeatureEnabled(base::StringPiece feature_name,
                                            const Profile& profile);

// Checks if migration flag is enabled on all OS.
bool IsAnyChromeAppToWebAppMigrationEnabled(const Profile& profile);

base::AutoReset<bool> SetPreinstalledAppInstallFeatureAlwaysEnabledForTesting();

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_APP_INSTALL_FEATURES_H_

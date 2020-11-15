// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_EXTERNAL_APP_INSTALL_FEATURES_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_EXTERNAL_APP_INSTALL_FEATURES_H_

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/strings/string_piece_forward.h"

namespace web_app {

extern const base::Feature kMigrateDefaultChromeAppToWebAppsGSuite;

extern const base::Feature kMigrateDefaultChromeAppToWebAppsNonGSuite;

// Returns the base::Feature in |kExternalAppInstallFeatures| that corresponds
// to |feature_name|. Used by external app install configs to gate installation
// on features listed in |kExternalAppInstallFeatures|.
bool IsExternalAppInstallFeatureEnabled(base::StringPiece feature_name);

base::AutoReset<bool> SetExternalAppInstallFeatureAlwaysEnabledForTesting();

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_EXTERNAL_APP_INSTALL_FEATURES_H_

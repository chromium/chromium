// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_APP_INSTALL_FEATURES_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_APP_INSTALL_FEATURES_H_

#include <string_view>

#include "base/auto_reset.h"

class Profile;

namespace web_app {

// Returns whether preinstalled Google Docs, Sheets, and Slides apps should
// display as standalone tabbed apps, and Drive as standalone but not
// tabbed, by default. Always false on non-CrOS.
bool IsPreinstalledDocsSheetsSlidesDriveStandaloneTabbed(Profile& profile);

// Returns the base::Feature in |kPreinstalledAppInstallFeatures| that
// corresponds to |feature_name|. Used by external app install configs to gate
// installation on features listed in |kPreinstalledAppInstallFeatures|.
bool IsPreinstalledAppInstallFeatureEnabled(std::string_view feature_name,
                                            const Profile& profile);

base::AutoReset<bool> SetPreinstalledAppInstallFeatureAlwaysEnabledForTesting();

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_APP_INSTALL_FEATURES_H_

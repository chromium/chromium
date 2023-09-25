// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_PREINSTALLED_WEB_APPS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_PREINSTALLED_WEB_APPS_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

bool PreinstalledWebAppsDisabled();

// Returns the list of web apps that should be pre-installed on new profiles.
std::vector<ExternalInstallOptions> GetPreinstalledWebApps();

// A subset of ExternalInstallOptions pertaining to web app migration.
struct PreinstalledWebAppMigration {
  PreinstalledWebAppMigration();
  PreinstalledWebAppMigration(PreinstalledWebAppMigration&&) noexcept;
  ~PreinstalledWebAppMigration();

  GURL install_url;
  webapps::AppId expected_web_app_id;
  webapps::AppId old_chrome_app_id;
};

// Returns the list of preinstalled web apps that are migrations away from their
// corresponding Chrome app.
std::vector<PreinstalledWebAppMigration> GetPreinstalledWebAppMigrations(
    Profile& profile);

// A scoped helper to provide a testing set of preinstalled app data. This will
// replace the default set.
struct ScopedTestingPreinstalledAppData {
  ScopedTestingPreinstalledAppData();
  ScopedTestingPreinstalledAppData(const ScopedTestingPreinstalledAppData&) =
      delete;
  ScopedTestingPreinstalledAppData& operator=(
      const ScopedTestingPreinstalledAppData&) = delete;
  ~ScopedTestingPreinstalledAppData();

  std::vector<ExternalInstallOptions> apps;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_PREINSTALLED_WEB_APPS_H_

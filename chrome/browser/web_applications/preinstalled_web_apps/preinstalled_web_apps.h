// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_PREINSTALLED_WEB_APPS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_PREINSTALLED_WEB_APPS_H_

#include <vector>

#include "chrome/browser/web_applications/components/external_install_options.h"

namespace web_app {

// Returns the list of web apps that should be pre-installed on new profiles.
std::vector<ExternalInstallOptions> GetPreinstalledWebApps();

// Preinstalled app configs are disabled in tests by default (having web apps
// install during start up adds a lot of noise). This allows tests to opt into
// having them install.
void ForceUsePreinstalledWebAppsForTesting();

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

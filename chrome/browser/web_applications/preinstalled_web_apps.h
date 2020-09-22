// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_H_

#include <string>
#include <vector>

#include "chrome/browser/web_applications/components/external_install_options.h"
#include "url/gurl.h"

namespace web_app {
struct ExternalInstallOptions;

// A simple struct that contains the relevant data for web apps that come
// preinstalled. These are used to generate the ExternalInstallOptions, which
// in turn are used to install the apps.
struct PreinstalledAppData {
  // The install url for the app.
  GURL install_url;
  // The name of a feature which must be enabled for the app to be installed.
  const char* feature_name = nullptr;
  // The ID of an existing app to uninstall when this app is installed.
  const char* app_id_to_replace = nullptr;
};

// A scoped helper to provide a testing set of preinstalled app data. This will
// replace the default set.
struct ScopedTestingPreinstalledAppData {
  ScopedTestingPreinstalledAppData();
  ScopedTestingPreinstalledAppData(const ScopedTestingPreinstalledAppData&) =
      delete;
  ScopedTestingPreinstalledAppData& operator=(
      const ScopedTestingPreinstalledAppData&) = delete;
  ~ScopedTestingPreinstalledAppData();

  std::vector<PreinstalledAppData> apps;
};

struct PreinstalledWebApps {
  PreinstalledWebApps();
  PreinstalledWebApps(PreinstalledWebApps&&);
  ~PreinstalledWebApps();

  std::vector<ExternalInstallOptions> options;
  int disabled_count = 0;
};

// Returns the list of web apps that should be pre-installed on new profiles.
PreinstalledWebApps GetPreinstalledWebApps();

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_H_

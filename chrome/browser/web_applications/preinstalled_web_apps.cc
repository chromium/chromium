// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps.h"

#include "base/feature_list.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"

namespace web_app {
namespace {

std::vector<ExternalInstallOptions>* g_preinstalled_app_data_for_testing =
    nullptr;

std::vector<ExternalInstallOptions> GetPreinstalledAppData() {
  if (g_preinstalled_app_data_for_testing)
    return *g_preinstalled_app_data_for_testing;

  return {
      // TODO(devlin): Add the web apps that should come preinstalled, gated by
      // OS.
  };
}

}  // namespace

ScopedTestingPreinstalledAppData::ScopedTestingPreinstalledAppData() {
  DCHECK_EQ(nullptr, g_preinstalled_app_data_for_testing);
  g_preinstalled_app_data_for_testing = &apps;
}

ScopedTestingPreinstalledAppData::~ScopedTestingPreinstalledAppData() {
  DCHECK_EQ(&apps, g_preinstalled_app_data_for_testing);
  g_preinstalled_app_data_for_testing = nullptr;
}

std::vector<ExternalInstallOptions> GetPreinstalledWebApps() {
  std::vector<ExternalInstallOptions> result;

  for (ExternalInstallOptions& app_data : GetPreinstalledAppData()) {
    DCHECK_EQ(app_data.install_source, ExternalInstallSource::kExternalDefault);

    // Preinstalled web apps should not have OS shortcuts of any kind.
    app_data.add_to_applications_menu = false;
    app_data.add_to_desktop = false;
    app_data.add_to_quick_launch_bar = false;
    app_data.add_to_search = false;
    app_data.add_to_management = false;
    app_data.require_manifest = true;
    result.push_back(std::move(app_data));
  }

  return result;
}

}  // namespace web_app

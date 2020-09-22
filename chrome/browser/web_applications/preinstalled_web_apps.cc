// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps.h"

#include "base/feature_list.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/chrome_features.h"

namespace web_app {
namespace {

std::vector<PreinstalledAppData>* g_preinstalled_app_data_for_testing = nullptr;

std::vector<PreinstalledAppData> GetPreinstalledAppData() {
  if (g_preinstalled_app_data_for_testing)
    return *g_preinstalled_app_data_for_testing;

  std::vector<PreinstalledAppData> preinstalled_app_data = {
      // TODO(devlin): Add the web apps that should come preinstalled, gated
      // by OS.
  };

  return preinstalled_app_data;
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

PreinstalledWebApps::PreinstalledWebApps() = default;
PreinstalledWebApps::PreinstalledWebApps(PreinstalledWebApps&&) = default;
PreinstalledWebApps::~PreinstalledWebApps() = default;

PreinstalledWebApps GetPreinstalledWebApps() {
  PreinstalledWebApps result;

  for (const PreinstalledAppData& app_data : GetPreinstalledAppData()) {
    if (!IsExternalAppInstallFeatureEnabled(app_data.feature_name)) {
      ++result.disabled_count;
      continue;
    }

    ExternalInstallOptions options(app_data.install_url, DisplayMode::kBrowser,
                                   ExternalInstallSource::kExternalDefault);
    // Preinstalled web apps should not have OS shortcuts of any kind.
    options.add_to_applications_menu = false;
    options.add_to_desktop = false;
    options.add_to_quick_launch_bar = false;
    options.add_to_search = false;
    options.add_to_management = false;
    options.require_manifest = true;
    options.uninstall_and_replace = {app_data.app_id_to_replace};
    result.options.push_back(std::move(options));
  }

  return result;
}

}  // namespace web_app

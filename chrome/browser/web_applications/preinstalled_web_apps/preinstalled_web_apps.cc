// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"

#include "base/feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/google_docs.h"

namespace web_app {
namespace {

std::vector<ExternalInstallOptions>* g_preinstalled_app_data_for_testing =
    nullptr;

std::vector<ExternalInstallOptions> GetPreinstalledAppData() {
  if (g_preinstalled_app_data_for_testing)
    return *g_preinstalled_app_data_for_testing;

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return {};
#endif

  // TODO(crbug.com/1104692): Replace these C++ configs with JSON configs like
  // those seen in: chrome/test/data/web_app_default_apps/good_json
  // This requires:
  // - Mimicking the directory packaging used by
  //   chrome/browser/resources/default_apps.
  // - Hooking up a second JSON config load to ExternalWebAppManager.
  // - Validating everything works on all OSs (Mac bundles things differently).
  // - Ensure that these resources are correctly installed by our Chrome
  //   installers on every desktop platform.
  return {
      GetConfigForGoogleDocs(),
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

#if !defined(OS_CHROMEOS)
    // Non-Chrome OS platforms are not permitted to fetch the web app install
    // URLs during start up.
    DCHECK(app_data.only_use_app_info_factory);
    DCHECK(app_data.app_info_factory);
#endif  // defined(OS_CHROMEOS)

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

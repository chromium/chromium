// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/chrome_switches.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/web_applications/preinstalled_web_apps/gmail.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/google_docs.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/google_drive.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/google_sheets.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/google_slides.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/youtube.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/web_applications/preinstalled_web_apps/google_calendar.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace web_app {
namespace {

std::vector<ExternalInstallOptions>* g_preinstalled_app_data_for_testing =
    nullptr;

bool g_force_use_preinstalled_web_apps_for_testing = false;

}  // namespace

std::vector<ExternalInstallOptions> GetPreinstalledWebApps() {
  if (g_preinstalled_app_data_for_testing)
    return *g_preinstalled_app_data_for_testing;

  if (!g_force_use_preinstalled_web_apps_for_testing &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kDisableDefaultApps)) {
    return {};
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
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
      // clang-format off
      GetConfigForGmail(),
      GetConfigForGoogleDocs(),
      GetConfigForGoogleDrive(),
      GetConfigForGoogleSheets(),
      GetConfigForGoogleSlides(),
      GetConfigForYouTube(),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      GetConfigForGoogleCalendar(),
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      // clang-format on
  };
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  return {};
}

void ForceUsePreinstalledWebAppsForTesting() {
  g_force_use_preinstalled_web_apps_for_testing = true;
}

ScopedTestingPreinstalledAppData::ScopedTestingPreinstalledAppData() {
  DCHECK_EQ(nullptr, g_preinstalled_app_data_for_testing);
  g_preinstalled_app_data_for_testing = &apps;
}

ScopedTestingPreinstalledAppData::~ScopedTestingPreinstalledAppData() {
  DCHECK_EQ(&apps, g_preinstalled_app_data_for_testing);
  g_preinstalled_app_data_for_testing = nullptr;
}

}  // namespace web_app

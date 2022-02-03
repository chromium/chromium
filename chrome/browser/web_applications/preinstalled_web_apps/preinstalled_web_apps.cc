// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/chrome_switches.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/web_applications/preinstalled_web_apps/gmail.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/google_docs.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/google_drive.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/google_sheets.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/google_slides.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/youtube.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/calculator.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/google_calendar.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/google_chat.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/google_meet.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace web_app {
namespace {

std::vector<ExternalInstallOptions>* g_preinstalled_app_data_for_testing =
    nullptr;

}  // namespace

std::vector<ExternalInstallOptions> GetPreinstalledWebApps() {
  if (g_preinstalled_app_data_for_testing)
    return *g_preinstalled_app_data_for_testing;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kDisablePreinstalledApps)) {
    return {};
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // TODO(crbug.com/1104692): Replace these C++ configs with JSON configs like
  // those seen in: chrome/test/data/web_app_default_apps/good_json
  // This requires:
  // - Mimicking the directory packaging used by
  //   chrome/browser/resources/default_apps.
  // - Hooking up a second JSON config load to PreinstalledWebAppManager.
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
#if BUILDFLAG(IS_CHROMEOS)
      GetConfigForCalculator(),
      GetConfigForGoogleCalendar(),
      GetConfigForGoogleChat(),
      GetConfigForGoogleMeet(),
#endif  // BUILDFLAG(IS_CHROMEOS)
      // clang-format on
  };
#else
  return {};
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

ScopedTestingPreinstalledAppData::ScopedTestingPreinstalledAppData() {
  DCHECK_EQ(nullptr, g_preinstalled_app_data_for_testing);
  g_preinstalled_app_data_for_testing = &apps;
}

ScopedTestingPreinstalledAppData::~ScopedTestingPreinstalledAppData() {
  DCHECK_EQ(&apps, g_preinstalled_app_data_for_testing);
  g_preinstalled_app_data_for_testing = nullptr;
}

PreinstalledWebAppMigration::PreinstalledWebAppMigration() = default;
PreinstalledWebAppMigration::PreinstalledWebAppMigration(
    PreinstalledWebAppMigration&&) = default;
PreinstalledWebAppMigration::~PreinstalledWebAppMigration() = default;

const std::vector<PreinstalledWebAppMigration>&
GetPreinstalledWebAppMigrations() {
  static base::NoDestructor<std::vector<PreinstalledWebAppMigration>>
      preinstalled_web_app_migrations([] {
        std::vector<PreinstalledWebAppMigration> migrations;
        for (const ExternalInstallOptions& options : GetPreinstalledWebApps()) {
          if (!options.expected_app_id ||
              options.uninstall_and_replace.size() != 1) {
            continue;
          }

          PreinstalledWebAppMigration migration;
          migration.install_url = options.install_url;
          migration.expected_web_app_id = *options.expected_app_id;
          migration.old_chrome_app_id = options.uninstall_and_replace[0];
          migration.gate_on_feature = options.gate_on_feature;
          migrations.push_back(std::move(migration));
        }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS)
        // Manually hard coded entries from
        // https://chrome-internal.googlesource.com/chromeos/overlays/chromeos-overlay/+/main/chromeos-base/chromeos-default-apps/files/web_apps
        // for any json configs that include a uninstall_and_replace field.
        // This is a temporary measure while the default web app duplication
        // issue is cleaned up.
        // TODO(crbug.com/1290716): Clean up once no longer needed.
        {
          PreinstalledWebAppMigration migration;
          migration.install_url = GURL(
              "https://play.google.com/books/installwebapp?usp=chromedefault");
          migration.expected_web_app_id = kPlayBooksAppId;
          migration.old_chrome_app_id = extension_misc::kGooglePlayBooksAppId;
          migration.gate_on_feature =
              kMigrateDefaultChromeAppToWebAppsNonGSuite.name;
          migrations.push_back(std::move(migration));
        }
        {
          PreinstalledWebAppMigration migration;
          migration.install_url =
              GURL("https://keep.google.com/installwebapp?usp=chrome_default");
          migration.expected_web_app_id = kGoogleKeepAppId;
          migration.old_chrome_app_id = extension_misc::kGoogleKeepAppId;
          migration.gate_on_feature =
              kMigrateDefaultChromeAppToWebAppsGSuite.name;
          migrations.push_back(std::move(migration));
        }
        {
          PreinstalledWebAppMigration migration;
          migration.install_url =
              GURL("https://www.google.com/maps/preview/pwa/ttinstall.html");
          migration.expected_web_app_id = kGoogleMapsAppId;
          migration.old_chrome_app_id = extension_misc::kGoogleMapsAppId;
          migration.gate_on_feature =
              kMigrateDefaultChromeAppToWebAppsNonGSuite.name;
          migrations.push_back(std::move(migration));
        }
        {
          PreinstalledWebAppMigration migration;
          migration.install_url = GURL(
              "https://play.google.com/store/movies/"
              "installwebapp?usp=chrome_default");
          migration.expected_web_app_id = kGoogleMoviesAppId;
          migration.old_chrome_app_id = extension_misc::kGooglePlayMoviesAppId;
          migration.gate_on_feature =
              kMigrateDefaultChromeAppToWebAppsNonGSuite.name;
          migrations.push_back(std::move(migration));
        }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS)
        return migrations;
      }());
  return *preinstalled_web_app_migrations;
}

}  // namespace web_app

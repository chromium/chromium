// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/calculator.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/google_calendar.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/google_chat.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/google_meet.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/messages_dogfood.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"
#include "google_apis/gaia/gaia_auth_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace web_app {
namespace {

std::vector<ExternalInstallOptions>* g_preinstalled_app_data_for_testing =
    nullptr;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_CHROMEOS)
bool IsGoogleInternalAccount() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return false;
  return gaia::IsGoogleInternalAccountEmail(profile->GetProfileUserName());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

std::vector<ExternalInstallOptions> GetChromeBrandedApps() {
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
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

bool PreinstalledWebAppsDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kDisableDefaultApps);
}

std::vector<ExternalInstallOptions> GetPreinstalledWebApps() {
  if (g_preinstalled_app_data_for_testing)
    return *g_preinstalled_app_data_for_testing;

  if (PreinstalledWebAppsDisabled())
    return {};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug/1346167): replace with config in admin console.
  if (IsGoogleInternalAccount()) {
    std::vector<ExternalInstallOptions> apps = GetChromeBrandedApps();
    apps.push_back(GetConfigForMessagesDogfood());
    return apps;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  return GetChromeBrandedApps();
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
    PreinstalledWebAppMigration&&) noexcept = default;
PreinstalledWebAppMigration::~PreinstalledWebAppMigration() = default;

std::vector<PreinstalledWebAppMigration> GetPreinstalledWebAppMigrations(
    Profile& profile) {
  std::vector<PreinstalledWebAppMigration> migrations;
  for (const ExternalInstallOptions& options : GetPreinstalledWebApps()) {
    if (!options.expected_app_id)
      continue;

    // All entries in the default web app migration had only one Chrome app to
    // replace.
    if (options.uninstall_and_replace.size() != 1)
      continue;

    if (options.gate_on_feature && !IsPreinstalledAppInstallFeatureEnabled(
                                       *options.gate_on_feature, profile)) {
      continue;
    }

    PreinstalledWebAppMigration migration;
    migration.install_url = options.install_url;
    migration.expected_web_app_id = *options.expected_app_id;
    migration.old_chrome_app_id = options.uninstall_and_replace[0];
    migrations.push_back(std::move(migration));
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS)
  if (!g_preinstalled_app_data_for_testing && !PreinstalledWebAppsDisabled()) {
    // Manually hard coded entries from
    // https://chrome-internal.googlesource.com/chromeos/overlays/chromeos-overlay/+/main/chromeos-base/chromeos-default-apps/files/web_apps
    // for any json configs that include a uninstall_and_replace field.
    // This is a temporary measure while the default web app duplication
    // issue is cleaned up.
    // TODO(crbug.com/1290716): Clean up once no longer needed.
    if (IsPreinstalledAppInstallFeatureEnabled(
            kMigrateDefaultChromeAppToWebAppsGSuite.name, profile)) {
      PreinstalledWebAppMigration keep_migration;
      keep_migration.install_url =
          GURL("https://keep.google.com/installwebapp?usp=chrome_default");
      keep_migration.expected_web_app_id = kGoogleKeepAppId;
      keep_migration.old_chrome_app_id = extension_misc::kGoogleKeepAppId;
      migrations.push_back(std::move(keep_migration));
    }

    if (IsPreinstalledAppInstallFeatureEnabled(
            kMigrateDefaultChromeAppToWebAppsNonGSuite.name, profile)) {
      PreinstalledWebAppMigration books_migration;
      books_migration.install_url =
          GURL("https://play.google.com/books/installwebapp?usp=chromedefault");
      books_migration.expected_web_app_id = kPlayBooksAppId;
      books_migration.old_chrome_app_id = extension_misc::kGooglePlayBooksAppId;
      migrations.push_back(std::move(books_migration));

      PreinstalledWebAppMigration maps_migration;
      maps_migration.install_url =
          GURL("https://www.google.com/maps/preview/pwa/ttinstall.html");
      maps_migration.expected_web_app_id = kGoogleMapsAppId;
      maps_migration.old_chrome_app_id = extension_misc::kGoogleMapsAppId;
      migrations.push_back(std::move(maps_migration));

      PreinstalledWebAppMigration movies_migration;
      movies_migration.install_url = GURL(
          "https://play.google.com/store/movies/"
          "installwebapp?usp=chrome_default");
      movies_migration.expected_web_app_id = kGoogleMoviesAppId;
      movies_migration.old_chrome_app_id =
          extension_misc::kGooglePlayMoviesAppId;
      migrations.push_back(std::move(movies_migration));
    }
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS)
  return migrations;
}

}  // namespace web_app

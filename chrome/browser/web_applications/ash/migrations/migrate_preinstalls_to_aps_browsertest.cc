// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/ash/migrations/migrate_preinstalls_to_aps.h"

#include <vector>

#include "ash/constants/web_app_id_constants.h"
#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_definition_utils.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "base/one_shot_event.h"
#include "base/run_loop.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#endif

namespace web_app::migrations {
namespace {

ExternalInstallOptions GetGmailConfig() {
  ExternalInstallOptions options(
      /*install_url=*/GURL(
          "https://mail.google.com/mail/installwebapp?usp=chrome_default"),
      /*user_display_mode=*/mojom::UserDisplayMode::kStandalone,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.uninstall_and_replace.push_back("pjkljhegncpnkpknbcohdijeoejaedia");
  options.disable_if_tablet_form_factor = true;
  options.load_and_await_service_worker_registration = false;
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    GURL start_url = GURL("https://mail.google.com/mail/?usp=installed_webapp");
    // `manifest_id` must remain fixed even if start_url changes.
    webapps::ManifestId manifest_id = GenerateManifestIdFromStartUrlOnly(
        GURL("https://mail.google.com/mail/?usp=installed_webapp"));
    auto info = std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
    info->title = u"Gmail";
    info->scope = GURL("https://mail.google.com/mail/");
    info->display_mode = DisplayMode::kBrowser;
    return info;
  });
  options.expected_app_id = kGmailAppId;

  return options;
}

ExternalInstallOptions GetGoogleCalendarConfig() {
  ExternalInstallOptions options(
      /*install_url=*/GURL("https://calendar.google.com/calendar/"
                           "installwebapp?usp=chrome_default"),
      /*user_display_mode=*/mojom::UserDisplayMode::kStandalone,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.uninstall_and_replace.push_back("ejjicmeblgpmajnghnpcppodonldlgfn");
  options.disable_if_tablet_form_factor = true;
  options.load_and_await_service_worker_registration = false;
  options.launch_query_params = "usp=installed_webapp";

  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    GURL start_url = GURL("https://calendar.google.com/calendar/r");
    // `manifest_id` must remain fixed even if start_url changes.
    webapps::ManifestId manifest_id = GenerateManifestIdFromStartUrlOnly(
        GURL("https://calendar.google.com/calendar/r"));
    auto info = std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
    info->title = u"Google Calendar";
    info->scope = GURL("https://calendar.google.com/calendar/");
    info->display_mode = DisplayMode::kStandalone;
    return info;
  });
  options.expected_app_id = kGoogleCalendarAppId;

  return options;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// MigratePreinstallsToApsToggleTest uses PRE_ tests to validate toggling
// feature::kPreinstalledWebAppsCoreOnly between restarts.
class MigratePreinstallsToApsToggleTest : public InProcessBrowserTest {
 public:
  MigratePreinstallsToApsToggleTest() {
    // Turn kPreinstalledWebAppsCoreOnly feature on or off as required.
    std::string_view test_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();
    if (test_name == "PRE_TurnOn" || test_name == "Rollback") {
      feature_list_.InitAndDisableFeature(
          chromeos::features::kPreinstalledWebAppsCoreOnly);
    } else {
      feature_list_.InitAndEnableFeature(
          chromeos::features::kPreinstalledWebAppsCoreOnly);
    }
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);

    // Reenable default apps from PrepareBrowserCommandLineForTests().
    command_line->RemoveSwitch(switches::kDisableDefaultApps);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    base::RunLoop run_loop;
    WebAppProvider::GetForTest(profile())
        ->on_external_managers_synchronized()
        .Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  std::vector<webapps::AppId> GetAppIdsWithSources(
      WebAppManagementTypes source) {
    std::vector<webapps::AppId> result;
    WebAppRegistrar& registrar =
        WebAppProvider::GetForTest(profile())->registrar_unsafe();
    for (const auto& app_id : registrar.GetAppIds()) {
      if (registrar.GetAppById(app_id)->GetSources() == source) {
        result.push_back(app_id);
      }
    }
    return result;
  }

  void ValidateHistograms(int install, int source_removed, int app_removed) {
    histograms_.ExpectUniqueSample("WebApp.Preinstalled.InstallCount", install,
                                   1);
    histograms_.ExpectUniqueSample(
        "WebApp.Preinstalled.UninstallSourceRemovedCount", source_removed, 1);
    histograms_.ExpectUniqueSample(
        "WebApp.Preinstalled.UninstallAppRemovedCount", app_removed, 1);
    histograms_.ExpectUniqueSample("WebApp.Preinstalled.UninstallTotalCount",
                                   source_removed + app_removed, 1);
  }

  Profile* profile() { return browser()->profile(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histograms_;
};

// All apps should be installed as kDefault.
IN_PROC_BROWSER_TEST_F(MigratePreinstallsToApsToggleTest, PRE_TurnOn) {
  EXPECT_THAT(GetAppIdsWithSources(
                  WebAppManagementTypes({WebAppManagement::Type::kDefault})),
              testing::UnorderedElementsAre(
                  kContainerAppId, kGmailAppId, kGoogleDocsAppId,
                  kGoogleDriveAppId, kGoogleSheetsAppId, kGoogleSlidesAppId,
                  kYoutubeAppId, kGoogleCalendarAppId));
  ValidateHistograms(/*install=*/11, /*source_removed=*/0, /*app_removed=*/0);
}

// Non-core apps (calendar) should be migrated to kApsDefault.
IN_PROC_BROWSER_TEST_F(MigratePreinstallsToApsToggleTest, TurnOn) {
  EXPECT_THAT(GetAppIdsWithSources(
                  WebAppManagementTypes({WebAppManagement::Type::kDefault})),
              testing::UnorderedElementsAre(
                  kGmailAppId, kGoogleDocsAppId, kGoogleDriveAppId,
                  kGoogleSheetsAppId, kGoogleSlidesAppId, kYoutubeAppId));
  EXPECT_THAT(GetAppIdsWithSources(
                  WebAppManagementTypes({WebAppManagement::Type::kApsDefault})),
              testing::ElementsAre(kContainerAppId, kGoogleCalendarAppId));
  ValidateHistograms(/*install=*/6, /*source_removed=*/2, /*app_removed=*/0);
}

// Core apps will be preinstalled, we must simulate APS installing calendar.
IN_PROC_BROWSER_TEST_F(MigratePreinstallsToApsToggleTest, PRE_Rollback) {
  EXPECT_THAT(GetAppIdsWithSources(
                  WebAppManagementTypes({WebAppManagement::Type::kDefault})),
              testing::UnorderedElementsAre(
                  kGmailAppId, kGoogleDocsAppId, kGoogleDriveAppId,
                  kGoogleSheetsAppId, kGoogleSlidesAppId, kYoutubeAppId));

  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://calendar.google.com/calendar/r"));
  webapps::AppId app_id =
      test::InstallWebApp(profile(), std::move(web_app_info),
                          /*overwrite_existing_manifest_fields=*/false,
                          webapps::WebappInstallSource::PRELOADED_DEFAULT);
  ASSERT_EQ(app_id, kGoogleCalendarAppId);
  EXPECT_THAT(GetAppIdsWithSources(
                  WebAppManagementTypes({WebAppManagement::Type::kApsDefault})),
              testing::ElementsAre(kGoogleCalendarAppId));
  ValidateHistograms(/*install=*/6, /*source_removed=*/0, /*app_removed=*/0);
}

// Core apps are kDefault, calendar is kDefault and kApsDefault.
IN_PROC_BROWSER_TEST_F(MigratePreinstallsToApsToggleTest, Rollback) {
  EXPECT_THAT(GetAppIdsWithSources(
                  WebAppManagementTypes({WebAppManagement::Type::kDefault})),
              testing::UnorderedElementsAre(kContainerAppId, kGmailAppId,
                                            kGoogleDocsAppId, kGoogleDriveAppId,
                                            kGoogleSheetsAppId,
                                            kGoogleSlidesAppId, kYoutubeAppId));
  EXPECT_THAT(GetAppIdsWithSources(
                  WebAppManagementTypes({WebAppManagement::Type::kDefault,
                                         WebAppManagement::Type::kApsDefault})),
              testing::ElementsAre(kGoogleCalendarAppId));
  ValidateHistograms(/*install=*/9, /*source_removed=*/0, /*app_removed=*/0);
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// MigratePreinstallsToApsSkipStartupTest skips the PWAM startup and allows
// targeted testing of MigratePreinstallsToAps().
class MigratePreinstallsToApsSkipStartupTest : public InProcessBrowserTest {
 public:
  MigratePreinstallsToApsSkipStartupTest()
      : skip_preinstalled_web_app_startup_(
            PreinstalledWebAppManager::SkipStartupForTesting()) {}

  Profile* profile() { return browser()->profile(); }

  void SyncApps(std::vector<ExternalInstallOptions> apps) {
    ScopedTestingPreinstalledAppData preinstalled_apps;
    preinstalled_apps.apps = apps;
    base::test::TestFuture<
        std::map<GURL /*install_url*/,
                 ExternallyManagedAppManager::InstallResult>,
        std::map<GURL /*install_url*/, webapps::UninstallResultCode>>
        result;
    WebAppProvider::GetForTest(profile())
        ->preinstalled_web_app_manager()
        .LoadAndSynchronizeForTesting(result.GetCallback());

    CHECK(result.Wait());
  }

 private:
  base::AutoReset<bool> skip_preinstalled_web_app_startup_;
};

IN_PROC_BROWSER_TEST_F(MigratePreinstallsToApsSkipStartupTest,
                       MigrateChangesDefaultToApsDefault) {
  WebAppProvider* provider = WebAppProvider::GetForTest(profile());
  WebAppRegistrar& registrar = provider->registrar_unsafe();

  // Install gmail (core) and calendar (non-core).
  SyncApps({GetGmailConfig(), GetGoogleCalendarConfig()});

  EXPECT_THAT(registrar.GetAppIds(),
              testing::UnorderedElementsAre(kGmailAppId, kGoogleCalendarAppId));
  auto expected = WebAppManagementTypes({WebAppManagement::Type::kDefault});
  EXPECT_EQ(registrar.GetAppById(kGmailAppId)->GetSources(), expected);
  EXPECT_EQ(registrar.GetAppById(kGoogleCalendarAppId)->GetSources(), expected);
  // Migration should add kApsDefault for non-core apps.
  migrations::MigratePreinstallsToAps(&provider->sync_bridge_unsafe());
  EXPECT_THAT(registrar.GetAppIds(),
              testing::UnorderedElementsAre(kGmailAppId, kGoogleCalendarAppId));
  EXPECT_EQ(registrar.GetAppById(kGmailAppId)->GetSources(),
            WebAppManagementTypes({WebAppManagement::Type::kDefault}));
  EXPECT_EQ(registrar.GetAppById(kGoogleCalendarAppId)->GetSources(),
            WebAppManagementTypes({WebAppManagement::Type::kDefault,
                                   WebAppManagement::Type::kApsDefault}));

  // Sync with core only should remove kDefault from non-core apps.
  SyncApps({GetGmailConfig()});
  EXPECT_THAT(registrar.GetAppIds(),
              testing::UnorderedElementsAre(kGmailAppId, kGoogleCalendarAppId));
  EXPECT_EQ(registrar.GetAppById(kGmailAppId)->GetSources(),
            WebAppManagementTypes({WebAppManagement::Type::kDefault}));
  EXPECT_EQ(registrar.GetAppById(kGoogleCalendarAppId)->GetSources(),
            WebAppManagementTypes({WebAppManagement::Type::kApsDefault}));
}

IN_PROC_BROWSER_TEST_F(MigratePreinstallsToApsSkipStartupTest,
                       AppsAreRemovedWithoutMigration) {
  // Install gmail (core) and calendar (non-core).
  SyncApps({GetGmailConfig(), GetGoogleCalendarConfig()});
  // Syncing only core apps without migration should remove other apps.
  SyncApps({GetGmailConfig()});
  EXPECT_THAT(
      WebAppProvider::GetForTest(profile())->registrar_unsafe().GetAppIds(),
      testing::ElementsAre(kGmailAppId));
}

IN_PROC_BROWSER_TEST_F(MigratePreinstallsToApsSkipStartupTest,
                       AppsAreNotRemovedWithMigration) {
  WebAppProvider* provider = WebAppProvider::GetForTest(profile());
  WebAppRegistrar& registrar = provider->registrar_unsafe();

  // Install gmail (core) and calendar (non-core), then do migration.
  SyncApps({GetGmailConfig(), GetGoogleCalendarConfig()});
  migrations::MigratePreinstallsToAps(&provider->sync_bridge_unsafe());

  // Syncing only core apps after migration should not remove APS apps.
  SyncApps({GetGmailConfig()});
  EXPECT_THAT(registrar.GetAppIds(),
              testing::UnorderedElementsAre(kGmailAppId, kGoogleCalendarAppId));
  // Core apps should have both kDefault and kApsDefault sources.
  EXPECT_EQ(registrar.GetAppById(kGmailAppId)->GetSources(),
            WebAppManagementTypes({WebAppManagement::Type::kDefault}));
  // APS apps should have only kApsDefault source.
  EXPECT_EQ(registrar.GetAppById(kGoogleCalendarAppId)->GetSources(),
            WebAppManagementTypes({WebAppManagement::Type::kApsDefault}));
}

IN_PROC_BROWSER_TEST_F(MigratePreinstallsToApsSkipStartupTest,
                       RollbackSetsAllAppsWithBothSources) {
  WebAppProvider* provider = WebAppProvider::GetForTest(profile());
  WebAppRegistrar& registrar = provider->registrar_unsafe();

  // Install gmail (core) and calendar (non-core), then do migration.
  SyncApps({GetGmailConfig(), GetGoogleCalendarConfig()});
  migrations::MigratePreinstallsToAps(&provider->sync_bridge_unsafe());

  // Rollback (syncing all apps) should leave migrated apps with both sources.
  SyncApps({GetGmailConfig(), GetGoogleCalendarConfig()});
  EXPECT_THAT(registrar.GetAppIds(),
              testing::UnorderedElementsAre(kGmailAppId, kGoogleCalendarAppId));
  EXPECT_EQ(registrar.GetAppById(kGmailAppId)->GetSources(),
            WebAppManagementTypes({WebAppManagement::Type::kDefault}));
  EXPECT_EQ(registrar.GetAppById(kGoogleCalendarAppId)->GetSources(),
            WebAppManagementTypes({WebAppManagement::Type::kDefault,
                                   WebAppManagement::Type::kApsDefault}));
}

}  // namespace
}  // namespace web_app::migrations

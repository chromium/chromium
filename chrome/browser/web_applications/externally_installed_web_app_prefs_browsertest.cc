// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/json/json_reader.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/externally_installed_prefs_migration_metrics.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace web_app {

class ExternallyInstalledWebAppPrefsBrowserTest
    : public WebAppControllerBrowserTest {
 public:
  ExternallyInstalledWebAppPrefsBrowserTest() = default;
  ExternallyInstalledWebAppPrefsBrowserTest(
      const ExternallyInstalledWebAppPrefsBrowserTest&) = delete;
  ExternallyInstalledWebAppPrefsBrowserTest& operator=(
      const ExternallyInstalledWebAppPrefsBrowserTest&) = delete;
  ~ExternallyInstalledWebAppPrefsBrowserTest() override = default;

  void SimulateInstallApp(std::unique_ptr<WebApp> web_app) {
    ScopedRegistryUpdate update(&provider().sync_bridge());
    update->CreateApp(std::move(web_app));
  }

  base::flat_set<GURL> GetAppUrls(ExternalInstallSource install_source) {
    base::flat_set<GURL> urls;
    for (const auto& id_and_url :
         provider().registrar_unsafe().GetExternallyInstalledApps(
             install_source)) {
      for (const auto& url : id_and_url.second) {
        urls.emplace(url);
      }
    }
    return urls;
  }

  bool IsExternalPrefMigrationReadFromWebAppDBEnabled() {
    return base::FeatureList::IsEnabled(
        features::kUseWebAppDBInsteadOfExternalPrefs);
  }
};

class ExternallyInstalledWebAppPrefsBrowserTest_ExternalPrefMigration
    : public ExternallyInstalledWebAppPrefsBrowserTest,
      public testing::WithParamInterface<test::ExternalPrefMigrationTestCases> {
 public:
  ExternallyInstalledWebAppPrefsBrowserTest_ExternalPrefMigration() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    switch (GetParam()) {
      case test::ExternalPrefMigrationTestCases::kDisableMigrationReadPref:
        disabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        disabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
      case test::ExternalPrefMigrationTestCases::kDisableMigrationReadDB:
        disabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        enabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
      case test::ExternalPrefMigrationTestCases::kEnableMigrationReadPref:
        enabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        disabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
      case test::ExternalPrefMigrationTestCases::kEnableMigrationReadDB:
        enabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        enabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    ExternallyInstalledWebAppPrefsBrowserTest_ExternalPrefMigration,
    BasicOps) {
  GURL url_a("https://a.example.com/");
  AppId id_a;

  // Start with no data in the DB.
  WebAppRegistrar& registrar = provider().registrar_unsafe();
  EXPECT_FALSE(registrar.LookupExternalAppId(url_a).has_value());
  EXPECT_FALSE(registrar.HasExternalApp(id_a));

  EXPECT_EQ(base::flat_set<GURL>({}),
            GetAppUrls(ExternalInstallSource::kInternalDefault));
  EXPECT_EQ(base::flat_set<GURL>({}),
            GetAppUrls(ExternalInstallSource::kExternalDefault));
  EXPECT_EQ(base::flat_set<GURL>({}),
            GetAppUrls(ExternalInstallSource::kExternalPolicy));

  // Add some entries.
  auto web_app = test::CreateWebApp();
  id_a = web_app->app_id();
  SimulateInstallApp(std::move(web_app));
  test::AddInstallUrlData(profile()->GetPrefs(), &provider().sync_bridge(),
                          id_a, url_a, ExternalInstallSource::kExternalDefault);

  EXPECT_EQ(id_a, registrar.LookupExternalAppId(url_a).value_or("missing"));
  EXPECT_TRUE(registrar.HasExternalApp(id_a));

  EXPECT_EQ(base::flat_set<GURL>({url_a}),
            GetAppUrls(ExternalInstallSource::kExternalDefault));

  // Uninstalling an underlying app still stores data in the external
  // prefs, but the registrar removes the app.
  test::UninstallWebApp(profile(), id_a);
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  if (IsExternalPrefMigrationReadFromWebAppDBEnabled()) {
    EXPECT_FALSE(registrar.HasExternalApp(id_a));
    EXPECT_FALSE(registrar.LookupExternalAppId(url_a).has_value());
  } else {
    EXPECT_TRUE(registrar.HasExternalApp(id_a));
    EXPECT_EQ(id_a, registrar.LookupExternalAppId(url_a).value_or("missing"));
  }
}

IN_PROC_BROWSER_TEST_P(
    ExternallyInstalledWebAppPrefsBrowserTest_ExternalPrefMigration,
    IsPlaceholderApp) {
  const GURL url("https://example.com");
  auto web_app_install_info = std::make_unique<WebAppInstallInfo>();
  web_app_install_info->start_url = url;
  web_app_install_info->title = u"App Title";
  web_app_install_info->display_mode = DisplayMode::kBrowser;
  web_app_install_info->user_display_mode = UserDisplayMode::kStandalone;
  web_app_install_info->install_url = url;

  AppId app_id =
      test::InstallWebApp(profile(), std::move(web_app_install_info),
                          /*overwrite_existing_manifest_fields=*/true,
                          webapps::WebappInstallSource::EXTERNAL_POLICY);
  ExternallyInstalledWebAppPrefs prefs(profile()->GetPrefs());
  prefs.Insert(url, app_id, ExternalInstallSource::kExternalPolicy);
  EXPECT_FALSE(provider().registrar_unsafe().IsPlaceholderApp(
      app_id, WebAppManagement::kPolicy));
  prefs.SetIsPlaceholder(url, true);
  {
    ScopedRegistryUpdate update(&provider().sync_bridge());
    WebApp* installed_app = update->UpdateApp(app_id);
    if (installed_app)
      installed_app->AddPlaceholderInfoToManagementExternalConfigMap(
          WebAppManagement::kPolicy, /*is_placeholder=*/true);
  }
  EXPECT_TRUE(provider().registrar_unsafe().IsPlaceholderApp(
      app_id, WebAppManagement::kPolicy));
}

IN_PROC_BROWSER_TEST_P(
    ExternallyInstalledWebAppPrefsBrowserTest_ExternalPrefMigration,
    OldPrefFormat) {
  // Set up the old format for this pref {url -> app_id}.
  ScopedDictPrefUpdate update(profile()->GetPrefs(),
                              prefs::kWebAppsExtensionIDs);
  update->Set("https://example.com", "add_id_string");
  // This should not crash on invalid pref data.
  EXPECT_FALSE(provider().registrar_unsafe().IsPlaceholderApp(
      "app_id_string", WebAppManagement::kPolicy));
}

// Case 1: Single Install URL per source, no placeholder info (defaults to
// false).
IN_PROC_BROWSER_TEST_F(ExternallyInstalledWebAppPrefsBrowserTest,
                       WebAppParsedDataOneInstallURLPerSource) {
  base::Value external_prefs = *base::JSONReader::Read(R"({
    "https://app1.com/": {
      "extension_id": "test_app1",
      "install_source": 2
    },
    "https://app2.com/": {
      "extension_id": "test_app1",
      "install_source": 3
    }
  })");
  profile()->GetPrefs()->Set(prefs::kWebAppsExtensionIDs,
                             std::move(external_prefs));
  ExternallyInstalledWebAppPrefs::ParsedPrefs web_app_data_map =
      ExternallyInstalledWebAppPrefs::ParseExternalPrefsToWebAppData(
          profile()->GetPrefs());

  // Set up expected_data
  ExternallyInstalledWebAppPrefs::ParsedPrefs expected_map;
  WebApp::ExternalManagementConfig config1;
  config1.is_placeholder = false;
  config1.install_urls = {GURL("https://app1.com/")};
  expected_map["test_app1"][WebAppManagement::kPolicy] = std::move(config1);
  WebApp::ExternalManagementConfig config2;
  config2.is_placeholder = false;
  config2.install_urls = {GURL("https://app2.com/")};
  expected_map["test_app1"][WebAppManagement::kSystem] = std::move(config2);

  EXPECT_EQ(web_app_data_map, expected_map);
}

// Case 2: Multiple Install URL per source, with is_placeholder set to true.
IN_PROC_BROWSER_TEST_F(ExternallyInstalledWebAppPrefsBrowserTest,
                       WebAppDataMapMultipleInstallURLPerSource) {
  base::Value external_prefs = *base::JSONReader::Read(R"({
    "https://app1.com/": {
      "extension_id": "test_app1",
      "install_source": 0
    },
    "https://app2.com/": {
      "extension_id": "test_app1",
      "install_source": 0,
      "is_placeholder": true
    }
  })");
  profile()->GetPrefs()->Set(prefs::kWebAppsExtensionIDs,
                             std::move(external_prefs));
  ExternallyInstalledWebAppPrefs::ParsedPrefs web_app_data_map =
      ExternallyInstalledWebAppPrefs::ParseExternalPrefsToWebAppData(
          profile()->GetPrefs());

  // Set up expected_data
  ExternallyInstalledWebAppPrefs::ParsedPrefs expected_map;
  WebApp::ExternalManagementConfig config;
  config.is_placeholder = true;
  config.install_urls = {GURL("https://app1.com/"), GURL("https://app2.com/")};
  expected_map["test_app1"][WebAppManagement::kDefault] = std::move(config);

  EXPECT_EQ(web_app_data_map, expected_map);
}

// Edge cases:
// 1. pref entry with no data.
// 2. pref entry with all data.
// 3. pref entry with no source but existing app_id (won't generate map
// without knowing which source to correspond with).
// 4. pref entry with source but no app_id (won't generate map because app
// ID does not exist).
IN_PROC_BROWSER_TEST_F(ExternallyInstalledWebAppPrefsBrowserTest,
                       WebAppDataMapMultipleAppIDsMultipleURLs) {
  base::Value external_prefs = *base::JSONReader::Read(R"({
    "https://app1.com/": {
      "extension_id": "test_app1",
      "install_source": 1
    },
    "https://app2.com/": {
      "extension_id": "test_app1",
      "install_source": 1
    },
    "https://app3.com/": {},
    "https://app4.com/": {
      "extension_id": "test_app4",
      "install_source": 4,
      "is_placeholder": true
    },
    "https://app5.com/": {
      "extension_id": "test_app4",
      "is_placeholder": true
    },
    "https://app6.com/": {
      "install_source": 4
    }
  })");
  profile()->GetPrefs()->Set(prefs::kWebAppsExtensionIDs,
                             std::move(external_prefs));
  ExternallyInstalledWebAppPrefs::ParsedPrefs web_app_data_map =
      ExternallyInstalledWebAppPrefs::ParseExternalPrefsToWebAppData(
          profile()->GetPrefs());

  // Set up expected_data
  ExternallyInstalledWebAppPrefs::ParsedPrefs expected_map;
  WebApp::ExternalManagementConfig config1;
  config1.is_placeholder = false;
  config1.install_urls = {GURL("https://app1.com/"), GURL("https://app2.com/")};
  WebApp::ExternalManagementConfig config2;
  config2.is_placeholder = true;
  config2.install_urls = {GURL("https://app4.com/")};
  expected_map["test_app1"][WebAppManagement::kDefault] = std::move(config1);
  expected_map["test_app4"][WebAppManagement::kWebAppStore] =
      std::move(config2);

  EXPECT_EQ(web_app_data_map, expected_map);
}

IN_PROC_BROWSER_TEST_P(
    ExternallyInstalledWebAppPrefsBrowserTest_ExternalPrefMigration,
    MigrationTestForSingleSource) {
  const GURL url("https://example.com/");
  auto web_app_install_info = std::make_unique<WebAppInstallInfo>();
  web_app_install_info->start_url = url;
  web_app_install_info->title = u"App Title";
  web_app_install_info->display_mode = DisplayMode::kBrowser;
  web_app_install_info->user_display_mode = UserDisplayMode::kStandalone;
  web_app_install_info->install_url = url;

  AppId app_id =
      test::InstallWebApp(profile(), std::move(web_app_install_info),
                          /*overwrite_existing_manifest_fields=*/true,
                          webapps::WebappInstallSource::EXTERNAL_POLICY);

  const WebApp* installed_app =
      provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(provider().registrar_unsafe().IsPlaceholderApp(
      app_id, WebAppManagement::kPolicy));

  GURL install_url("https://app.com/install");
  ExternallyInstalledWebAppPrefs external_prefs(profile()->GetPrefs());
  external_prefs.Insert(install_url, app_id,
                        ExternalInstallSource::kExternalPolicy);
  external_prefs.SetIsPlaceholder(install_url, true);

  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      profile()->GetPrefs(), &provider().sync_bridge());

  // Verify install source, placeholder info and urls have been migrated.
  EXPECT_TRUE(provider().registrar_unsafe().IsPlaceholderApp(
      app_id, WebAppManagement::kPolicy));
  const WebApp::ExternalConfigMap& config_map =
      installed_app->management_to_external_config_map();
  EXPECT_EQ(1u, config_map.size());
  auto it = config_map.find(WebAppManagement::kPolicy);
  EXPECT_NE(it, config_map.end());
  EXPECT_TRUE(base::Contains(it->second.install_urls, install_url));
}

IN_PROC_BROWSER_TEST_F(ExternallyInstalledWebAppPrefsBrowserTest,
                       UserUninstalledPreInstalledWebAppMigrationSingleApp) {
  ExternallyInstalledWebAppPrefs external_prefs(profile()->GetPrefs());
  UserUninstalledPreinstalledWebAppPrefs preinstalled_prefs(
      profile()->GetPrefs());
  AppId app_id1 = "test_app1";
  AppId app_id2 = "test_app2";
  external_prefs.Insert(GURL("https://app1.com/install"), app_id1,
                        ExternalInstallSource::kExternalDefault);
  external_prefs.SetIsPlaceholder(GURL("https://app1.com/install"), true);

  external_prefs.Insert(GURL("https://app2.com/install"), app_id2,
                        ExternalInstallSource::kArc);
  external_prefs.SetIsPlaceholder(GURL("https://app2.com/install"), true);

  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      profile()->GetPrefs(), &provider().sync_bridge());

  // On migration, only a app_id1 should be migrated.
  EXPECT_TRUE(preinstalled_prefs.DoesAppIdExist(app_id1));
  EXPECT_FALSE(preinstalled_prefs.DoesAppIdExist(app_id2));
}

IN_PROC_BROWSER_TEST_F(
    ExternallyInstalledWebAppPrefsBrowserTest,
    UserUninstalledPreInstalledWebAppMigrationSingleAppNonDefault) {
  ExternallyInstalledWebAppPrefs external_prefs(profile()->GetPrefs());
  UserUninstalledPreinstalledWebAppPrefs preinstalled_prefs(
      profile()->GetPrefs());
  AppId app_id1 = "test_app1";
  external_prefs.Insert(GURL("https://app1.com/install"), app_id1,
                        ExternalInstallSource::kArc);
  external_prefs.SetIsPlaceholder(GURL("https://app1.com/install"), true);

  external_prefs.Insert(GURL("https://app2.com/install"), app_id1,
                        ExternalInstallSource::kExternalPolicy);
  external_prefs.SetIsPlaceholder(GURL("https://app2.com/install"), true);

  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      profile()->GetPrefs(), &provider().sync_bridge());

  // On migration, nothing is migrated because default installs do not exist in
  // the external prefs.
  EXPECT_FALSE(preinstalled_prefs.DoesAppIdExist(app_id1));
}

IN_PROC_BROWSER_TEST_F(
    ExternallyInstalledWebAppPrefsBrowserTest,
    UserUninstalledPreInstalledWebAppMigrationSingleAppMultiURL) {
  ExternallyInstalledWebAppPrefs external_prefs(profile()->GetPrefs());
  UserUninstalledPreinstalledWebAppPrefs preinstalled_prefs(
      profile()->GetPrefs());
  AppId app_id1 = "test_app1";
  external_prefs.Insert(GURL("https://app1.com/install"), app_id1,
                        ExternalInstallSource::kInternalDefault);
  external_prefs.SetIsPlaceholder(GURL("https://app1.com/install"), true);

  external_prefs.Insert(GURL("https://app2.com/install"), app_id1,
                        ExternalInstallSource::kExternalDefault);
  external_prefs.SetIsPlaceholder(GURL("https://app2.com/install"), true);

  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      profile()->GetPrefs(), &provider().sync_bridge());

  // On migration, everything (app_ids and both URLs should be migrated).
  EXPECT_TRUE(preinstalled_prefs.DoesAppIdExist(app_id1));
  EXPECT_EQ(app_id1, preinstalled_prefs.LookUpAppIdByInstallUrl(
                         GURL("https://app1.com/install")));
  EXPECT_EQ(app_id1, preinstalled_prefs.LookUpAppIdByInstallUrl(
                         GURL("https://app2.com/install")));
}

IN_PROC_BROWSER_TEST_F(ExternallyInstalledWebAppPrefsBrowserTest,
                       UserUninstalledPreInstalledWebAppMigrationOldPref) {
  ExternallyInstalledWebAppPrefs external_prefs(profile()->GetPrefs());
  UserUninstalledPreinstalledWebAppPrefs preinstalled_prefs(
      profile()->GetPrefs());
  base::HistogramTester tester;

  AppId app_id1 = "test_app1";
  external_prefs.Insert(GURL("https://app1.com/install"), app_id1,
                        ExternalInstallSource::kExternalPolicy);
  external_prefs.SetIsPlaceholder(GURL("https://app1.com/install"), true);

  external_prefs.Insert(GURL("https://app2.com/install"), app_id1,
                        ExternalInstallSource::kArc);
  external_prefs.SetIsPlaceholder(GURL("https://app2.com/install"), true);
  tester.ExpectBucketCount(kPreinstalledAppMigrationHistogram,
                           UserUninstalledPreinstalledAppMigrationState::
                               kPreinstalledAppDataMigratedByOldPref,
                           0);

  UpdateBoolWebAppPref(profile()->GetPrefs(), app_id1,
                       kWasExternalAppUninstalledByUser, true);

  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      profile()->GetPrefs(), &provider().sync_bridge());
  tester.ExpectBucketCount(kPreinstalledAppMigrationHistogram,
                           UserUninstalledPreinstalledAppMigrationState::
                               kPreinstalledAppDataMigratedByOldPref,
                           1);

  // On migration, everything (app_ids and both URLs should be migrated).
  EXPECT_TRUE(preinstalled_prefs.DoesAppIdExist(app_id1));
  EXPECT_EQ(app_id1, preinstalled_prefs.LookUpAppIdByInstallUrl(
                         GURL("https://app1.com/install")));
  EXPECT_EQ(app_id1, preinstalled_prefs.LookUpAppIdByInstallUrl(
                         GURL("https://app2.com/install")));
}

IN_PROC_BROWSER_TEST_F(ExternallyInstalledWebAppPrefsBrowserTest,
                       DuplicateMigrationDoesNotGrowPreinstalledPrefs) {
  ExternallyInstalledWebAppPrefs external_prefs(profile()->GetPrefs());
  UserUninstalledPreinstalledWebAppPrefs preinstalled_prefs(
      profile()->GetPrefs());
  AppId app_id1 = "test_app1";
  external_prefs.Insert(GURL("https://app1.com/install"), app_id1,
                        ExternalInstallSource::kExternalPolicy);
  external_prefs.SetIsPlaceholder(GURL("https://app1.com/install"), true);

  UpdateBoolWebAppPref(profile()->GetPrefs(), app_id1,
                       kWasExternalAppUninstalledByUser, true);

  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      profile()->GetPrefs(), &provider().sync_bridge());

  // On migration, everything (app_ids and both URLs should be migrated).
  EXPECT_TRUE(preinstalled_prefs.DoesAppIdExist(app_id1));
  EXPECT_EQ(app_id1, preinstalled_prefs.LookUpAppIdByInstallUrl(
                         GURL("https://app1.com/install")));
  EXPECT_EQ(1, preinstalled_prefs.Size());

  // Call migration 2 more times, pref size should not grow if same
  // data is being migrated.
  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      profile()->GetPrefs(), &provider().sync_bridge());
  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      profile()->GetPrefs(), &provider().sync_bridge());
  EXPECT_EQ(1, preinstalled_prefs.Size());
}

// TODO(crbug.com/1339849)
// ExternallyInstalledWebAppPrefsBrowserTest.MigrationMetricsLoggedProperly
// is failing on Mac builders.
#if BUILDFLAG(IS_MAC)
#define Maybe_MigrationMetricsLoggedProperly \
  DISABLED_MigrationMetricsLoggedProperly
#else
#define Maybe_MigrationMetricsLoggedProperly MigrationMetricsLoggedProperly
#endif
IN_PROC_BROWSER_TEST_F(ExternallyInstalledWebAppPrefsBrowserTest,
                       Maybe_MigrationMetricsLoggedProperly) {
  PrefService* pref_service = profile()->GetPrefs();
  ExternallyInstalledWebAppPrefs external_prefs(pref_service);
  base::HistogramTester tester;
  const GURL url("https://app.com/install");

  // Initially, all buckets are empty.
  EXPECT_EQ(tester.GetTotalSum(kPrefDataAbsentDBDataAbsent), 0);
  EXPECT_EQ(tester.GetTotalSum(kPrefDataAbsentDBDataPresent), 0);
  EXPECT_EQ(tester.GetTotalSum(kPrefDataPresentDBDataAbsent), 0);
  EXPECT_EQ(tester.GetTotalSum(kPrefDataPresentDBDataPresent), 0);

  external_prefs.Insert(url, GenerateAppId(/*manifest_id=*/absl::nullopt, url),
                        ExternalInstallSource::kExternalDefault);
  external_prefs.SetIsPlaceholder(url, true);

  // Mock the startup by triggering a migration. Right now, only pref
  // data should be present.
  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      pref_service, &provider().sync_bridge());
  EXPECT_EQ(tester.GetTotalSum(kPrefDataAbsentDBDataAbsent), 0);
  EXPECT_EQ(tester.GetTotalSum(kPrefDataAbsentDBDataPresent), 0);
  EXPECT_EQ(tester.GetTotalSum(kPrefDataPresentDBDataAbsent), 1);
  EXPECT_EQ(tester.GetTotalSum(kPrefDataPresentDBDataPresent), 0);
  tester.ExpectBucketCount(kPlaceholderMigrationHistogram,
                           PlaceholderMigrationState::kPlaceholderInfoMigrated,
                           0);
  tester.ExpectBucketCount(kInstallURLMigrationHistogram,
                           InstallURLMigrationState::kInstallURLMigrated, 0);
  tester.ExpectBucketCount(kPreinstalledAppMigrationHistogram,
                           UserUninstalledPreinstalledAppMigrationState::
                               kPreinstalledAppDataMigratedByUser,
                           1);

  // Install a web_app and then retrigger migration. This should cause both
  // pref and DB data to be present.
  {
    auto web_app_install_info = std::make_unique<WebAppInstallInfo>();
    web_app_install_info->start_url = url;
    web_app_install_info->title = u"App Title";
    web_app_install_info->display_mode = DisplayMode::kBrowser;
    web_app_install_info->user_display_mode = UserDisplayMode::kStandalone;
    AppId app_id =
        test::InstallWebApp(profile(), std::move(web_app_install_info),
                            /*overwrite_existing_manifest_fields=*/true,
                            webapps::WebappInstallSource::EXTERNAL_DEFAULT);
  }

  // Retrigger the migration once web_app has been installed.
  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      pref_service, &provider().sync_bridge());
  EXPECT_EQ(tester.GetTotalSum(kPrefDataAbsentDBDataAbsent), 0);
  EXPECT_EQ(tester.GetTotalSum(kPrefDataAbsentDBDataPresent), 0);
  EXPECT_EQ(tester.GetTotalSum(kPrefDataPresentDBDataAbsent), 1);
  EXPECT_EQ(tester.GetTotalSum(kPrefDataPresentDBDataPresent), 1);
  tester.ExpectBucketCount(kPlaceholderMigrationHistogram,
                           PlaceholderMigrationState::kPlaceholderInfoMigrated,
                           1);
  tester.ExpectBucketCount(kInstallURLMigrationHistogram,
                           InstallURLMigrationState::kInstallURLMigrated, 1);
  tester.ExpectBucketCount(kPreinstalledAppMigrationHistogram,
                           UserUninstalledPreinstalledAppMigrationState::
                               kPreinstalledAppDataMigratedByUser,
                           1);
}

IN_PROC_BROWSER_TEST_F(ExternallyInstalledWebAppPrefsBrowserTest,
                       MetricsInSyncWebAppDB) {
  PrefService* pref_service = profile()->GetPrefs();
  ExternallyInstalledWebAppPrefs external_prefs(pref_service);
  base::HistogramTester tester;
  const GURL url("https://app.com/install");

  external_prefs.Insert(url, GenerateAppId(/*manifest_id=*/absl::nullopt, url),
                        ExternalInstallSource::kExternalPolicy);
  external_prefs.SetIsPlaceholder(url, true);

  // Install a web_app and then trigger migration. This would cause the
  // sync histograms to be filled as data already exists.
  AppId app_id;
  {
    auto web_app_install_info = std::make_unique<WebAppInstallInfo>();
    web_app_install_info->start_url = url;
    web_app_install_info->title = u"App Title";
    web_app_install_info->display_mode = DisplayMode::kBrowser;
    web_app_install_info->user_display_mode = UserDisplayMode::kStandalone;
    web_app_install_info->install_url = url;
    app_id = test::InstallWebApp(profile(), std::move(web_app_install_info),
                                 /*overwrite_existing_manifest_fields=*/true,
                                 webapps::WebappInstallSource::EXTERNAL_POLICY);
  }
  tester.ExpectBucketCount(
      kPlaceholderMigrationHistogram,
      PlaceholderMigrationState::kPlaceholderInfoAlreadyInSync, 0);
  tester.ExpectBucketCount(kInstallURLMigrationHistogram,
                           InstallURLMigrationState::kInstallURLAlreadyInSync,
                           0);
  {
    ScopedRegistryUpdate update(&provider().sync_bridge());
    WebApp* installed_app = update->UpdateApp(app_id);
    if (installed_app) {
      installed_app->AddPlaceholderInfoToManagementExternalConfigMap(
          WebAppManagement::kPolicy, /*is_placeholder=*/true);
    }
  }
  // Retrigger the migration once web_app has been installed.
  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      pref_service, &provider().sync_bridge());
  tester.ExpectBucketCount(
      kPlaceholderMigrationHistogram,
      PlaceholderMigrationState::kPlaceholderInfoAlreadyInSync, 1);
  tester.ExpectBucketCount(kInstallURLMigrationHistogram,
                           InstallURLMigrationState::kInstallURLAlreadyInSync,
                           1);
}

IN_PROC_BROWSER_TEST_F(ExternallyInstalledWebAppPrefsBrowserTest,
                       MetricsInSyncUserUninstalledPrefs) {
  PrefService* pref_service = profile()->GetPrefs();
  ExternallyInstalledWebAppPrefs external_prefs(pref_service);
  UserUninstalledPreinstalledWebAppPrefs preinstalled_prefs(pref_service);
  base::HistogramTester tester;

  external_prefs.Insert(GURL("https://a.com/"), "app_id",
                        ExternalInstallSource::kExternalDefault);
  external_prefs.Insert(GURL("https://b.com/"), "app_id",
                        ExternalInstallSource::kInternalDefault);
  tester.ExpectBucketCount(kPreinstalledAppMigrationHistogram,
                           UserUninstalledPreinstalledAppMigrationState::
                               kPreinstalledAppDataAlreadyInSync,
                           0);

  // Now the data in the preinstalled_prefs and external_prefs are the safe,
  // triggering migration should log a kPreinstalledAppDataAlreadyInSync.
  preinstalled_prefs.Add("app_id",
                         {GURL("https://a.com/"), GURL("https://b.com/")});

  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      pref_service, &provider().sync_bridge());
  tester.ExpectBucketCount(kPreinstalledAppMigrationHistogram,
                           UserUninstalledPreinstalledAppMigrationState::
                               kPreinstalledAppDataAlreadyInSync,
                           1);
}

IN_PROC_BROWSER_TEST_F(ExternallyInstalledWebAppPrefsBrowserTest,
                       FixBadPrefsByRemovingInstallUrl) {
  PrefService* pref_service = profile()->GetPrefs();
  ExternallyInstalledWebAppPrefs external_prefs(pref_service);
  UserUninstalledPreinstalledWebAppPrefs preinstalled_prefs(pref_service);
  base::HistogramTester tester;

  // Install the web app
  auto web_app = test::CreateWebApp(GURL("https://example.com/path"),
                                    WebAppManagement::kDefault);
  auto id = web_app->app_id();
  GURL install_url = GURL("https://a.com/");
  SimulateInstallApp(std::move(web_app));
  test::AddInstallUrlData(profile()->GetPrefs(), &provider().sync_bridge(), id,
                          install_url, ExternalInstallSource::kExternalDefault);

  // Add data to both the old prefs and the new prefs
  external_prefs.Insert(install_url, id,
                        ExternalInstallSource::kExternalDefault);
  preinstalled_prefs.Add(id, {install_url});

  // When migrating, the code should detect that the app is still installed, and
  // remove the new pref.
  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      pref_service, &provider().sync_bridge());

  EXPECT_THAT(tester.GetAllSamples(kPreinstalledAppMigrationHistogram),
              testing::IsEmpty());

  EXPECT_THAT(tester.GetAllSamples(
                  "WebApp.ExternalPrefs.CorruptionFixedInstallUrlsDeleted"),
              BucketsAre(base::Bucket(/*min=*/1, /*count=*/1)));
  EXPECT_THAT(
      tester.GetAllSamples("WebApp.ExternalPrefs.CorruptionFixedRemovedAppId"),
      BucketsAre(base::Bucket(/*min=*/0, /*count=*/1)));

  EXPECT_FALSE(preinstalled_prefs.DoesAppIdExist(id));
}

IN_PROC_BROWSER_TEST_F(ExternallyInstalledWebAppPrefsBrowserTest,
                       FixBadPrefsByRemovingAppId) {
  PrefService* pref_service = profile()->GetPrefs();
  ExternallyInstalledWebAppPrefs external_prefs(pref_service);
  UserUninstalledPreinstalledWebAppPrefs preinstalled_prefs(pref_service);
  base::HistogramTester tester;

  // Install the web app
  auto web_app = test::CreateWebApp(GURL("https://example.com/path"),
                                    WebAppManagement::kDefault);
  auto id = web_app->app_id();
  GURL install_url = GURL("https://a.com/");
  SimulateInstallApp(std::move(web_app));
  // Do NOT add the external data, similating upgrading an old client.

  // Add data to both the old prefs and the new prefs
  external_prefs.Insert(install_url, id,
                        ExternalInstallSource::kExternalDefault);
  preinstalled_prefs.Add(id, {install_url});

  // When migrating, the code should detect that the app is still installed, and
  // remove the new pref.
  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      pref_service, &provider().sync_bridge());

  EXPECT_THAT(tester.GetAllSamples(kPreinstalledAppMigrationHistogram),
              testing::IsEmpty());

  EXPECT_THAT(tester.GetAllSamples(
                  "WebApp.ExternalPrefs.CorruptionFixedInstallUrlsDeleted"),
              BucketsAre(base::Bucket(/*min=*/0, /*count=*/1)));
  EXPECT_THAT(
      tester.GetAllSamples("WebApp.ExternalPrefs.CorruptionFixedRemovedAppId"),
      BucketsAre(base::Bucket(/*min=*/1, /*count=*/1)));

  EXPECT_FALSE(preinstalled_prefs.DoesAppIdExist(id));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExternallyInstalledWebAppPrefsBrowserTest_ExternalPrefMigration,
    ::testing::Values(
        test::ExternalPrefMigrationTestCases::kDisableMigrationReadPref,
        test::ExternalPrefMigrationTestCases::kDisableMigrationReadDB,
        test::ExternalPrefMigrationTestCases::kEnableMigrationReadPref,
        test::ExternalPrefMigrationTestCases::kEnableMigrationReadDB),
    test::GetExternalPrefMigrationTestName);

}  // namespace web_app

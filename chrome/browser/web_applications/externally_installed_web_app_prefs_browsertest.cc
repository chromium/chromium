// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

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

  AppId SimulatePreviouslyInstalledApp(const GURL& url,
                                       ExternalInstallSource install_source) {
    AppId id = test::InstallDummyWebApp(profile(), "TestApp", url);
    ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
        .Insert(url, id, install_source);
    return id;
  }

  std::vector<GURL> GetAppUrls(ExternalInstallSource install_source) {
    std::vector<GURL> urls;
    for (const auto& id_and_url :
         ExternallyInstalledWebAppPrefs::BuildAppIdsMap(profile()->GetPrefs(),
                                                        install_source)) {
      urls.push_back(id_and_url.second);
    }
    std::sort(urls.begin(), urls.end());
    return urls;
  }
};

class ExternallyInstalledWebAppPrefsBrowserTest_ExternalPrefMigration
    : public ExternallyInstalledWebAppPrefsBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ExternallyInstalledWebAppPrefsBrowserTest_ExternalPrefMigration() {
    bool enable_migration = GetParam();
    if (enable_migration) {
      scoped_feature_list_.InitWithFeatures(
          {features::kUseWebAppDBInsteadOfExternalPrefs}, {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {features::kUseWebAppDBInsteadOfExternalPrefs});
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExternallyInstalledWebAppPrefsBrowserTest, BasicOps) {
  GURL url_a("https://a.example.com/");
  GURL url_b("https://b.example.com/");
  GURL url_c("https://c.example.com/");
  GURL url_d("https://d.example.com/");

  AppId id_a;
  AppId id_b;
  AppId id_c;
  AppId id_d;

  auto* prefs = profile()->GetPrefs();
  ExternallyInstalledWebAppPrefs map(prefs);

  // Start with an empty map.

  EXPECT_EQ("missing", map.LookupAppId(url_a).value_or("missing"));
  EXPECT_EQ("missing", map.LookupAppId(url_b).value_or("missing"));
  EXPECT_EQ("missing", map.LookupAppId(url_c).value_or("missing"));
  EXPECT_EQ("missing", map.LookupAppId(url_d).value_or("missing"));

  EXPECT_FALSE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_a));
  EXPECT_FALSE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_b));
  EXPECT_FALSE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_c));
  EXPECT_FALSE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_d));

  EXPECT_EQ(std::vector<GURL>({}),
            GetAppUrls(ExternalInstallSource::kInternalDefault));
  EXPECT_EQ(std::vector<GURL>({}),
            GetAppUrls(ExternalInstallSource::kExternalDefault));
  EXPECT_EQ(std::vector<GURL>({}),
            GetAppUrls(ExternalInstallSource::kExternalPolicy));

  // Add some entries.
  id_a = SimulatePreviouslyInstalledApp(
      url_a, ExternalInstallSource::kExternalDefault);
  id_b = SimulatePreviouslyInstalledApp(
      url_b, ExternalInstallSource::kInternalDefault);
  id_c = SimulatePreviouslyInstalledApp(
      url_c, ExternalInstallSource::kExternalDefault);

  EXPECT_EQ(id_a, map.LookupAppId(url_a).value_or("missing"));
  EXPECT_EQ(id_b, map.LookupAppId(url_b).value_or("missing"));
  EXPECT_EQ(id_c, map.LookupAppId(url_c).value_or("missing"));
  EXPECT_EQ("missing", map.LookupAppId(url_d).value_or("missing"));

  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_a));
  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_b));
  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_c));
  EXPECT_FALSE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_d));

  EXPECT_EQ(std::vector<GURL>({url_b}),
            GetAppUrls(ExternalInstallSource::kInternalDefault));
  EXPECT_EQ(std::vector<GURL>({url_a, url_c}),
            GetAppUrls(ExternalInstallSource::kExternalDefault));
  EXPECT_EQ(std::vector<GURL>({}),
            GetAppUrls(ExternalInstallSource::kExternalPolicy));

  // Overwrite an entry.

  SimulatePreviouslyInstalledApp(url_c,
                                 ExternalInstallSource::kInternalDefault);

  EXPECT_EQ(id_a, map.LookupAppId(url_a).value_or("missing"));
  EXPECT_EQ(id_b, map.LookupAppId(url_b).value_or("missing"));
  EXPECT_EQ(id_c, map.LookupAppId(url_c).value_or("missing"));
  EXPECT_EQ("missing", map.LookupAppId(url_d).value_or("missing"));

  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_a));
  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_b));
  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_c));
  EXPECT_FALSE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_d));

  EXPECT_EQ(std::vector<GURL>({url_b, url_c}),
            GetAppUrls(ExternalInstallSource::kInternalDefault));
  EXPECT_EQ(std::vector<GURL>({url_a}),
            GetAppUrls(ExternalInstallSource::kExternalDefault));
  EXPECT_EQ(std::vector<GURL>({}),
            GetAppUrls(ExternalInstallSource::kExternalPolicy));

  // Uninstall an underlying extension. The ExternallyInstalledWebAppPrefs will
  // still return positive.
  ScopedRegistryUpdate update(&provider().sync_bridge());
  update->DeleteApp(id_b);

  EXPECT_EQ(id_a, map.LookupAppId(url_a).value_or("missing"));
  EXPECT_EQ(id_b, map.LookupAppId(url_b).value_or("missing"));
  EXPECT_EQ(id_c, map.LookupAppId(url_c).value_or("missing"));
  EXPECT_EQ("missing", map.LookupAppId(url_d).value_or("missing"));

  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_a));
  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_b));
  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_c));
  EXPECT_FALSE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_d));

  EXPECT_EQ(std::vector<GURL>({url_b, url_c}),
            GetAppUrls(ExternalInstallSource::kInternalDefault));
  EXPECT_EQ(std::vector<GURL>({url_a}),
            GetAppUrls(ExternalInstallSource::kExternalDefault));
  EXPECT_EQ(std::vector<GURL>({}),
            GetAppUrls(ExternalInstallSource::kExternalPolicy));
}

IN_PROC_BROWSER_TEST_P(
    ExternallyInstalledWebAppPrefsBrowserTest_ExternalPrefMigration,
    IsPlaceholderApp) {
  const GURL url("https://example.com");
  auto web_app_install_info = std::make_unique<WebAppInstallInfo>();
  web_app_install_info->start_url = url;
  web_app_install_info->title = u"App Title";
  web_app_install_info->display_mode = web_app::DisplayMode::kBrowser;
  web_app_install_info->user_display_mode =
      web_app::UserDisplayMode::kStandalone;

  AppId app_id =
      test::InstallWebApp(profile(), std::move(web_app_install_info),
                          /*overwrite_existing_manifest_fields=*/true,
                          webapps::WebappInstallSource::EXTERNAL_POLICY);
  ExternallyInstalledWebAppPrefs prefs(profile()->GetPrefs());
  prefs.Insert(url, app_id, ExternalInstallSource::kExternalPolicy);
  EXPECT_FALSE(provider().registrar().IsPlaceholderApp(
      app_id, WebAppManagement::kPolicy));
  prefs.SetIsPlaceholder(url, true);
  {
    ScopedRegistryUpdate update(&provider().sync_bridge());
    WebApp* installed_app = update->UpdateApp(app_id);
    if (installed_app)
      installed_app->AddPlaceholderInfoToManagementExternalConfigMap(
          WebAppManagement::kPolicy, /*is_placeholder=*/true);
  }
  EXPECT_TRUE(provider().registrar().IsPlaceholderApp(
      app_id, WebAppManagement::kPolicy));
}

IN_PROC_BROWSER_TEST_P(
    ExternallyInstalledWebAppPrefsBrowserTest_ExternalPrefMigration,
    OldPrefFormat) {
  // Set up the old format for this pref {url -> app_id}.
  DictionaryPrefUpdate update(profile()->GetPrefs(),
                              prefs::kWebAppsExtensionIDs);
  update->SetStringKey("https://example.com", "add_id_string");
  // This should not crash on invalid pref data.
  EXPECT_FALSE(provider().registrar().IsPlaceholderApp(
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
  web_app_install_info->display_mode = web_app::DisplayMode::kBrowser;
  web_app_install_info->user_display_mode =
      web_app::UserDisplayMode::kStandalone;

  AppId app_id =
      test::InstallWebApp(profile(), std::move(web_app_install_info),
                          /*overwrite_existing_manifest_fields=*/true,
                          webapps::WebappInstallSource::EXTERNAL_POLICY);

  const WebApp* installed_app = provider().registrar().GetAppById(app_id);
  EXPECT_FALSE(provider().registrar().IsPlaceholderApp(
      app_id, WebAppManagement::kPolicy));

  GURL install_url("https://app.com/install");
  ExternallyInstalledWebAppPrefs external_prefs(profile()->GetPrefs());
  external_prefs.Insert(install_url, app_id,
                        ExternalInstallSource::kExternalPolicy);
  external_prefs.SetIsPlaceholder(install_url, true);

  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      profile()->GetPrefs(), &provider().sync_bridge());

  // Verify install source, placeholder info and urls have been migrated.
  EXPECT_TRUE(provider().registrar().IsPlaceholderApp(
      app_id, WebAppManagement::kPolicy));
  EXPECT_EQ(1u, installed_app->management_to_external_config_map().size());
  EXPECT_TRUE(base::Contains(
      installed_app
          ->management_to_external_config_map()[WebAppManagement::kPolicy]
          .install_urls,
      install_url));
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
  AppId app_id1 = "test_app1";
  external_prefs.Insert(GURL("https://app1.com/install"), app_id1,
                        ExternalInstallSource::kExternalPolicy);
  external_prefs.SetIsPlaceholder(GURL("https://app1.com/install"), true);

  external_prefs.Insert(GURL("https://app2.com/install"), app_id1,
                        ExternalInstallSource::kArc);
  external_prefs.SetIsPlaceholder(GURL("https://app2.com/install"), true);

  UpdateBoolWebAppPref(profile()->GetPrefs(), app_id1,
                       kWasExternalAppUninstalledByUser, true);

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

INSTANTIATE_TEST_SUITE_P(
    All,
    ExternallyInstalledWebAppPrefsBrowserTest_ExternalPrefMigration,
    ::testing::Bool());

}  // namespace web_app

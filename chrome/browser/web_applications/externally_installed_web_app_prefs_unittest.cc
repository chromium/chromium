// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"

#include <algorithm>

#include "base/json/json_reader.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class ExternallyInstalledWebAppPrefsTest : public WebAppTest {
 public:
  ExternallyInstalledWebAppPrefsTest() = default;
  ExternallyInstalledWebAppPrefsTest(
      const ExternallyInstalledWebAppPrefsTest&) = delete;
  ExternallyInstalledWebAppPrefsTest& operator=(
      const ExternallyInstalledWebAppPrefsTest&) = delete;
  ~ExternallyInstalledWebAppPrefsTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_app_provider_ = web_app::FakeWebAppProvider::Get(profile());
    web_app_provider_->StartWithSubsystems();
  }

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

  FakeWebAppProvider* provider() {
    base::RunLoop run_loop;
    web_app_provider_->on_registry_ready().Post(FROM_HERE,
                                                run_loop.QuitClosure());
    run_loop.Run();
    return web_app_provider_;
  }

 private:
  FakeWebAppProvider* web_app_provider_;
};

TEST_F(ExternallyInstalledWebAppPrefsTest, BasicOps) {
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
  ScopedRegistryUpdate update(&provider()->sync_bridge());
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

TEST_F(ExternallyInstalledWebAppPrefsTest, IsPlaceholderApp) {
  const GURL url("https://example.com");
  const AppId app_id = "app_id_string";
  ExternallyInstalledWebAppPrefs prefs(profile()->GetPrefs());
  prefs.Insert(url, app_id, ExternalInstallSource::kExternalPolicy);
  EXPECT_FALSE(ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                   .IsPlaceholderApp(app_id));
  prefs.SetIsPlaceholder(url, true);
  EXPECT_TRUE(ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                  .IsPlaceholderApp(app_id));
}

TEST_F(ExternallyInstalledWebAppPrefsTest, OldPrefFormat) {
  // Set up the old format for this pref {url -> app_id}.
  DictionaryPrefUpdate update(profile()->GetPrefs(),
                              prefs::kWebAppsExtensionIDs);
  update->SetStringKey("https://example.com", "add_id_string");
  // This should not crash on invalid pref data.
  EXPECT_FALSE(ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                   .IsPlaceholderApp("app_id_string"));
}

// Case 1: Single Install URL per source, no placeholder info (defaults to
// false).
TEST_F(ExternallyInstalledWebAppPrefsTest,
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
  expected_map["test_app1"][WebAppManagement::Type::kPolicy] =
      std::move(config1);
  WebApp::ExternalManagementConfig config2;
  config2.is_placeholder = false;
  config2.install_urls = {GURL("https://app2.com/")};
  expected_map["test_app1"][WebAppManagement::Type::kSystem] =
      std::move(config2);

  EXPECT_EQ(web_app_data_map, expected_map);
}

// Case 2: Multiple Install URL per source, with is_placeholder set to true.
TEST_F(ExternallyInstalledWebAppPrefsTest,
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
  expected_map["test_app1"][WebAppManagement::Type::kDefault] =
      std::move(config);

  EXPECT_EQ(web_app_data_map, expected_map);
}

// Edge cases:
// 1. pref entry with no data.
// 2. pref entry with all data.
// 3. pref entry with no source but existing app_id (won't generate map
// without knowing which source to correspond with).
// 4. pref entry with source but no app_id (won't generate map because app
// ID does not exist).
TEST_F(ExternallyInstalledWebAppPrefsTest,
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
  expected_map["test_app1"][WebAppManagement::Type::kDefault] =
      std::move(config1);
  expected_map["test_app4"][WebAppManagement::Type::kWebAppStore] =
      std::move(config2);

  EXPECT_EQ(web_app_data_map, expected_map);
}

TEST_F(ExternallyInstalledWebAppPrefsTest, MigrationTestForSingleSource) {
  std::unique_ptr<WebApp> web_app = test::CreateWebApp(
      GURL("https://app.com/"), WebAppManagement::Type::kDefault);
  AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->CreateApp(std::move(web_app));
  }
  const WebApp* installed_app = provider()->registrar().GetAppById(app_id);
  EXPECT_FALSE(provider()->registrar().IsPlaceholderApp(app_id));
  EXPECT_EQ(0u, installed_app->management_to_external_config_map().size());

  ExternallyInstalledWebAppPrefs external_prefs(profile()->GetPrefs());
  external_prefs.Insert(GURL("https://app.com/install"), app_id,
                        ExternalInstallSource::kExternalDefault);
  external_prefs.SetIsPlaceholder(GURL("https://app.com/install"), true);

  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      profile()->GetPrefs(), &provider()->sync_bridge());

  EXPECT_TRUE(provider()->registrar().IsPlaceholderApp(app_id));
  EXPECT_EQ(1u, installed_app->management_to_external_config_map().size());
  EXPECT_TRUE(installed_app->management_to_external_config_map()
                  .at(WebAppManagement::Type::kDefault)
                  .is_placeholder);
  // Verify install source and urls have been migrated.
  EXPECT_EQ(1u, installed_app->management_to_external_config_map()
                    .at(WebAppManagement::Type::kDefault)
                    .install_urls.size());
  EXPECT_EQ(GURL("https://app.com/install"),
            *installed_app->management_to_external_config_map()
                 .at(WebAppManagement::Type::kDefault)
                 .install_urls.begin());
}

TEST_F(ExternallyInstalledWebAppPrefsTest,
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
      profile()->GetPrefs(), &provider()->sync_bridge());

  // On migration, only a app_id1 should be migrated.
  EXPECT_TRUE(preinstalled_prefs.DoesAppIdExist(app_id1));
  EXPECT_FALSE(preinstalled_prefs.DoesAppIdExist(app_id2));
}

TEST_F(ExternallyInstalledWebAppPrefsTest,
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
      profile()->GetPrefs(), &provider()->sync_bridge());

  // On migration, nothing is migrated because default installs do not exist in
  // the external prefs.
  EXPECT_FALSE(preinstalled_prefs.DoesAppIdExist(app_id1));
}

TEST_F(ExternallyInstalledWebAppPrefsTest,
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
      profile()->GetPrefs(), &provider()->sync_bridge());

  // On migration, everything (app_ids and both URLs should be migrated).
  EXPECT_TRUE(preinstalled_prefs.DoesAppIdExist(app_id1));
  EXPECT_EQ(app_id1, preinstalled_prefs.LookUpAppIdByInstallUrl(
                         GURL("https://app1.com/install")));
  EXPECT_EQ(app_id1, preinstalled_prefs.LookUpAppIdByInstallUrl(
                         GURL("https://app2.com/install")));
}

TEST_F(ExternallyInstalledWebAppPrefsTest,
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
      profile()->GetPrefs(), &provider()->sync_bridge());

  // On migration, everything (app_ids and both URLs should be migrated).
  EXPECT_TRUE(preinstalled_prefs.DoesAppIdExist(app_id1));
  EXPECT_EQ(app_id1, preinstalled_prefs.LookUpAppIdByInstallUrl(
                         GURL("https://app1.com/install")));
  EXPECT_EQ(app_id1, preinstalled_prefs.LookUpAppIdByInstallUrl(
                         GURL("https://app2.com/install")));
}

TEST_F(ExternallyInstalledWebAppPrefsTest,
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
      profile()->GetPrefs(), &provider()->sync_bridge());

  // On migration, everything (app_ids and both URLs should be migrated).
  EXPECT_TRUE(preinstalled_prefs.DoesAppIdExist(app_id1));
  EXPECT_EQ(app_id1, preinstalled_prefs.LookUpAppIdByInstallUrl(
                         GURL("https://app1.com/install")));
  EXPECT_EQ(1, preinstalled_prefs.Size());

  // Call migration 2 more times, pref size should not grow if same
  // data is being migrated.
  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      profile()->GetPrefs(), &provider()->sync_bridge());
  ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
      profile()->GetPrefs(), &provider()->sync_bridge());
  EXPECT_EQ(1, preinstalled_prefs.Size());
}

}  // namespace web_app

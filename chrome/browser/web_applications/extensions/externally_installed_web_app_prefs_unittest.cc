// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

using ExternalInstallSource = ExternalInstallSource;

class ExternallyInstalledWebAppPrefsTest
    : public ChromeRenderViewHostTestHarness {
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
    web_app_provider_->SkipAwaitingExtensionSystem();
    web_app_provider_->StartWithSubsystems();
    // TODO(https://crbug.com/891172): Use an extension agnostic test registry.
    extensions::TestExtensionSystem* test_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));
    test_system->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                        profile()->GetPath(),
                                        false);  // autoupdate_enabled
  }

  std::string GenerateFakeExtensionId(const GURL& url) {
    return crx_file::id_util::GenerateId("fake_app_id_for:" + url.spec());
  }

  void SimulatePreviouslyInstalledApp(const GURL& url,
                                      ExternalInstallSource install_source) {
    std::string id = GenerateFakeExtensionId(url);
    extensions::ExtensionRegistry::Get(profile())->AddEnabled(
        extensions::ExtensionBuilder("Dummy Name").SetID(id).Build());

    ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
        .Insert(url, id, install_source);
  }

  void SimulateUninstallApp(const GURL& url) {
    std::string id = GenerateFakeExtensionId(url);
    extensions::ExtensionRegistry::Get(profile())->RemoveEnabled(id);
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

  void InitProvider() {
    base::RunLoop run_loop;
    web_app_provider_->on_registry_ready().Post(FROM_HERE,
                                                run_loop.QuitClosure());
    run_loop.Run();
  }

  FakeWebAppProvider* provider() { return web_app_provider_; }

 private:
  FakeWebAppProvider* web_app_provider_;
};

TEST_F(ExternallyInstalledWebAppPrefsTest, BasicOps) {
  GURL url_a("https://a.example.com/");
  GURL url_b("https://b.example.com/");
  GURL url_c("https://c.example.com/");
  GURL url_d("https://d.example.com/");

  std::string id_a = GenerateFakeExtensionId(url_a);
  std::string id_b = GenerateFakeExtensionId(url_b);
  std::string id_c = GenerateFakeExtensionId(url_c);
  std::string id_d = GenerateFakeExtensionId(url_d);

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

  SimulatePreviouslyInstalledApp(url_a,
                                 ExternalInstallSource::kExternalDefault);
  SimulatePreviouslyInstalledApp(url_b,
                                 ExternalInstallSource::kInternalDefault);
  SimulatePreviouslyInstalledApp(url_c,
                                 ExternalInstallSource::kExternalDefault);

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

  SimulateUninstallApp(url_b);

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

// Case 1: Single App ID, no placeholder info (defaults to
// false).
TEST_F(ExternallyInstalledWebAppPrefsTest, NoPlaceholderInfoDefaultsToFalse) {
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
  base::flat_map<AppId, ExternallyInstalledWebAppPrefs::ParsedPrefs>
      web_app_data_map =
          ExternallyInstalledWebAppPrefs::GetAppIdToWebAppParsedData(
              profile()->GetPrefs());
  EXPECT_EQ(1u, web_app_data_map.size());
  EXPECT_FALSE(web_app_data_map["test_app1"]
                   .placeholder_map[WebAppManagement::Type::kPolicy]);
  EXPECT_FALSE(web_app_data_map["test_app1"]
                   .placeholder_map[WebAppManagement::Type::kSystem]);
}

// Case 2: Multiple entries with single app ID, with is_placeholder set to true.
TEST_F(ExternallyInstalledWebAppPrefsTest,
       SinglePlaceholderInfoDefaultsToTrue) {
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

  base::flat_map<AppId, ExternallyInstalledWebAppPrefs::ParsedPrefs>
      web_app_data_map =
          ExternallyInstalledWebAppPrefs::GetAppIdToWebAppParsedData(
              profile()->GetPrefs());
  EXPECT_EQ(1u, web_app_data_map.size());
  EXPECT_TRUE(web_app_data_map["test_app1"]
                  .placeholder_map[WebAppManagement::Type::kDefault]);
}

TEST_F(ExternallyInstalledWebAppPrefsTest, MultiAppsMultiPlaceholderInfo) {
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
  base::flat_map<AppId, ExternallyInstalledWebAppPrefs::ParsedPrefs>
      web_app_data_map =
          ExternallyInstalledWebAppPrefs::GetAppIdToWebAppParsedData(
              profile()->GetPrefs());
  EXPECT_EQ(2u, web_app_data_map.size());
  EXPECT_FALSE(web_app_data_map["test_app1"]
                   .placeholder_map[WebAppManagement::Type::kDefault]);
  EXPECT_TRUE(web_app_data_map["test_app4"]
                  .placeholder_map[WebAppManagement::Type::kWebAppStore]);
}

TEST_F(ExternallyInstalledWebAppPrefsTest,
       PlaceholderMigrationTestForSingleSource) {
  InitProvider();
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
}

}  // namespace web_app

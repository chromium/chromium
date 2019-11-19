// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"

#include <algorithm>

#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
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
  ~ExternallyInstalledWebAppPrefsTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // TODO(https://crbug.com/891172): Use an extension agnostic test registry.
    extensions::TestExtensionSystem* test_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));
    test_system->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                        profile()->GetPath(),
                                        false);  // autoupdate_enabled
  }

  std::string GenerateFakeExtensionId(GURL url) {
    return crx_file::id_util::GenerateId("fake_app_id_for:" + url.spec());
  }

  void SimulatePreviouslyInstalledApp(GURL url,
                                      ExternalInstallSource install_source) {
    std::string id = GenerateFakeExtensionId(url);
    extensions::ExtensionRegistry::Get(profile())->AddEnabled(
        extensions::ExtensionBuilder("Dummy Name").SetID(id).Build());

    ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
        .Insert(url, id, install_source);
  }

  void SimulateUninstallApp(GURL url) {
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

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternallyInstalledWebAppPrefsTest);
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
  update->SetKey("https://example.com", base::Value("add_id_string"));
  // This should not crash on invalid pref data.
  EXPECT_FALSE(ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                   .IsPlaceholderApp("app_id_string"));
}

}  // namespace web_app

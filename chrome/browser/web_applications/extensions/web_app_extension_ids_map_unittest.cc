// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/web_app_extension_ids_map.h"

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

using InstallSource = InstallSource;

class WebAppExtensionIdsMapTest : public ChromeRenderViewHostTestHarness {
 public:
  WebAppExtensionIdsMapTest() = default;
  ~WebAppExtensionIdsMapTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
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
                                      InstallSource install_source,
                                      const char* format = nullptr) {
    std::string id = GenerateFakeExtensionId(url);
    extensions::ExtensionRegistry::Get(profile())->AddEnabled(
        extensions::ExtensionBuilder("Dummy Name").SetID(id).Build());

    if (format == nullptr) {
      // Write the latest format.
      ExtensionIdsMap extension_ids_map(profile()->GetPrefs());
      extension_ids_map.Insert(url, id, install_source);
    } else if (std::string(format) == "M70") {
      // Write the M70 format, with an implicit kInternal install source.
      EXPECT_EQ(InstallSource::kInternal, install_source);
      DictionaryPrefUpdate(profile()->GetPrefs(), prefs::kWebAppsExtensionIDs)
          ->SetKey(url.spec(), base::Value(id));
    } else {
      NOTREACHED();
    }
  }

  void SimulateUninstallApp(GURL url) {
    std::string id = GenerateFakeExtensionId(url);
    extensions::ExtensionRegistry::Get(profile())->RemoveEnabled(id);
  }

  std::vector<GURL> GetInstalledAppUrls(InstallSource install_source) {
    std::vector<GURL> vec =
        ExtensionIdsMap::GetInstalledAppUrls(profile(), install_source);
    std::sort(vec.begin(), vec.end());
    return vec;
  }

  bool HasM70FormatEntries() {
    const base::DictionaryValue* urls_to_dicts =
        profile()->GetPrefs()->GetDictionary(prefs::kWebAppsExtensionIDs);
    if (!urls_to_dicts) {
      return false;
    }
    for (const auto& it : urls_to_dicts->DictItems()) {
      if (it.second.is_string()) {
        return true;
      }
    }
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAppExtensionIdsMapTest);
};

TEST_F(WebAppExtensionIdsMapTest, BasicOps) {
  GURL url_a("https://a.example.com/");
  GURL url_b("https://b.example.com/");
  GURL url_c("https://c.example.com/");
  GURL url_d("https://d.example.com/");

  std::string id_a = GenerateFakeExtensionId(url_a);
  std::string id_b = GenerateFakeExtensionId(url_b);
  std::string id_c = GenerateFakeExtensionId(url_c);
  std::string id_d = GenerateFakeExtensionId(url_d);

  auto* prefs = profile()->GetPrefs();
  ExtensionIdsMap map(prefs);

  // Start with an empty map.

  EXPECT_EQ("missing", map.LookupExtensionId(url_a).value_or("missing"));
  EXPECT_EQ("missing", map.LookupExtensionId(url_b).value_or("missing"));
  EXPECT_EQ("missing", map.LookupExtensionId(url_c).value_or("missing"));
  EXPECT_EQ("missing", map.LookupExtensionId(url_d).value_or("missing"));

  EXPECT_FALSE(ExtensionIdsMap::HasExtensionId(prefs, id_a));
  EXPECT_FALSE(ExtensionIdsMap::HasExtensionId(prefs, id_b));
  EXPECT_FALSE(ExtensionIdsMap::HasExtensionId(prefs, id_c));
  EXPECT_FALSE(ExtensionIdsMap::HasExtensionId(prefs, id_d));

  EXPECT_EQ(std::vector<GURL>({}),
            GetInstalledAppUrls(InstallSource::kInternal));
  EXPECT_EQ(std::vector<GURL>({}),
            GetInstalledAppUrls(InstallSource::kExternalDefault));
  EXPECT_EQ(std::vector<GURL>({}),
            GetInstalledAppUrls(InstallSource::kExternalPolicy));

  // Add some entries.

  SimulatePreviouslyInstalledApp(url_a, InstallSource::kExternalDefault);
  SimulatePreviouslyInstalledApp(url_b, InstallSource::kInternal);
  SimulatePreviouslyInstalledApp(url_c, InstallSource::kExternalDefault);

  EXPECT_EQ(id_a, map.LookupExtensionId(url_a).value_or("missing"));
  EXPECT_EQ(id_b, map.LookupExtensionId(url_b).value_or("missing"));
  EXPECT_EQ(id_c, map.LookupExtensionId(url_c).value_or("missing"));
  EXPECT_EQ("missing", map.LookupExtensionId(url_d).value_or("missing"));

  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_a));
  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_b));
  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_c));
  EXPECT_FALSE(ExtensionIdsMap::HasExtensionId(prefs, id_d));

  EXPECT_EQ(std::vector<GURL>({url_b}),
            GetInstalledAppUrls(InstallSource::kInternal));
  EXPECT_EQ(std::vector<GURL>({url_a, url_c}),
            GetInstalledAppUrls(InstallSource::kExternalDefault));
  EXPECT_EQ(std::vector<GURL>({}),
            GetInstalledAppUrls(InstallSource::kExternalPolicy));

  // Overwrite an entry.

  SimulatePreviouslyInstalledApp(url_c, InstallSource::kInternal);

  EXPECT_EQ(id_a, map.LookupExtensionId(url_a).value_or("missing"));
  EXPECT_EQ(id_b, map.LookupExtensionId(url_b).value_or("missing"));
  EXPECT_EQ(id_c, map.LookupExtensionId(url_c).value_or("missing"));
  EXPECT_EQ("missing", map.LookupExtensionId(url_d).value_or("missing"));

  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_a));
  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_b));
  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_c));
  EXPECT_FALSE(ExtensionIdsMap::HasExtensionId(prefs, id_d));

  EXPECT_EQ(std::vector<GURL>({url_b, url_c}),
            GetInstalledAppUrls(InstallSource::kInternal));
  EXPECT_EQ(std::vector<GURL>({url_a}),
            GetInstalledAppUrls(InstallSource::kExternalDefault));
  EXPECT_EQ(std::vector<GURL>({}),
            GetInstalledAppUrls(InstallSource::kExternalPolicy));

  // Uninstall an underlying extension. The ExtensionIdsMap will still return
  // positive for LookupExtensionId and HasExtensionId (as they ignore
  // installed-ness), but GetInstalledAppUrls will skip over it.

  SimulateUninstallApp(url_b);

  EXPECT_EQ(id_a, map.LookupExtensionId(url_a).value_or("missing"));
  EXPECT_EQ(id_b, map.LookupExtensionId(url_b).value_or("missing"));
  EXPECT_EQ(id_c, map.LookupExtensionId(url_c).value_or("missing"));
  EXPECT_EQ("missing", map.LookupExtensionId(url_d).value_or("missing"));

  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_a));
  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_b));
  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_c));
  EXPECT_FALSE(ExtensionIdsMap::HasExtensionId(prefs, id_d));

  EXPECT_EQ(std::vector<GURL>({url_c}),
            GetInstalledAppUrls(InstallSource::kInternal));
  EXPECT_EQ(std::vector<GURL>({url_a}),
            GetInstalledAppUrls(InstallSource::kExternalDefault));
  EXPECT_EQ(std::vector<GURL>({}),
            GetInstalledAppUrls(InstallSource::kExternalPolicy));

  // Add an entry in the older M70 prefs format.

  EXPECT_FALSE(HasM70FormatEntries());
  SimulatePreviouslyInstalledApp(url_d, InstallSource::kInternal, "M70");
  EXPECT_TRUE(HasM70FormatEntries());

  EXPECT_EQ(id_a, map.LookupExtensionId(url_a).value_or("missing"));
  EXPECT_EQ(id_b, map.LookupExtensionId(url_b).value_or("missing"));
  EXPECT_EQ(id_c, map.LookupExtensionId(url_c).value_or("missing"));
  EXPECT_EQ(id_d, map.LookupExtensionId(url_d).value_or("missing"));

  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_a));
  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_b));
  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_c));
  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_d));

  EXPECT_EQ(std::vector<GURL>({url_c, url_d}),
            GetInstalledAppUrls(InstallSource::kInternal));
  EXPECT_EQ(std::vector<GURL>({url_a}),
            GetInstalledAppUrls(InstallSource::kExternalDefault));
  EXPECT_EQ(std::vector<GURL>({}),
            GetInstalledAppUrls(InstallSource::kExternalPolicy));

  // Upgrade from the older M70 prefs format to the newer M71+ one. Other than
  // HasM70FormatEntries, none of the other EXPECTs should change.

  EXPECT_TRUE(HasM70FormatEntries());
  ExtensionIdsMap::UpgradeFromM70Format(prefs);
  EXPECT_FALSE(HasM70FormatEntries());

  EXPECT_EQ(id_a, map.LookupExtensionId(url_a).value_or("missing"));
  EXPECT_EQ(id_b, map.LookupExtensionId(url_b).value_or("missing"));
  EXPECT_EQ(id_c, map.LookupExtensionId(url_c).value_or("missing"));
  EXPECT_EQ(id_d, map.LookupExtensionId(url_d).value_or("missing"));

  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_a));
  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_b));
  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_c));
  EXPECT_TRUE(ExtensionIdsMap::HasExtensionId(prefs, id_d));

  EXPECT_EQ(std::vector<GURL>({url_c, url_d}),
            GetInstalledAppUrls(InstallSource::kInternal));
  EXPECT_EQ(std::vector<GURL>({url_a}),
            GetInstalledAppUrls(InstallSource::kExternalDefault));
  EXPECT_EQ(std::vector<GURL>({}),
            GetInstalledAppUrls(InstallSource::kExternalPolicy));
}

}  // namespace web_app

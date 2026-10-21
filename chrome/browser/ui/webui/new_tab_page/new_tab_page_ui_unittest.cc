// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"

#include "base/functional/bind.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_pref_observer.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ntp_tiles/tile_type.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"

class NewTabPageUITest : public testing::Test {
 public:
  NewTabPageUITest() = default;

  TestingProfile& profile() { return profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(NewTabPageUITest,
       MigrateDeprecatedUseMostVisitedTilesPref_UseMostVisitedTilesTrue) {
  PrefService* prefs = profile().GetPrefs();
  prefs->SetBoolean(ntp_prefs::kNtpUseMostVisitedTiles, true);

  MostVisitedPrefObserver::MigrateDeprecatedUseMostVisitedTilesPref(prefs);

  EXPECT_EQ(static_cast<int>(ntp_tiles::TileType::kTopSites),
            prefs->GetInteger(ntp_prefs::kNtpShortcutsType));
  // Check that the old pref has been cleared.
  EXPECT_EQ(nullptr,
            prefs->GetUserPrefValue(ntp_prefs::kNtpUseMostVisitedTiles));
}

TEST_F(NewTabPageUITest,
       MigrateDeprecatedUseMostVisitedTilesPref_UseMostVisitedTilesFalse) {
  PrefService* prefs = profile().GetPrefs();
  prefs->SetBoolean(ntp_prefs::kNtpUseMostVisitedTiles, false);

  MostVisitedPrefObserver::MigrateDeprecatedUseMostVisitedTilesPref(prefs);

  EXPECT_EQ(static_cast<int>(ntp_tiles::TileType::kCustomLinks),
            prefs->GetInteger(ntp_prefs::kNtpShortcutsType));
  // Check that the old pref has been cleared.
  EXPECT_EQ(nullptr,
            prefs->GetUserPrefValue(ntp_prefs::kNtpUseMostVisitedTiles));
}

TEST_F(NewTabPageUITest,
       MigrateDeprecatedUseMostVisitedTilesPref_UseMostVisitedTilesNotSet) {
  PrefService* prefs = profile().GetPrefs();
  // Ensure the deprecated pref has its default value but not a user-set value.
  ASSERT_EQ(nullptr,
            prefs->GetUserPrefValue(ntp_prefs::kNtpUseMostVisitedTiles));

  // The function should not crash and should not modify the new pref.
  MostVisitedPrefObserver::MigrateDeprecatedUseMostVisitedTilesPref(prefs);

  // Check that the old and new pref are not set.
  EXPECT_EQ(nullptr, prefs->GetUserPrefValue(ntp_prefs::kNtpShortcutsType));
  EXPECT_EQ(nullptr,
            prefs->GetUserPrefValue(ntp_prefs::kNtpUseMostVisitedTiles));
}

TEST_F(
    NewTabPageUITest,
    MigrateDeprecatedUseMostVisitedTilesPref_UseMostVisitedTilesNotSet_ShortcutsTilesTypeSet) {
  PrefService* prefs = profile().GetPrefs();
  // Ensure the deprecated pref has its default value but not a user-set value.
  ASSERT_EQ(nullptr,
            prefs->GetUserPrefValue(ntp_prefs::kNtpUseMostVisitedTiles));

  // Set the value of the new pref.
  int initial_value = 1;
  prefs->SetInteger(ntp_prefs::kNtpShortcutsType, initial_value);

  // The function should not crash and should not modify the new pref.
  MostVisitedPrefObserver::MigrateDeprecatedUseMostVisitedTilesPref(prefs);

  // Check that the new pref is set properly and the old pref is not set.
  EXPECT_EQ(initial_value, prefs->GetInteger(ntp_prefs::kNtpShortcutsType));
  EXPECT_EQ(nullptr,
            prefs->GetUserPrefValue(ntp_prefs::kNtpUseMostVisitedTiles));
}

TEST_F(NewTabPageUITest, MigrateDeprecatedShortcutsTypePref_TopSites) {
  PrefService* prefs = profile().GetPrefs();
  prefs->SetInteger(ntp_prefs::kNtpShortcutsType,
                    static_cast<int>(ntp_tiles::TileType::kTopSites));

  MostVisitedPrefObserver::MigrateDeprecatedShortcutsTypePref(prefs);

  EXPECT_FALSE(prefs->GetBoolean(ntp_prefs::kNtpCustomLinksVisible));
  EXPECT_FALSE(prefs->GetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible));
  // Check that the old pref has been cleared.
  EXPECT_EQ(nullptr, prefs->GetUserPrefValue(ntp_prefs::kNtpShortcutsType));
}

TEST_F(NewTabPageUITest, MigrateDeprecatedShortcutsTypePref_CustomLinks) {
  PrefService* prefs = profile().GetPrefs();
  prefs->SetInteger(ntp_prefs::kNtpShortcutsType,
                    static_cast<int>(ntp_tiles::TileType::kCustomLinks));

  MostVisitedPrefObserver::MigrateDeprecatedShortcutsTypePref(prefs);

  EXPECT_TRUE(prefs->GetBoolean(ntp_prefs::kNtpCustomLinksVisible));
  EXPECT_FALSE(prefs->GetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible));
  // Check that the old pref has been cleared.
  EXPECT_EQ(nullptr, prefs->GetUserPrefValue(ntp_prefs::kNtpShortcutsType));
}

TEST_F(NewTabPageUITest, MigrateDeprecatedShortcutsTypePref_Enterprise) {
  PrefService* prefs = profile().GetPrefs();
  prefs->SetInteger(
      ntp_prefs::kNtpShortcutsType,
      static_cast<int>(ntp_tiles::TileType::kEnterpriseShortcuts));

  MostVisitedPrefObserver::MigrateDeprecatedShortcutsTypePref(prefs);

  EXPECT_FALSE(prefs->GetBoolean(ntp_prefs::kNtpCustomLinksVisible));
  EXPECT_TRUE(prefs->GetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible));
  // Check that the old pref has been cleared.
  EXPECT_EQ(nullptr, prefs->GetUserPrefValue(ntp_prefs::kNtpShortcutsType));
}

TEST_F(NewTabPageUITest, MigrateDeprecatedShortcutsTypePref_NotSet) {
  PrefService* prefs = profile().GetPrefs();
  // Ensure the deprecated pref has its default value but not a user-set value.
  ASSERT_EQ(nullptr, prefs->GetUserPrefValue(ntp_prefs::kNtpShortcutsType));

  // The function should not crash and should not modify the new pref.
  MostVisitedPrefObserver::MigrateDeprecatedShortcutsTypePref(prefs);

  // Check that the new prefs are not set by the migration.
  EXPECT_EQ(nullptr,
            prefs->GetUserPrefValue(ntp_prefs::kNtpCustomLinksVisible));
  EXPECT_EQ(nullptr,
            prefs->GetUserPrefValue(ntp_prefs::kNtpEnterpriseShortcutsVisible));
}

TEST_F(NewTabPageUITest,
       MigrateDeprecatedShortcutsTypePref_NotSet_NewPrefsSet) {
  PrefService* prefs = profile().GetPrefs();
  // Ensure the deprecated pref has its default value but not a user-set value.
  ASSERT_EQ(nullptr, prefs->GetUserPrefValue(ntp_prefs::kNtpShortcutsType));

  // Set the value of the new prefs.
  prefs->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, true);
  prefs->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible, false);

  // The function should not crash and should not modify the new pref.
  MostVisitedPrefObserver::MigrateDeprecatedShortcutsTypePref(prefs);

  // Check that the new pref is set properly and the old pref is not set.
  ASSERT_EQ(nullptr, prefs->GetUserPrefValue(ntp_prefs::kNtpShortcutsType));
  EXPECT_TRUE(prefs->GetBoolean(ntp_prefs::kNtpCustomLinksVisible));
  EXPECT_FALSE(prefs->GetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible));
}

TEST_F(NewTabPageUITest, ShowAllMostVisitedTilesPref_Default_False) {
  PrefService* prefs = profile().GetPrefs();
  // Ensure the pref has its default value as false;
  EXPECT_FALSE(prefs->GetBoolean(ntp_prefs::kNtpShowAllMostVisitedTiles));
}

TEST_F(NewTabPageUITest, ShowAllMostVisitedTilesPref_Set_True) {
  PrefService* prefs = profile().GetPrefs();
  bool init_value = prefs->GetBoolean(ntp_prefs::kNtpShowAllMostVisitedTiles);
  ASSERT_FALSE(init_value);

  // Set the pref to true
  prefs->SetBoolean(ntp_prefs::kNtpShowAllMostVisitedTiles, true);

  // Check that the pref is set to true
  EXPECT_TRUE(prefs->GetBoolean(ntp_prefs::kNtpShowAllMostVisitedTiles));
}

TEST_F(NewTabPageUITest, ShortcutsAutoRemovalDisabledPref_Set_True) {
  PrefService* prefs = profile().GetPrefs();
  // Ensure the pref has its default value as false.
  bool init_value =
      prefs->GetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled);
  ASSERT_FALSE(init_value);

  // Set the pref to true.
  prefs->SetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled, true);
  EXPECT_TRUE(prefs->GetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
}

TEST_F(NewTabPageUITest,
       GetLoadTimeData_ExpandedLhsUrlIncludesColorSchemeAndLocale) {
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider(
      variations::VariationsIdsProvider::Mode::kUseSignedInState);
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile(),
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  ScopedBrowserLocale scoped_locale("he");
  ui::MockOsSettingsProvider os_settings_provider;

  for (const auto& [color_scheme, expected_cs] :
       {std::pair{ui::NativeTheme::PreferredColorScheme::kLight, "0"},
        std::pair{ui::NativeTheme::PreferredColorScheme::kDark, "1"}}) {
    os_settings_provider.SetPreferredColorScheme(color_scheme);
    base::DictValue load_time_data = NewTabPageUI::GetLoadTimeData(
        &profile(), /*session_allows_drag_and_drop=*/false);

    const std::string* url_str = load_time_data.FindString("expandedLhsUrl");
    ASSERT_NE(nullptr, url_str);
    GURL url(*url_str);
    ASSERT_TRUE(url.is_valid());

    std::string cs_value;
    EXPECT_TRUE(net::GetValueForKeyInQuery(url, "cs", &cs_value));
    EXPECT_EQ(expected_cs, cs_value);

    std::string csui_value;
    EXPECT_TRUE(net::GetValueForKeyInQuery(url, "csui", &csui_value));
    EXPECT_EQ("3", csui_value);

    std::string hl_value;
    EXPECT_TRUE(net::GetValueForKeyInQuery(url, "hl", &hl_value));
    EXPECT_EQ("he", hl_value);
  }
}

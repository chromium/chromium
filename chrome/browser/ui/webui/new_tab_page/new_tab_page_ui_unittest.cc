// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"

#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ntp_tiles/tile_type.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  NewTabPageUI::MigrateDeprecatedUseMostVisitedTilesPref(prefs);

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

  NewTabPageUI::MigrateDeprecatedUseMostVisitedTilesPref(prefs);

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
  NewTabPageUI::MigrateDeprecatedUseMostVisitedTilesPref(prefs);

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
  NewTabPageUI::MigrateDeprecatedUseMostVisitedTilesPref(prefs);

  // Check that the new pref is set properly and the old pref is not set.
  EXPECT_EQ(initial_value, prefs->GetInteger(ntp_prefs::kNtpShortcutsType));
  EXPECT_EQ(nullptr,
            prefs->GetUserPrefValue(ntp_prefs::kNtpUseMostVisitedTiles));
}

TEST_F(NewTabPageUITest, MigrateDeprecatedShortcutsTypePref_TopSites) {
  PrefService* prefs = profile().GetPrefs();
  prefs->SetInteger(ntp_prefs::kNtpShortcutsType,
                    static_cast<int>(ntp_tiles::TileType::kTopSites));

  NewTabPageUI::MigrateDeprecatedShortcutsTypePref(prefs);

  EXPECT_FALSE(prefs->GetBoolean(ntp_prefs::kNtpCustomLinksVisible));
  EXPECT_FALSE(prefs->GetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible));
  // Check that the old pref has been cleared.
  EXPECT_EQ(nullptr, prefs->GetUserPrefValue(ntp_prefs::kNtpShortcutsType));
}

TEST_F(NewTabPageUITest, MigrateDeprecatedShortcutsTypePref_CustomLinks) {
  PrefService* prefs = profile().GetPrefs();
  prefs->SetInteger(ntp_prefs::kNtpShortcutsType,
                    static_cast<int>(ntp_tiles::TileType::kCustomLinks));

  NewTabPageUI::MigrateDeprecatedShortcutsTypePref(prefs);

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

  NewTabPageUI::MigrateDeprecatedShortcutsTypePref(prefs);

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
  NewTabPageUI::MigrateDeprecatedShortcutsTypePref(prefs);

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
  NewTabPageUI::MigrateDeprecatedShortcutsTypePref(prefs);

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

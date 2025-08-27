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

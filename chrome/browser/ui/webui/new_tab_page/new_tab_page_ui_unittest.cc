// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_pref_observer.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ntp_tiles/tile_type.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/lhs_rail_config.pb.h"
#include "url/gurl.h"

namespace {

std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
    content::BrowserContext* context) {
  auto* profile = Profile::FromBrowserContext(context);
  return std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
      *profile->GetPrefs(), TemplateURLServiceFactory::GetForProfile(profile),
      /*url_loader_factory=*/nullptr,
      /*identity_manager=*/nullptr);
}

}  // namespace

class NewTabPageUITest : public testing::Test {
 public:
  NewTabPageUITest() = default;

  TestingProfile& profile() { return profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
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

TEST_F(NewTabPageUITest, GetLoadTimeData_ExpandedLhsUrlParameters) {
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider(
      variations::VariationsIdsProvider::Mode::kUseSignedInState);
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile(),
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  ScopedBrowserLocale scoped_locale("he");

  base::DictValue load_time_data = NewTabPageUI::GetLoadTimeData(
      &profile(), /*session_allows_drag_and_drop=*/false);

  const std::string* url_str = load_time_data.FindString("expandedLhsUrl");
  ASSERT_NE(nullptr, url_str);
  GURL url(*url_str);
  ASSERT_TRUE(url.is_valid());

  std::string cs_value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(url, "cs", &cs_value));

  std::string csui_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "csui", &csui_value));
  EXPECT_EQ("3", csui_value);

  std::string hl_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "hl", &hl_value));
  EXPECT_EQ("he", hl_value);
}

TEST_F(NewTabPageUITest, GetLoadTimeData_LhsRailConfig) {
  base::test::ScopedFeatureList scoped_feature_list{
      ntp_features::kNtpThreadsRail};
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile(),
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  auto* mock_aim_service =
      static_cast<testing::NiceMock<MockAimEligibilityService>*>(
          AimEligibilityServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile(),
              base::BindRepeating(&BuildMockAimEligibilityService)));
  ON_CALL(*mock_aim_service, IsAimEligible())
      .WillByDefault(testing::Return(true));

  // 1. When GetLhsRailConfig() returns nullptr, lhsRailConfig is not set.
  ON_CALL(*mock_aim_service, GetLhsRailConfig())
      .WillByDefault(testing::Return(nullptr));
  base::DictValue load_time_data_empty = NewTabPageUI::GetLoadTimeData(
      &profile(), /*session_allows_drag_and_drop=*/false);
  EXPECT_EQ(load_time_data_empty.FindDict("lhsRailConfig"), nullptr);

  // 2. When GetLhsRailConfig() returns a populated config, lhsRailConfig is
  // serialized with items (including resolved targetUrl for valid relative
  // URLs, and omitting targetUrl when relative_url is empty or invalid).
  omnibox::LhsRailConfig lhs_config;

  auto* action_only_item = lhs_config.add_lhs_items();
  action_only_item->set_icon_svg("<svg id='0'></svg>");
  action_only_item->set_name("AI Mode history");
  action_only_item->mutable_click_action()->set_client_action(
      omnibox::LhsRailConfig::ClickAction::CLIENT_ACTION_EXPAND_RAIL);

  auto* valid_item = lhs_config.add_lhs_items();
  valid_item->set_icon_svg("<svg id='1'></svg>");
  valid_item->set_name("Tasks");
  valid_item->mutable_click_action()->set_client_action(
      omnibox::LhsRailConfig::ClickAction::CLIENT_ACTION_NAVIGATE);
  valid_item->mutable_click_action()->mutable_link()->set_relative_url(
      "/search?udm=50&atvm=3");

  auto* missing_slash_item = lhs_config.add_lhs_items();
  missing_slash_item->set_icon_svg("<svg id='2'></svg>");
  missing_slash_item->set_name("Missing Leading Slash");
  missing_slash_item->mutable_click_action()->set_client_action(
      omnibox::LhsRailConfig::ClickAction::CLIENT_ACTION_NAVIGATE);
  missing_slash_item->mutable_click_action()->mutable_link()->set_relative_url(
      "search?udm=50");

  auto* absolute_url_item = lhs_config.add_lhs_items();
  absolute_url_item->set_icon_svg("<svg id='3'></svg>");
  absolute_url_item->set_name("Invalid Absolute URL");
  absolute_url_item->mutable_click_action()->set_client_action(
      omnibox::LhsRailConfig::ClickAction::CLIENT_ACTION_NAVIGATE);
  absolute_url_item->mutable_click_action()->mutable_link()->set_relative_url(
      "https://evil.com/search");

  auto* scheme_relative_item = lhs_config.add_lhs_items();
  scheme_relative_item->set_icon_svg("<svg id='4'></svg>");
  scheme_relative_item->set_name("Invalid Scheme-Relative URL");
  scheme_relative_item->mutable_click_action()->set_client_action(
      omnibox::LhsRailConfig::ClickAction::CLIENT_ACTION_NAVIGATE);
  scheme_relative_item->mutable_click_action()
      ->mutable_link()
      ->set_relative_url("//evil.com/search");

  auto* backslash_item = lhs_config.add_lhs_items();
  backslash_item->set_icon_svg("<svg id='5'></svg>");
  backslash_item->set_name("Invalid Backslash URL");
  backslash_item->mutable_click_action()->set_client_action(
      omnibox::LhsRailConfig::ClickAction::CLIENT_ACTION_NAVIGATE);
  backslash_item->mutable_click_action()->mutable_link()->set_relative_url(
      "/\\evil.com/search");

  ON_CALL(*mock_aim_service, GetLhsRailConfig())
      .WillByDefault(testing::Return(&lhs_config));

  base::DictValue load_time_data = NewTabPageUI::GetLoadTimeData(
      &profile(), /*session_allows_drag_and_drop=*/false);
  const base::DictValue* lhs_rail_dict =
      load_time_data.FindDict("lhsRailConfig");
  if (!load_time_data.FindBool("enableThreadsRail").value_or(false)) {
    EXPECT_EQ(lhs_rail_dict, nullptr);
    return;
  }
  ASSERT_NE(lhs_rail_dict, nullptr);

  const base::ListValue* items = lhs_rail_dict->FindList("items");
  ASSERT_NE(items, nullptr);
  ASSERT_EQ(items->size(), 6u);

  const base::DictValue& item0 = (*items)[0].GetDict();
  EXPECT_EQ(*item0.FindString("iconSvg"), "<svg id='0'></svg>");
  EXPECT_EQ(*item0.FindString("name"), "AI Mode history");
  EXPECT_EQ(
      item0.FindInt("clientAction"),
      static_cast<int>(
          omnibox::LhsRailConfig::ClickAction::CLIENT_ACTION_EXPAND_RAIL));
  EXPECT_EQ(item0.FindString("targetUrl"), nullptr);

  const base::DictValue& item1 = (*items)[1].GetDict();
  EXPECT_EQ(*item1.FindString("iconSvg"), "<svg id='1'></svg>");
  EXPECT_EQ(*item1.FindString("name"), "Tasks");
  EXPECT_EQ(item1.FindInt("clientAction"),
            static_cast<int>(
                omnibox::LhsRailConfig::ClickAction::CLIENT_ACTION_NAVIGATE));
  ASSERT_NE(item1.FindString("targetUrl"), nullptr);
  EXPECT_EQ(*item1.FindString("targetUrl"),
            "https://www.google.com/search?udm=50&atvm=3");

  for (size_t i = 2; i < items->size(); ++i) {
    EXPECT_EQ((*items)[i].GetDict().FindString("targetUrl"), nullptr);
  }

  // 3. When the user is not AIM eligible, lhsRailConfig is not serialized.
  ON_CALL(*mock_aim_service, IsAimEligible())
      .WillByDefault(testing::Return(false));
  base::DictValue load_time_data_ineligible = NewTabPageUI::GetLoadTimeData(
      &profile(), /*session_allows_drag_and_drop=*/false);
  EXPECT_EQ(load_time_data_ineligible.FindDict("lhsRailConfig"), nullptr);
}

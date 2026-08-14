// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_pref_observer.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/ui/search/most_visited_metrics_logger.h"
#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/tile_type.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockMostVisitedHandler : public MostVisitedHandler {
 public:
  MockMostVisitedHandler(Profile* profile, content::WebContents* web_contents)
      : MostVisitedHandler(
            mojo::PendingReceiver<
                most_visited::mojom::MostVisitedPageHandler>(),
            mojo::PendingRemote<most_visited::mojom::MostVisitedPage>(),
            profile,
            web_contents,
            std::make_unique<MostVisitedMetricsLogger>("NewTabPage")) {}
  ~MockMostVisitedHandler() override = default;

  MOCK_METHOD(
      void,
      EnableTileTypes,
      (const ntp_tiles::MostVisitedSites::EnableTileTypesOptions& options),
      (override));
  MOCK_METHOD(void, SetShortcutsVisible, (bool visible), (override));
};

MATCHER_P(EnableTileTypesOptionsEq, expected, "") {
  return arg.enable_top_sites == expected.enable_top_sites &&
         arg.enable_custom_links == expected.enable_custom_links &&
         arg.enable_enterprise_shortcuts ==
             expected.enable_enterprise_shortcuts;
}

class MostVisitedPrefObserverTest : public testing::Test {
 public:
  MostVisitedPrefObserverTest() = default;

  void SetUp() override {
    SetUpGoogleDefaultSearchProvider();
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    mock_handler_ = std::make_unique<testing::NiceMock<MockMostVisitedHandler>>(
        &profile_, web_contents_.get());
  }

  void SetUpGoogleDefaultSearchProvider() {
    factory_util_.VerifyLoad();
    TemplateURLData data;
    data.SetURL("https://www.google.com/search?q={searchTerms}");
    data.suggestions_url =
        "https://www.google.com/complete/search?q={searchTerms}";
    TemplateURLService* template_url_service = factory_util_.model();
    template_url_service->SetUserSelectedDefaultSearchProvider(
        template_url_service->Add(std::make_unique<TemplateURL>(data)));
  }

  TestingProfile& profile() { return profile_; }
  PrefService* prefs() { return profile_.GetPrefs(); }
  MockMostVisitedHandler& mock_handler() { return *mock_handler_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  TemplateURLServiceFactoryTestUtil factory_util_{&profile_};
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<testing::NiceMock<MockMostVisitedHandler>> mock_handler_;
};

TEST_F(MostVisitedPrefObserverTest, InitialSetup_DefaultPrefs) {
  EXPECT_CALL(mock_handler(),
              EnableTileTypes(EnableTileTypesOptionsEq(
                  ntp_tiles::MostVisitedSites::EnableTileTypesOptions()
                      .with_custom_links(true)
                      .with_top_sites(false)
                      .with_enterprise_shortcuts(false))))
      .Times(1);
  EXPECT_CALL(mock_handler(), SetShortcutsVisible(true)).Times(1);

  MostVisitedPrefObserver observer(&profile(), &mock_handler());
}

TEST_F(MostVisitedPrefObserverTest, InitialSetup_ShortcutsHidden) {
  prefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, false);

  EXPECT_CALL(mock_handler(), SetShortcutsVisible(false)).Times(1);

  MostVisitedPrefObserver observer(&profile(), &mock_handler());
}

TEST_F(MostVisitedPrefObserverTest, InitialSetup_TopSitesEnabled) {
  prefs()->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, false);

  EXPECT_CALL(mock_handler(),
              EnableTileTypes(EnableTileTypesOptionsEq(
                  ntp_tiles::MostVisitedSites::EnableTileTypesOptions()
                      .with_custom_links(false)
                      .with_top_sites(true)
                      .with_enterprise_shortcuts(false))))
      .Times(1);

  MostVisitedPrefObserver observer(&profile(), &mock_handler());
}

TEST_F(MostVisitedPrefObserverTest, DynamicPrefChange_ShortcutsVisibility) {
  MostVisitedPrefObserver observer(&profile(), &mock_handler());

  EXPECT_CALL(mock_handler(), SetShortcutsVisible(false)).Times(1);
  prefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, false);

  EXPECT_CALL(mock_handler(), SetShortcutsVisible(true)).Times(1);
  prefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
}

TEST_F(MostVisitedPrefObserverTest, DynamicPrefChange_CustomLinksVisibility) {
  MostVisitedPrefObserver observer(&profile(), &mock_handler());

  // Toggling custom links to false enables top sites.
  EXPECT_CALL(mock_handler(),
              EnableTileTypes(EnableTileTypesOptionsEq(
                  ntp_tiles::MostVisitedSites::EnableTileTypesOptions()
                      .with_custom_links(false)
                      .with_top_sites(true)
                      .with_enterprise_shortcuts(false))))
      .Times(1);
  prefs()->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, false);

  // Toggling custom links to true enables custom links.
  EXPECT_CALL(mock_handler(),
              EnableTileTypes(EnableTileTypesOptionsEq(
                  ntp_tiles::MostVisitedSites::EnableTileTypesOptions()
                      .with_custom_links(true)
                      .with_top_sites(false)
                      .with_enterprise_shortcuts(false))))
      .Times(1);
  prefs()->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, true);
}

TEST_F(MostVisitedPrefObserverTest, DynamicPrefChange_PersonalShortcuts) {
  MostVisitedPrefObserver observer(&profile(), &mock_handler());

  EXPECT_CALL(mock_handler(), EnableTileTypes(testing::_)).Times(1);
  prefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible, false);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(MostVisitedPrefObserverTest,
       EnterpriseShortcutsPolicy_InitSetsVisibilityIfUnset) {
  base::DictValue shortcut_item;
  shortcut_item.Set("url", "https://enterprise.test");
  base::ListValue policy_list;
  policy_list.Append(std::move(shortcut_item));
  prefs()->SetList(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
                   std::move(policy_list));

  ASSERT_FALSE(prefs()->HasPrefPath(ntp_prefs::kNtpEnterpriseShortcutsVisible));

  MostVisitedPrefObserver observer(&profile(), &mock_handler());

  // Policy presence should automatically enable enterprise shortcuts
  // visibility.
  EXPECT_TRUE(prefs()->GetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible));
}

TEST_F(MostVisitedPrefObserverTest,
       EnterpriseShortcutsPolicy_DynamicPolicyUpdateSetsVisibility) {
  MostVisitedPrefObserver observer(&profile(), &mock_handler());

  ASSERT_FALSE(prefs()->HasPrefPath(ntp_prefs::kNtpEnterpriseShortcutsVisible));

  base::DictValue shortcut_item;
  shortcut_item.Set("url", "https://enterprise.test");
  base::ListValue policy_list;
  policy_list.Append(std::move(shortcut_item));
  prefs()->SetList(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
                   std::move(policy_list));

  EXPECT_TRUE(prefs()->GetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(MostVisitedPrefObserverTest, ResetProfilePrefs) {
  prefs()->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, false);
  prefs()->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible, true);
  prefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, false);
  prefs()->SetInteger(ntp_prefs::kNtpShortcutsStalenessCount, 5);
  prefs()->SetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled, true);
  prefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible, false);
  prefs()->SetBoolean(ntp_prefs::kNtpShowAllMostVisitedTiles, true);
  prefs()->SetInt64(ntp_prefs::kNtpMostVisitedTileHoverCount, 42);
  prefs()->SetInt64(ntp_prefs::kNtpMostVisitedTileNavigationCount, 24);

  MostVisitedPrefObserver::ResetProfilePrefs(prefs());

  EXPECT_TRUE(prefs()->GetBoolean(ntp_prefs::kNtpCustomLinksVisible));
  EXPECT_FALSE(prefs()->GetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible));
  EXPECT_TRUE(prefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible));
  EXPECT_EQ(0, prefs()->GetInteger(ntp_prefs::kNtpShortcutsStalenessCount));
  EXPECT_FALSE(
      prefs()->GetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
  EXPECT_TRUE(prefs()->GetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible));
  EXPECT_FALSE(prefs()->GetBoolean(ntp_prefs::kNtpShowAllMostVisitedTiles));
  EXPECT_EQ(0, prefs()->GetInt64(ntp_prefs::kNtpMostVisitedTileHoverCount));
  EXPECT_EQ(0,
            prefs()->GetInt64(ntp_prefs::kNtpMostVisitedTileNavigationCount));
}

TEST_F(MostVisitedPrefObserverTest,
       MigrateDeprecatedUseMostVisitedTilesPref_True) {
  prefs()->SetBoolean(ntp_prefs::kNtpUseMostVisitedTiles, true);

  MostVisitedPrefObserver::MigrateDeprecatedUseMostVisitedTilesPref(prefs());

  EXPECT_EQ(static_cast<int>(ntp_tiles::TileType::kTopSites),
            prefs()->GetInteger(ntp_prefs::kNtpShortcutsType));
  EXPECT_EQ(nullptr,
            prefs()->GetUserPrefValue(ntp_prefs::kNtpUseMostVisitedTiles));
}

TEST_F(MostVisitedPrefObserverTest,
       MigrateDeprecatedUseMostVisitedTilesPref_False) {
  prefs()->SetBoolean(ntp_prefs::kNtpUseMostVisitedTiles, false);

  MostVisitedPrefObserver::MigrateDeprecatedUseMostVisitedTilesPref(prefs());

  EXPECT_EQ(static_cast<int>(ntp_tiles::TileType::kCustomLinks),
            prefs()->GetInteger(ntp_prefs::kNtpShortcutsType));
  EXPECT_EQ(nullptr,
            prefs()->GetUserPrefValue(ntp_prefs::kNtpUseMostVisitedTiles));
}

TEST_F(MostVisitedPrefObserverTest,
       MigrateDeprecatedShortcutsTypePref_TopSites) {
  prefs()->SetInteger(ntp_prefs::kNtpShortcutsType,
                      static_cast<int>(ntp_tiles::TileType::kTopSites));

  MostVisitedPrefObserver::MigrateDeprecatedShortcutsTypePref(prefs());

  EXPECT_FALSE(prefs()->GetBoolean(ntp_prefs::kNtpCustomLinksVisible));
  EXPECT_FALSE(prefs()->GetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible));
  EXPECT_EQ(nullptr, prefs()->GetUserPrefValue(ntp_prefs::kNtpShortcutsType));
}

TEST_F(MostVisitedPrefObserverTest,
       MigrateDeprecatedShortcutsTypePref_CustomLinks) {
  prefs()->SetInteger(ntp_prefs::kNtpShortcutsType,
                      static_cast<int>(ntp_tiles::TileType::kCustomLinks));

  MostVisitedPrefObserver::MigrateDeprecatedShortcutsTypePref(prefs());

  EXPECT_TRUE(prefs()->GetBoolean(ntp_prefs::kNtpCustomLinksVisible));
  EXPECT_FALSE(prefs()->GetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible));
  EXPECT_EQ(nullptr, prefs()->GetUserPrefValue(ntp_prefs::kNtpShortcutsType));
}

TEST_F(MostVisitedPrefObserverTest,
       MigrateDeprecatedShortcutsTypePref_Enterprise) {
  prefs()->SetInteger(
      ntp_prefs::kNtpShortcutsType,
      static_cast<int>(ntp_tiles::TileType::kEnterpriseShortcuts));

  MostVisitedPrefObserver::MigrateDeprecatedShortcutsTypePref(prefs());

  EXPECT_FALSE(prefs()->GetBoolean(ntp_prefs::kNtpCustomLinksVisible));
  EXPECT_TRUE(prefs()->GetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible));
  EXPECT_EQ(nullptr, prefs()->GetUserPrefValue(ntp_prefs::kNtpShortcutsType));
}

}  // namespace

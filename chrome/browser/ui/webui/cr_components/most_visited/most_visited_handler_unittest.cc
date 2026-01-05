// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_handler.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/ui/search/ntp_user_data_types.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/cr_components/most_visited/most_visited.mojom.h"

namespace {

struct MostVisitedAutoRemovalTestParam {
  bool custom_links_enabled;
};

class MockMostVisitedPage : public most_visited::mojom::MostVisitedPage {
 public:
  MockMostVisitedPage() = default;
  ~MockMostVisitedPage() override = default;

  mojo::PendingRemote<most_visited::mojom::MostVisitedPage> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              SetMostVisitedInfo,
              (most_visited::mojom::MostVisitedInfoPtr info),
              (override));
  MOCK_METHOD(void, OnMostVisitedTilesAutoRemoval, (), (override));

 private:
  mojo::Receiver<most_visited::mojom::MostVisitedPage> receiver_{this};
};

class MostVisitedAutoRemovalTest
    : public ::testing::TestWithParam<MostVisitedAutoRemovalTestParam> {
 public:
  MostVisitedAutoRemovalTest() = default;

  void SetUp() override {
    SetUpGoogleDefaultSearchProvider();
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    handler_ = std::make_unique<MostVisitedHandler>(
        mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandler>(),
        page_.BindAndGetRemote(), &profile_, web_contents_.get(), GURL(),
        base::Time());
    handler_->EnableTileTypes(
        ntp_tiles::MostVisitedSites::EnableTileTypesOptions().with_custom_links(
            GetParam().custom_links_enabled));
  }

  void InitFeature(bool enable) {
    if (enable) {
      feature_list_.InitAndEnableFeatureWithParameters(
          ntp_features::kNtpFeatureOptimizationShortcutsRemoval,
          {{ntp_features::kStaleShortcutsCountThreshold.name, "5"}});
    }
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

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  TemplateURLServiceFactoryTestUtil factory_util_{&profile_};
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<MostVisitedHandler> handler_;
  testing::NiceMock<MockMostVisitedPage> page_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

INSTANTIATE_TEST_SUITE_P(
    MostVisitedHandlerTest,
    MostVisitedAutoRemovalTest,
    ::testing::Values(MostVisitedAutoRemovalTestParam{false},
                      MostVisitedAutoRemovalTestParam{true}));

TEST_P(MostVisitedAutoRemovalTest, AddMostVisitedTile) {
  ASSERT_FALSE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
  handler_->AddMostVisitedTile(GURL("https://foo.com"), "Foo",
                               base::DoNothing());
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
}

TEST_P(MostVisitedAutoRemovalTest, DeleteMostVisitedTile) {
  auto tile = most_visited::mojom::MostVisitedTile::New();
  tile->url = GURL("https://foo.com");
  handler_->AddMostVisitedTile(tile->url, "Foo", base::DoNothing());
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled,
                                  false);
  ASSERT_FALSE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));

  handler_->DeleteMostVisitedTile(std::move(tile));

  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
}

TEST_P(MostVisitedAutoRemovalTest, ReorderMostVisitedTile) {
  auto tile = most_visited::mojom::MostVisitedTile::New();
  tile->url = GURL("https://foo.com");
  handler_->AddMostVisitedTile(tile->url, "Foo", base::DoNothing());
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled,
                                  false);
  ASSERT_FALSE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));

  handler_->ReorderMostVisitedTile(std::move(tile), 0);

  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
}

TEST_P(MostVisitedAutoRemovalTest, UpdateMostVisitedTile) {
  auto tile = most_visited::mojom::MostVisitedTile::New();
  tile->url = GURL("https://foo.com");
  handler_->AddMostVisitedTile(tile->url, "Foo", base::DoNothing());
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled,
                                  false);
  ASSERT_FALSE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));

  handler_->UpdateMostVisitedTile(std::move(tile), GURL("https://bar.com"),
                                  "Bar", base::DoNothing());

  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
}

TEST_P(MostVisitedAutoRemovalTest, SetMostVisitedExpandedState) {
  ASSERT_FALSE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
  handler_->SetMostVisitedExpandedState(true);
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
}

TEST_P(MostVisitedAutoRemovalTest, OnMostVisitedTileNavigation) {
  ASSERT_FALSE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
  auto tile = most_visited::mojom::MostVisitedTile::New();
  tile->url = GURL("https://foo.com");

  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://bar.com"));
  handler_->OnMostVisitedTileNavigation(std::move(tile), 0, 0, false, false,
                                        false, false);

  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
}

// Verify that UndoMostVisitedAutoRemoval restores shortcuts visibility and
// disables future auto removal.
TEST_P(MostVisitedAutoRemovalTest, UndoMostVisitedAutoRemoval) {
  ASSERT_FALSE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
  // Hide the shortcuts to test that the undo method sets it to true.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, false);

  handler_->UndoMostVisitedAutoRemoval();

  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible));
  histogram_tester_.ExpectUniqueSample(
      "NewTabPage.CustomizeShortcutAction",
      CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_AUTO_REMOVE_UNDO, 1);
}

TEST_P(MostVisitedAutoRemovalTest, OnMostVisitedTilesRendered) {
  // Ensure conditions for staleness update are met.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled,
                                  false);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);

  // Set initial staleness time to be older than the threshold.
  base::Time start_time =
      base::Time::Now() -
      ntp_features::kShortcutsMinStalenessUpdateTimeInterval.Get() -
      base::Days(1);
  profile_.GetPrefs()->SetTime(ntp_prefs::kNtpLastShortcutsStalenessUpdate,
                               start_time);
  profile_.GetPrefs()->SetInteger(ntp_prefs::kNtpShortcutsStalenessCount, 0);

  // Call with non-empty tiles - staleness should be updated.
  std::vector<most_visited::mojom::MostVisitedTilePtr> tiles;
  tiles.push_back(most_visited::mojom::MostVisitedTile::New());
  tiles[0]->url = GURL("https://foo.com");

  handler_->OnMostVisitedTilesRendered(std::move(tiles), 0.0);

  EXPECT_GT(
      profile_.GetPrefs()->GetTime(ntp_prefs::kNtpLastShortcutsStalenessUpdate),
      start_time);
  EXPECT_EQ(
      profile_.GetPrefs()->GetInteger(ntp_prefs::kNtpShortcutsStalenessCount),
      1);
}

TEST_P(MostVisitedAutoRemovalTest, DoNotRemoveStaleShortcutsIfFeatureDisabled) {
  InitFeature(false);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled,
                                  false);
  profile_.GetPrefs()->SetInteger(ntp_prefs::kNtpShortcutsStalenessCount, 5);

  EXPECT_CALL(page_, OnMostVisitedTilesAutoRemoval()).Times(0);
  static_cast<ntp_tiles::MostVisitedSites::Observer*>(handler_.get())
      ->OnURLsAvailable(false, {{ntp_tiles::SectionType::PERSONALIZED, {}}});

  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible));
}

TEST_P(MostVisitedAutoRemovalTest,
       DoNotRemoveStaleShortcutsIfEnterpriseShortcutsEnabled) {
  InitFeature(true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled,
                                  false);
  profile_.GetPrefs()->SetInteger(ntp_prefs::kNtpShortcutsStalenessCount, 5);

  handler_->EnableTileTypes(
      ntp_tiles::MostVisitedSites::EnableTileTypesOptions()
          .with_custom_links(GetParam().custom_links_enabled)
          .with_enterprise_shortcuts(true));

  EXPECT_CALL(page_, OnMostVisitedTilesAutoRemoval()).Times(0);
  static_cast<ntp_tiles::MostVisitedSites::Observer*>(handler_.get())
      ->OnURLsAvailable(false, {{ntp_tiles::SectionType::PERSONALIZED, {}}});

  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible));
}

TEST_P(MostVisitedAutoRemovalTest, RemoveStaleShortcutsIfReachThreshold) {
  InitFeature(true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled,
                                  false);
  profile_.GetPrefs()->SetInteger(ntp_prefs::kNtpShortcutsStalenessCount, 5);

  base::RunLoop run_loop;
  EXPECT_CALL(page_, OnMostVisitedTilesAutoRemoval())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  static_cast<ntp_tiles::MostVisitedSites::Observer*>(handler_.get())
      ->OnURLsAvailable(false, {{ntp_tiles::SectionType::PERSONALIZED, {}}});
  run_loop.Run();

  EXPECT_FALSE(
      profile_.GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible));
  EXPECT_EQ(5, profile_.GetPrefs()->GetInteger(
                   ntp_prefs::kNtpShortcutsStalenessCount));
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
  histogram_tester_.ExpectUniqueSample(
      "NewTabPage.CustomizeShortcutAction",
      CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_AUTO_REMOVE, 1);
}

TEST_P(MostVisitedAutoRemovalTest, DoNotRemoveStaleShortcutsIfAlreadyHidden) {
  InitFeature(true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, false);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled,
                                  false);
  profile_.GetPrefs()->SetInteger(ntp_prefs::kNtpShortcutsStalenessCount, 5);

  EXPECT_CALL(page_, OnMostVisitedTilesAutoRemoval()).Times(0);
  static_cast<ntp_tiles::MostVisitedSites::Observer*>(handler_.get())
      ->OnURLsAvailable(false, {{ntp_tiles::SectionType::PERSONALIZED, {}}});

  EXPECT_FALSE(
      profile_.GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible));
}

TEST_P(MostVisitedAutoRemovalTest, DoNotRemoveStaleShortcutsIfDisabled) {
  InitFeature(true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled,
                                  true);
  profile_.GetPrefs()->SetInteger(ntp_prefs::kNtpShortcutsStalenessCount, 5);

  EXPECT_CALL(page_, OnMostVisitedTilesAutoRemoval()).Times(0);
  static_cast<ntp_tiles::MostVisitedSites::Observer*>(handler_.get())
      ->OnURLsAvailable(false, {{ntp_tiles::SectionType::PERSONALIZED, {}}});

  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible));
}

TEST_P(MostVisitedAutoRemovalTest,
       DoNotRemoveStaleShortcutsIfNotAboveThreshold) {
  InitFeature(true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled,
                                  false);
  profile_.GetPrefs()->SetInteger(ntp_prefs::kNtpShortcutsStalenessCount, 4);

  EXPECT_CALL(page_, OnMostVisitedTilesAutoRemoval()).Times(0);
  static_cast<ntp_tiles::MostVisitedSites::Observer*>(handler_.get())
      ->OnURLsAvailable(false, {{ntp_tiles::SectionType::PERSONALIZED, {}}});

  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible));
}

}  // namespace

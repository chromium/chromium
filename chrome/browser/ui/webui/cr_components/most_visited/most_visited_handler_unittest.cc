// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_handler.h"

#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/cr_components/most_visited/most_visited.mojom.h"

namespace {

struct ShortcutsAutoRemovalPrefTestParam {
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

 private:
  mojo::Receiver<most_visited::mojom::MostVisitedPage> receiver_{this};
};

class ShortcutsAutoRemovalPrefTest
    : public ::testing::TestWithParam<ShortcutsAutoRemovalPrefTestParam> {
 public:
  ShortcutsAutoRemovalPrefTest() = default;

  void SetUp() override {
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

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<MostVisitedHandler> handler_;
  testing::NiceMock<MockMostVisitedPage> page_;
};

INSTANTIATE_TEST_SUITE_P(
    MostVisitedHandlerTest,
    ShortcutsAutoRemovalPrefTest,
    ::testing::Values(ShortcutsAutoRemovalPrefTestParam{false},
                      ShortcutsAutoRemovalPrefTestParam{true}));

TEST_P(ShortcutsAutoRemovalPrefTest, AddMostVisitedTile) {
  ASSERT_FALSE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
  handler_->AddMostVisitedTile(GURL("https://foo.com"), "Foo",
                               base::DoNothing());
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
}

TEST_P(ShortcutsAutoRemovalPrefTest, DeleteMostVisitedTile) {
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

TEST_P(ShortcutsAutoRemovalPrefTest, ReorderMostVisitedTile) {
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

TEST_P(ShortcutsAutoRemovalPrefTest, UpdateMostVisitedTile) {
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

TEST_P(ShortcutsAutoRemovalPrefTest, SetMostVisitedExpandedState) {
  ASSERT_FALSE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
  handler_->SetMostVisitedExpandedState(true);
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled));
}

TEST_P(ShortcutsAutoRemovalPrefTest, OnMostVisitedTileNavigation) {
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

}  // namespace

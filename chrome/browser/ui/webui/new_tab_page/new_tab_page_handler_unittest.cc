// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using testing::DoAll;

class MockInstantService : public InstantService {
 public:
  explicit MockInstantService(Profile* profile) : InstantService(profile) {}
  ~MockInstantService() override = default;

  MOCK_METHOD1(AddObserver, void(InstantServiceObserver*));
  MOCK_METHOD0(UpdateNtpTheme, void());
};

class MockPage : public new_tab_page::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<new_tab_page::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD1(SetTheme, void(new_tab_page::mojom::ThemePtr));
  MOCK_METHOD2(SetDisabledModules, void(bool, const std::vector<std::string>&));

  mojo::Receiver<new_tab_page::mojom::Page> receiver_{this};
};

}  // namespace

class NewTabPageHandlerTest : public testing::Test {
 public:
  NewTabPageHandlerTest()
      : mock_instant_service_(&profile_),
        web_contents_(factory_.CreateWebContents(&profile_)) {}

  ~NewTabPageHandlerTest() override = default;

  void SetUp() override {
    EXPECT_CALL(mock_instant_service_, AddObserver)
        .WillOnce(DoAll(testing::SaveArg<0>(&instant_service_observer_)));
    EXPECT_CALL(mock_instant_service_, UpdateNtpTheme());
    handler_ = std::make_unique<NewTabPageHandler>(
        mojo::PendingReceiver<new_tab_page::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), &profile_, &mock_instant_service_,
        web_contents_, base::Time::Now());
    EXPECT_EQ(handler_.get(), instant_service_observer_);
  }

  void TearDown() override { testing::Test::TearDown(); }

 protected:
  testing::NiceMock<MockPage> mock_page_;
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  MockInstantService mock_instant_service_;
  content::TestWebContentsFactory factory_;
  content::WebContents* web_contents_;  // Weak. Owned by factory_.
  base::HistogramTester histogram_tester_;
  std::unique_ptr<NewTabPageHandler> handler_;
  InstantServiceObserver* instant_service_observer_;
};

TEST_F(NewTabPageHandlerTest, SetTheme) {
  new_tab_page::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce(testing::Invoke([&theme](new_tab_page::mojom::ThemePtr arg) {
        theme = std::move(arg);
      }));
  NtpTheme ntp_theme;
  ntp_theme.custom_background_attribution_line_1 = "foo line";
  ntp_theme.custom_background_attribution_line_2 = "bar line";
  ntp_theme.custom_background_attribution_action_url = GURL("https://foo.com");
  ntp_theme.collection_id = "foo";
  ntp_theme.background_color = SkColorSetRGB(0, 0, 1);
  ntp_theme.text_color = SkColorSetRGB(0, 0, 2);
  ntp_theme.using_default_theme = false;
  ntp_theme.logo_alternate = true;
  ntp_theme.logo_color = SkColorSetRGB(0, 0, 3);
  ntp_theme.theme_id = "bar";
  ntp_theme.image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_CENTER;
  ntp_theme.image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_TOP;
  ntp_theme.image_tiling = THEME_BKGRND_IMAGE_REPEAT_X;
  ntp_theme.has_attribution = true;
  ntp_theme.has_theme_image = true;
  ntp_theme.shortcut_color = SkColorSetRGB(0, 0, 4);
  ntp_theme.search_box.bg = SkColorSetRGB(0, 0, 5);
  ntp_theme.search_box.icon = SkColorSetRGB(0, 0, 6);
  ntp_theme.search_box.icon_selected = SkColorSetRGB(0, 0, 7);
  ntp_theme.search_box.placeholder = SkColorSetRGB(0, 0, 8);
  ntp_theme.search_box.results_bg = SkColorSetRGB(0, 0, 9);
  ntp_theme.search_box.results_bg_hovered = SkColorSetRGB(0, 0, 10);
  ntp_theme.search_box.results_bg_selected = SkColorSetRGB(0, 0, 11);
  ntp_theme.search_box.results_dim = SkColorSetRGB(0, 0, 12);
  ntp_theme.search_box.results_dim_selected = SkColorSetRGB(0, 0, 13);
  ntp_theme.search_box.results_text = SkColorSetRGB(0, 0, 14);
  ntp_theme.search_box.results_text_selected = SkColorSetRGB(0, 0, 15);
  ntp_theme.search_box.results_url = SkColorSetRGB(0, 0, 16);
  ntp_theme.search_box.results_url_selected = SkColorSetRGB(0, 0, 17);
  ntp_theme.search_box.text = SkColorSetRGB(0, 0, 18);

  instant_service_observer_->NtpThemeChanged(ntp_theme);
  mock_page_.FlushForTesting();

  EXPECT_EQ(SkColorSetRGB(0, 0, 1), theme->background_color);
  EXPECT_EQ(SkColorSetRGB(0, 0, 2), theme->text_color);
  EXPECT_FALSE(theme->is_default);
  EXPECT_FALSE(theme->is_dark);
  EXPECT_EQ(SkColorSetRGB(0, 0, 3), theme->logo_color);
  EXPECT_EQ("foo", theme->daily_refresh_collection_id);
  EXPECT_EQ("chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND?bar",
            theme->background_image->url);
  EXPECT_EQ("chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND@2x?bar",
            theme->background_image->url_2x);
  EXPECT_EQ("chrome://theme/IDR_THEME_NTP_ATTRIBUTION?bar",
            theme->background_image->attribution_url);
  EXPECT_EQ("initial", theme->background_image->size);
  EXPECT_EQ("repeat", theme->background_image->repeat_x);
  EXPECT_EQ("no-repeat", theme->background_image->repeat_y);
  EXPECT_EQ("center", theme->background_image->position_x);
  EXPECT_EQ("top", theme->background_image->position_y);
  EXPECT_EQ("foo line", theme->background_image_attribution_1);
  EXPECT_EQ("bar line", theme->background_image_attribution_2);
  EXPECT_EQ(GURL("https://foo.com"), theme->background_image_attribution_url);
  EXPECT_EQ(SkColorSetRGB(0, 0, 4), theme->most_visited->background_color);
  EXPECT_TRUE(theme->most_visited->use_white_tile_icon);
  EXPECT_TRUE(theme->most_visited->use_title_pill);
  EXPECT_EQ(false, theme->most_visited->is_dark);
  EXPECT_EQ(SkColorSetRGB(0, 0, 5), theme->search_box->bg);
  EXPECT_EQ(SkColorSetRGB(0, 0, 6), theme->search_box->icon);
  EXPECT_EQ(SkColorSetRGB(0, 0, 7), theme->search_box->icon_selected);
  EXPECT_EQ(SkColorSetRGB(0, 0, 8), theme->search_box->placeholder);
  EXPECT_EQ(SkColorSetRGB(0, 0, 9), theme->search_box->results_bg);
  EXPECT_EQ(SkColorSetRGB(0, 0, 10), theme->search_box->results_bg_hovered);
  EXPECT_EQ(SkColorSetRGB(0, 0, 11), theme->search_box->results_bg_selected);
  EXPECT_EQ(SkColorSetRGB(0, 0, 12), theme->search_box->results_dim);
  EXPECT_EQ(SkColorSetRGB(0, 0, 13), theme->search_box->results_dim_selected);
  EXPECT_EQ(SkColorSetRGB(0, 0, 14), theme->search_box->results_text);
  EXPECT_EQ(SkColorSetRGB(0, 0, 15), theme->search_box->results_text_selected);
  EXPECT_EQ(SkColorSetRGB(0, 0, 16), theme->search_box->results_url);
  EXPECT_EQ(SkColorSetRGB(0, 0, 17), theme->search_box->results_url_selected);
  EXPECT_EQ(SkColorSetRGB(0, 0, 18), theme->search_box->text);
}

TEST_F(NewTabPageHandlerTest, Histograms) {
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleDismissedHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleRestoredHistogram, 0);

  handler_->OnDismissModule("shopping_tasks");
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleDismissedHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      std::string(NewTabPageHandler::kModuleDismissedHistogram) +
          ".shopping_tasks",
      1);

  handler_->OnRestoreModule("kaleidoscope");
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleRestoredHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      std::string(NewTabPageHandler::kModuleRestoredHistogram) +
          ".kaleidoscope",
      1);
}

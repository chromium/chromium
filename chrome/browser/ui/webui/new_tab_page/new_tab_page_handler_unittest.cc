// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "base/memory/ref_counted_memory.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_provider_logos/logo_common.h"
#include "components/search_provider_logos/logo_service.h"
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

class MockLogoService : public search_provider_logos::LogoService {
 public:
  MOCK_METHOD2(GetLogo, void(search_provider_logos::LogoCallbacks, bool));
  MOCK_METHOD1(GetLogo, void(search_provider_logos::LogoObserver*));
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
        &mock_logo_service_, web_contents_, base::Time::Now());
    EXPECT_EQ(handler_.get(), instant_service_observer_);
  }

  void TearDown() override { testing::Test::TearDown(); }

  new_tab_page::mojom::DoodlePtr GetDoodle(
      const search_provider_logos::EncodedLogo& logo) {
    search_provider_logos::EncodedLogoCallback on_cached_encoded_logo_available;
    EXPECT_CALL(mock_logo_service_, GetLogo(testing::_, testing::_))
        .Times(1)
        .WillOnce(
            testing::Invoke([&on_cached_encoded_logo_available](
                                search_provider_logos::LogoCallbacks callbacks,
                                bool for_webui_ntp) {
              on_cached_encoded_logo_available =
                  std::move(callbacks.on_cached_encoded_logo_available);
            }));
    base::MockCallback<NewTabPageHandler::GetDoodleCallback> callback;
    new_tab_page::mojom::DoodlePtr doodle;
    EXPECT_CALL(callback, Run(testing::_))
        .Times(1)
        .WillOnce(
            testing::Invoke([&doodle](new_tab_page::mojom::DoodlePtr arg) {
              doodle = std::move(arg);
            }));
    handler_->GetDoodle(callback.Get());

    std::move(on_cached_encoded_logo_available)
        .Run(search_provider_logos::LogoCallbackReason::DETERMINED,
             absl::make_optional(logo));

    return doodle;
  }

 protected:
  testing::NiceMock<MockPage> mock_page_;
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  MockInstantService mock_instant_service_;
  MockLogoService mock_logo_service_;
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

TEST_F(NewTabPageHandlerTest, GetAnimatedDoodle) {
  search_provider_logos::EncodedLogo logo;
  std::string encoded_image("light image");
  std::string dark_encoded_image("dark image");
  logo.encoded_image = base::RefCountedString::TakeString(&encoded_image);
  logo.dark_encoded_image =
      base::RefCountedString::TakeString(&dark_encoded_image);
  logo.metadata.type = search_provider_logos::LogoType::ANIMATED;
  logo.metadata.on_click_url = GURL("https://doodle.com/on_click_url");
  logo.metadata.alt_text = "alt text";
  logo.metadata.mime_type = "light_mime_type";
  logo.metadata.dark_mime_type = "dark_mime_type";
  logo.metadata.dark_background_color = "#000001";
  logo.metadata.animated_url = GURL("https://doodle.com/light_animation");
  logo.metadata.dark_animated_url = GURL("https://doodle.com/dark_animation");
  logo.metadata.cta_log_url = GURL("https://doodle.com/light_cta_log_url");
  logo.metadata.dark_cta_log_url = GURL("https://doodle.com/dark_cta_log_url");
  logo.metadata.log_url = GURL("https://doodle.com/light_log_url");
  logo.metadata.dark_log_url = GURL("https://doodle.com/dark_log_url");
  logo.metadata.short_link = GURL("https://doodle.com/short_link");
  logo.metadata.width_px = 1;
  logo.metadata.height_px = 2;
  logo.metadata.dark_width_px = 3;
  logo.metadata.dark_height_px = 4;
  logo.metadata.share_button_x = 5;
  logo.metadata.dark_share_button_x = 6;
  logo.metadata.share_button_y = 7;
  logo.metadata.dark_share_button_y = 8;
  logo.metadata.share_button_opacity = 0.5;
  logo.metadata.dark_share_button_opacity = 0.7;
  logo.metadata.share_button_icon = "light share button";
  logo.metadata.dark_share_button_icon = "dark share button";
  logo.metadata.share_button_bg = "#000100";
  logo.metadata.dark_share_button_bg = "#010000";

  auto doodle = GetDoodle(logo);

  ASSERT_TRUE(doodle);
  ASSERT_TRUE(doodle->image);
  ASSERT_FALSE(doodle->interactive);
  EXPECT_EQ("data:light_mime_type;base64,bGlnaHQgaW1hZ2U=",
            doodle->image->light->image_url);
  EXPECT_EQ("https://doodle.com/light_animation",
            doodle->image->light->animation_url);
  EXPECT_EQ(1u, doodle->image->light->width);
  EXPECT_EQ(2u, doodle->image->light->height);
  EXPECT_EQ(SK_ColorWHITE, doodle->image->light->background_color);
  EXPECT_EQ(5, doodle->image->light->share_button->x);
  EXPECT_EQ(7, doodle->image->light->share_button->y);
  EXPECT_EQ(SkColorSetARGB(127, 0, 1, 0),
            doodle->image->light->share_button->background_color);
  EXPECT_EQ("data:image/png;base64,light share button",
            doodle->image->light->share_button->icon_url);
  EXPECT_EQ("https://doodle.com/light_cta_log_url",
            doodle->image->light->image_impression_log_url);
  EXPECT_EQ("https://doodle.com/light_log_url",
            doodle->image->light->animation_impression_log_url);
  EXPECT_EQ("data:dark_mime_type;base64,ZGFyayBpbWFnZQ==",
            doodle->image->dark->image_url);
  EXPECT_EQ("https://doodle.com/dark_animation",
            doodle->image->dark->animation_url);
  EXPECT_EQ(3u, doodle->image->dark->width);
  EXPECT_EQ(4u, doodle->image->dark->height);
  EXPECT_EQ(SkColorSetRGB(0, 0, 1), doodle->image->dark->background_color);
  EXPECT_EQ(6, doodle->image->dark->share_button->x);
  EXPECT_EQ(8, doodle->image->dark->share_button->y);
  EXPECT_EQ(SkColorSetARGB(178, 1, 0, 0),
            doodle->image->dark->share_button->background_color);
  EXPECT_EQ("data:image/png;base64,dark share button",
            doodle->image->dark->share_button->icon_url);
  EXPECT_EQ("https://doodle.com/dark_cta_log_url",
            doodle->image->dark->image_impression_log_url);
  EXPECT_EQ("https://doodle.com/dark_log_url",
            doodle->image->dark->animation_impression_log_url);
  EXPECT_EQ("https://doodle.com/on_click_url", doodle->image->on_click_url);
  EXPECT_EQ("https://doodle.com/short_link", doodle->image->share_url);
  EXPECT_EQ("alt text", doodle->description);
}

TEST_F(NewTabPageHandlerTest, GetInteractiveDoodle) {
  search_provider_logos::EncodedLogo logo;
  logo.metadata.type = search_provider_logos::LogoType::INTERACTIVE;
  logo.metadata.full_page_url = GURL("https://doodle.com/full_page_url");
  logo.metadata.iframe_width_px = 1;
  logo.metadata.iframe_height_px = 2;
  logo.metadata.alt_text = "alt text";

  auto doodle = GetDoodle(logo);

  EXPECT_EQ("https://doodle.com/full_page_url", doodle->interactive->url);
  EXPECT_EQ(1u, doodle->interactive->width);
  EXPECT_EQ(2u, doodle->interactive->height);
  EXPECT_EQ("alt text", doodle->description);
}

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/theme_source.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_user_profile.h"
#include "chrome/browser/ui/webui/util/webui_util_desktop.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/test/base/test_theme_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "url/gurl.h"

class WebUISourcesTest : public testing::Test {
 public:
  WebUISourcesTest() = default;

  TestingProfile* profile() const { return profile_.get(); }
  ThemeSource* theme_source() const { return theme_source_.get(); }
  const content::WebContents::Getter& web_contents_getter() const {
    return test_web_contents_getter_;
  }
  size_t result_data_size() const { return result_data_size_; }

  void StartDataRequest(const std::string& source) {
    theme_source()->StartDataRequest(
        GURL(base::StrCat({content::kChromeUIScheme, "://",
                           chrome::kChromeUIThemeHost, "/", source})),
        test_web_contents_getter_,
        base::BindOnce(&WebUISourcesTest::SendResponse,
                       base::Unretained(this)));
  }

  size_t result_data_size_ = 0;

 private:
  void SetUp() override {
    webui::SetThemeProviderForTestingDeprecated(&test_theme_provider_);
    profile_ = std::make_unique<TestingProfile>();
    theme_source_ = std::make_unique<ThemeSource>(profile_.get());
    test_web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    test_web_contents_getter_ =
        base::BindLambdaForTesting([&] { return test_web_contents_.get(); });
  }

  void TearDown() override {
    theme_source_.reset();
    test_web_contents_.reset();
    test_web_contents_getter_ = content::WebContents::Getter();
    profile_.reset();
    webui::SetThemeProviderForTestingDeprecated(nullptr);
  }

  void SendResponse(scoped_refptr<base::RefCountedMemory> data) {
    result_data_size_ = data ? data->size() : 0;
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;

  TestThemeProvider test_theme_provider_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ThemeSource> theme_source_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  content::WebContents::Getter test_web_contents_getter_;
};

TEST_F(WebUISourcesTest, ThemeSourceMimeTypes) {
  std::vector<std::string> css_urls{
      "chrome://theme/css/new_tab_theme.css",
      "chrome://theme/css/new_tab_theme.css?foo",
      "chrome://theme/colors.css?sets.css",
      "chrome://theme/colors.css?sets.css=ui",
  };

  for (auto url : css_urls) {
    EXPECT_EQ(theme_source()->GetMimeType(GURL(url)), "text/css");
  }

  std::vector<std::string> png_urls{
      "chrome://theme/current-channel-logo",
      "chrome://theme/other.png",
      "chrome://theme/WRONGURL",
  };

  for (auto url : png_urls) {
    EXPECT_EQ(theme_source()->GetMimeType(GURL(url)), "image/png");
  }
}

TEST_F(WebUISourcesTest, ThemeSourceImages) {
  // We used to PNGEncode the images ourselves, but encoder differences
  // invalidated that. We now just check that the image exists.
  StartDataRequest("IDR_THEME_FRAME_INCOGNITO");
  size_t min = 0;
  EXPECT_GT(result_data_size_, min);

  StartDataRequest("IDR_THEME_TOOLBAR");
  EXPECT_GT(result_data_size_, min);
}

TEST_F(WebUISourcesTest, ThemeSourceCSS) {
  // Generating the test data for the NTP CSS would just involve copying the
  // method, or being super brittle and hard-coding the result (requiring
  // an update to the unittest every time the CSS template changes), so we
  // just check for a successful request and data that is non-null.
  size_t empty_size = 0;

  StartDataRequest("css/new_tab_theme.css");
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(result_data_size_, empty_size);

  StartDataRequest("css/new_tab_theme.css?pie");
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(result_data_size_, empty_size);

#if !DCHECK_IS_ON()
  // Check that we send NULL back when we can't find what we're looking for.
  StartDataRequest("css/WRONGURL");
  EXPECT_EQ(result_data_size_, empty_size);
#endif
}

TEST_F(WebUISourcesTest, ThemeSourceColorsCSS) {
  // Check for a successful request and that the data is non-null under both
  // optimized and unoptimized (baseline) paths. The actual conversion of color
  // provider colors to css colors is tested in helper functions.
  for (bool feature_enabled : {false, true}) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(
        features::kColorIdCssStyleSheetOptimization, feature_enabled);

    result_data_size_ = 0;
    StartDataRequest("colors.css?sets=ui");
    base::RunLoop().RunUntilIdle();
    EXPECT_NE(result_data_size_, 0u);
  }
}

TEST_F(WebUISourcesTest, ThemeAllowedOrigin) {
  EXPECT_EQ(
      theme_source()->GetAccessControlAllowOriginForOrigin("chrome://settings"),
      "chrome://settings");
  EXPECT_EQ(theme_source()->GetAccessControlAllowOriginForOrigin(
                "chrome-extensions://some-id"),
            "");
  EXPECT_EQ(
      theme_source()->GetAccessControlAllowOriginForOrigin("http://google.com"),
      "");
  // Only the source serving chrome-untrusted://theme allows
  // chrome-untrusted:// origins.
  EXPECT_EQ(theme_source()->GetAccessControlAllowOriginForOrigin(
                "chrome-untrusted://new-tab-page"),
            "");
}

TEST_F(WebUISourcesTest, ThemeAllowedOriginUntrusted) {
  ThemeSource untrusted_theme_source(profile(), /*serve_untrusted=*/true);
  EXPECT_EQ(untrusted_theme_source.GetAccessControlAllowOriginForOrigin(
                "chrome-untrusted://new-tab-page"),
            "chrome-untrusted://new-tab-page");
  EXPECT_EQ(untrusted_theme_source.GetAccessControlAllowOriginForOrigin(
                "chrome://settings"),
            "chrome://settings");
  EXPECT_EQ(untrusted_theme_source.GetAccessControlAllowOriginForOrigin(
                "chrome-extensions://some-id"),
            "");
  EXPECT_EQ(untrusted_theme_source.GetAccessControlAllowOriginForOrigin(
                "http://google.com"),
            "");
}

TEST_F(WebUISourcesTest, ThemeSourceShouldServiceRequest) {
  EXPECT_TRUE(theme_source()->ShouldServiceRequest(
      GURL("chrome://theme/colors.css"), nullptr, -1));
  EXPECT_TRUE(theme_source()->ShouldServiceRequest(
      GURL("chrome://theme/IDR_THEME_FRAME"), nullptr, -1));

  EXPECT_TRUE(theme_source()->ShouldServiceRequest(
      GURL("devtools://theme/colors.css"), nullptr, -1));
  EXPECT_TRUE(theme_source()->ShouldServiceRequest(
      GURL("devtools://theme/colors.css?sets=ui,chrome"), nullptr, -1));

  EXPECT_FALSE(theme_source()->ShouldServiceRequest(
      GURL("devtools://theme/IDR_THEME_FRAME"), nullptr, -1));
  EXPECT_FALSE(theme_source()->ShouldServiceRequest(
      GURL("devtools://theme/css/new_tab_theme.css"), nullptr, -1));
  EXPECT_FALSE(theme_source()->ShouldServiceRequest(GURL("devtools://theme/"),
                                                    nullptr, -1));
  EXPECT_FALSE(theme_source()->ShouldServiceRequest(
      GURL("chrome-untrusted://theme/colors.css"), nullptr, -1));
  EXPECT_FALSE(theme_source()->ShouldServiceRequest(
      GURL("https://theme/colors.css"), nullptr, -1));

  ThemeSource untrusted_theme_source(profile(), /*serve_untrusted=*/true);
  EXPECT_TRUE(untrusted_theme_source.ShouldServiceRequest(
      GURL("chrome-untrusted://theme/colors.css"), nullptr, -1));
  EXPECT_FALSE(untrusted_theme_source.ShouldServiceRequest(
      GURL("chrome://theme/colors.css"), nullptr, -1));
}

namespace {

using ::testing::AllOf;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::StartsWith;

// Returns the data that `source` serves for `url`, or an empty string if it
// serves nothing.
std::string GetDataForUrl(ThemeSource& source,
                          const GURL& url,
                          content::WebContents::Getter wc_getter) {
  std::string result;
  base::RunLoop run_loop;
  source.StartDataRequest(url, std::move(wc_getter),
                          base::BindLambdaForTesting(
                              [&](scoped_refptr<base::RefCountedMemory> data) {
                                if (data) {
                                  result.assign(data->begin(), data->end());
                                }
                                run_loop.Quit();
                              }));
  run_loop.Run();
  return result;
}

// Returns the data that `source` serves for `path` on `chrome://theme/`, or an
// empty string if it serves nothing.
std::string GetData(ThemeSource& source,
                    std::string_view path,
                    content::WebContents::Getter wc_getter) {
  return GetDataForUrl(
      source,
      GURL(base::StrCat({content::kChromeUIScheme, "://",
                         chrome::kChromeUIThemeHost, "/", path})),
      std::move(wc_getter));
}

}  // namespace

TEST_F(WebUISourcesTest, GenerateColorsCssInvalidOrMissingSets) {
  ui::ColorProvider color_provider;
  for (bool feature_enabled : {false, true}) {
    SCOPED_TRACE(feature_enabled);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(
        features::kColorIdCssStyleSheetOptimization, feature_enabled);

    EXPECT_EQ(ThemeSource::GenerateColorsCss(color_provider,
                                             GURL("chrome://theme/colors.css")),
              std::nullopt);
    EXPECT_EQ(
        ThemeSource::GenerateColorsCss(
            color_provider, GURL("chrome://theme/colors.css?shadow_host=true")),
        std::nullopt);
    EXPECT_EQ(
        ThemeSource::GenerateColorsCss(
            color_provider, GURL("chrome://theme/colors.css?sets=invalid")),
        std::nullopt);
    EXPECT_EQ(
        ThemeSource::GenerateColorsCss(
            color_provider, GURL("chrome://theme/colors.css?sets=ui,invalid")),
        std::nullopt);
    EXPECT_EQ(
        ThemeSource::GenerateColorsCss(
            color_provider, GURL("devtools://theme/colors.css?sets=invalid")),
        std::nullopt);
  }
}

TEST_F(WebUISourcesTest, GenerateColorsCssSelectorsRgbVarsAndTokens) {
  ui::ColorProvider color_provider;
  color_provider.SetColorForTesting(ui::kColorAccent,
                                    SkColorSetARGB(0x80, 0x12, 0x34, 0x56));

  for (bool feature_enabled : {false, true}) {
    SCOPED_TRACE(feature_enabled);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(
        features::kColorIdCssStyleSheetOptimization, feature_enabled);

    std::optional<std::string> ui_css = ThemeSource::GenerateColorsCss(
        color_provider, GURL("chrome://theme/colors.css?sets=ui"));
    ASSERT_TRUE(ui_css.has_value());
    EXPECT_THAT(
        *ui_css,
        AllOf(StartsWith("html:not(#z){"), EndsWith("}"),
              HasSubstr("--color-accent:#12345680;"),
              Not(HasSubstr("--color-app-menu-highlight-severity-low")),
              Not(HasSubstr("-rgb:")), Not(HasSubstr("--user-color-source")),
              Not(HasSubstr("baseline-grayscale")),
              Not(HasSubstr("baseline-default"))));

    std::optional<std::string> shadow_css = ThemeSource::GenerateColorsCss(
        color_provider,
        GURL("chrome://theme/colors.css?sets=ui,chrome&shadow_host=true"));
    ASSERT_TRUE(shadow_css.has_value());
    EXPECT_THAT(
        *shadow_css,
        AllOf(StartsWith(":host{"), EndsWith("}"),
              Not(HasSubstr("html:not(#z)")),
              HasSubstr("--color-accent:#12345680;"),
              HasSubstr("--color-app-menu-highlight-severity-low:"),
              Not(HasSubstr("-rgb:")), Not(HasSubstr("--user-color-source")),
              Not(HasSubstr("baseline-grayscale")),
              Not(HasSubstr("baseline-default"))));

    std::optional<std::string> non_shadow_css = ThemeSource::GenerateColorsCss(
        color_provider,
        GURL("chrome://theme/colors.css?sets=ui&shadow_host=false"));
    ASSERT_TRUE(non_shadow_css.has_value());
    EXPECT_THAT(*non_shadow_css,
                AllOf(StartsWith("html:not(#z){"), EndsWith("}")));

    std::optional<std::string> rgb_css = ThemeSource::GenerateColorsCss(
        color_provider,
        GURL("chrome://theme/colors.css?sets=ui&generate_rgb_vars=true"));
    ASSERT_TRUE(rgb_css.has_value());
    EXPECT_THAT(*rgb_css, AllOf(StartsWith("html:not(#z){"), EndsWith("}"),
                                HasSubstr("--color-accent:#12345680;"),
                                HasSubstr("--color-accent-rgb:18,52,86;"),
                                Not(HasSubstr("--user-color-source")),
                                Not(HasSubstr("baseline-grayscale")),
                                Not(HasSubstr("baseline-default"))));

    std::optional<std::string> no_rgb_css = ThemeSource::GenerateColorsCss(
        color_provider,
        GURL("chrome://theme/colors.css?sets=ui&generate_rgb_vars=false"));
    ASSERT_TRUE(no_rgb_css.has_value());
    EXPECT_THAT(*no_rgb_css, Not(HasSubstr("-rgb:")));
  }
}

TEST_F(WebUISourcesTest, ThemeSourceServesColorsCssAcrossSchemesAndThemes) {
  auto expect_valid_colors_css = [&]() {
    for (bool feature_enabled : {false, true}) {
      SCOPED_TRACE(feature_enabled);
      base::test::ScopedFeatureList feature_list;
      feature_list.InitWithFeatureState(
          features::kColorIdCssStyleSheetOptimization, feature_enabled);

      for (const char* url_str :
           {"chrome://theme/colors.css?sets=ui,chrome",
            "devtools://theme/colors.css?sets=ui,chrome"}) {
        SCOPED_TRACE(url_str);
        EXPECT_THAT(GetDataForUrl(*theme_source(), GURL(url_str),
                                  web_contents_getter()),
                    AllOf(StartsWith("html:not(#z){"), EndsWith("}"),
                          HasSubstr("--color-accent:"),
                          HasSubstr("--color-app-menu-highlight-severity-low:"),
                          Not(HasSubstr("--user-color-source")),
                          Not(HasSubstr("baseline-grayscale")),
                          Not(HasSubstr("baseline-default"))));
      }
    }
  };

  // Default baseline theme.
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(theme_service);
  ASSERT_TRUE(theme_service->GetIsBaseline());
  ASSERT_FALSE(theme_service->GetIsGrayscale());
  expect_valid_colors_css();

  // Grayscale theme.
  theme_service->SetIsGrayscale(true);
  ASSERT_TRUE(theme_service->GetIsGrayscale());
  expect_valid_colors_css();

  // Custom user color theme (neither baseline nor grayscale).
  theme_service->SetUserColor(SK_ColorBLUE);
  ASSERT_FALSE(theme_service->GetIsBaseline());
  ASSERT_FALSE(theme_service->GetIsGrayscale());
  expect_valid_colors_css();

  // Requests with no WebContents or unrecognized sets return empty data.
  auto no_contents_getter =
      base::BindRepeating([]() -> content::WebContents* { return nullptr; });
  EXPECT_THAT(GetDataForUrl(*theme_source(),
                            GURL("chrome://theme/colors.css?sets=ui,chrome"),
                            no_contents_getter),
              IsEmpty());
  EXPECT_THAT(GetDataForUrl(*theme_source(),
                            GURL("chrome://theme/colors.css?sets=invalid"),
                            web_contents_getter()),
              IsEmpty());
}

// A source created for TopChromeWebUIProfile serves colors.css using the
// requesting WebContents's ColorProvider even without a user profile set on the
// WebContents (as well as when a user profile is set), and never emits
// --user-color-source.
TEST_F(WebUISourcesTest,
       ThemeSourceInTopChromeWebUIProfileServesColorsCssWithoutUserProfile) {
  webui::ScopedTopChromeWebUIProfileForTesting scoped_top_chrome_webui_profile;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          webui::GetTopChromeWebUIProfile(), /*instance=*/nullptr);
  ASSERT_EQ(webui::GetUserProfile(web_contents.get()), nullptr);

  ThemeSource source(webui::GetTopChromeWebUIProfile());
  auto contents_getter =
      base::BindLambdaForTesting([&] { return web_contents.get(); });

  auto expect_valid_top_chrome_colors_css = [&]() {
    for (bool feature_enabled : {false, true}) {
      SCOPED_TRACE(feature_enabled);
      base::test::ScopedFeatureList feature_list;
      feature_list.InitWithFeatureState(
          features::kColorIdCssStyleSheetOptimization, feature_enabled);

      for (const char* url_str :
           {"chrome://theme/colors.css?sets=ui,chrome",
            "devtools://theme/colors.css?sets=ui,chrome"}) {
        SCOPED_TRACE(url_str);
        EXPECT_THAT(GetDataForUrl(source, GURL(url_str), contents_getter),
                    AllOf(StartsWith("html:not(#z){"), EndsWith("}"),
                          HasSubstr("--color-accent:"),
                          HasSubstr("--color-app-menu-highlight-severity-low:"),
                          Not(HasSubstr("--user-color-source")),
                          Not(HasSubstr("baseline-grayscale")),
                          Not(HasSubstr("baseline-default"))));
      }
    }
  };

  // Serves colors.css even before webui::SetUserProfile() is called.
  expect_valid_top_chrome_colors_css();

  // Also serves colors.css without --user-color-source when a grayscale user
  // profile is associated with the WebContents.
  webui::SetUserProfile(web_contents.get(), profile());
  ThemeServiceFactory::GetForProfile(profile())->SetIsGrayscale(true);
  expect_valid_top_chrome_colors_css();
}

// A source created for TopChromeWebUIProfile serves the theme images and the
// NTP CSS with the user profile of the requesting contents. Without contents,
// it serves the default theme images, and no NTP CSS.
TEST_F(WebUISourcesTest, ThemeSourceInTopChromeWebUIProfileImagesAndNewTabCSS) {
  webui::ScopedTopChromeWebUIProfileForTesting scoped_top_chrome_webui_profile;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          webui::GetTopChromeWebUIProfile(), /*instance=*/nullptr);
  webui::SetUserProfile(web_contents.get(), profile());
  ThemeSource source(webui::GetTopChromeWebUIProfile());
  auto contents_getter =
      base::BindLambdaForTesting([&] { return web_contents.get(); });
  auto no_contents_getter =
      base::BindRepeating([]() -> content::WebContents* { return nullptr; });

  ThemeServiceFactory::GetForProfile(profile())
      ->BuildAutogeneratedThemeFromColor(SK_ColorRED);
  EXPECT_FALSE(GetData(source, "IDR_THEME_TOOLBAR", contents_getter).empty());
  EXPECT_THAT(GetData(source, "css/new_tab_theme.css", contents_getter),
              testing::HasSubstr(ThemeService::kAutogeneratedThemeID));

  EXPECT_FALSE(
      GetData(source, "IDR_THEME_TOOLBAR", no_contents_getter).empty());
  EXPECT_TRUE(
      GetData(source, "css/new_tab_theme.css", no_contents_getter).empty());
}

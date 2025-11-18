// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_types.mojom-data-view.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-data-view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-data-view.h"
#include "third_party/liburlpattern/part.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

// The paths to the two pages in the test IWA. See `CreateBorderlessIwaBuilder`.
constexpr std::string_view kStandalonePagePath = "/standalone-page.html";
constexpr std::string_view kBorderlessPagePath = "/borderless-page.html";

// The text displayed in the test IWA `message` element to report when it is
// in borderless display mode. See `CreateBorderlessIwaBuilder` and
// `ReadAppMessage`.
constexpr std::string_view kNotBorderlessMessage =
    "This window is not borderless.";
constexpr std::string_view kBorderlessMessage = "This window is borderless.";

// Returns a `SafeUrlPattern` that matches the given `pathname`.
blink::SafeUrlPattern UrlPatternForPath(std::string_view pathname) {
  liburlpattern::Part part;
  part.type = liburlpattern::PartType::kFixed;
  part.value = pathname;
  blink::SafeUrlPattern pattern;
  pattern.pathname = {part};
  return pattern;
}

IsolatedWebAppBuilder CreateBorderlessIwaBuilder() {
  constexpr std::string_view kPageHtml = R"(
    <!doctype html>
    <html>
      <head>
        <link rel="stylesheet" href="/styles.css" />
      </head>
      <body>
        <p id="message">
          This window is <span class="hide-in-borderless">not</span> borderless.
        </p>
      </body>
    </html>
    )";
  return IsolatedWebAppBuilder(
             ManifestBuilder()
                 .SetDisplayMode(blink::mojom::DisplayMode::kStandalone)
                 .SetDisplayModeOverride(
                     {blink::mojom::DisplayMode::kBorderless})
                 .AddBorderlessUrlPattern(
                     UrlPatternForPath(kBorderlessPagePath))
                 .AddPermissionsPolicy(
                     network::mojom::PermissionsPolicyFeature::
                         kWindowManagement,
                     /*self=*/true, /*origins=*/{}))
      .AddResource("/styles.css", R"(
          .hide-in-borderless {
            display: initial;
          }
          @media (display-mode: borderless) {
            .hide-in-borderless {
              display: none;
            }
          }
          )",
                   "text/css")
      .AddHtml(kStandalonePagePath, kPageHtml)
      .AddHtml(kBorderlessPagePath, kPageHtml);
}

content::WebContents& WebContentsOf(Browser& browser) {
  return CHECK_DEREF(browser.tab_strip_model()->GetActiveWebContents());
}

blink::mojom::DisplayMode DisplayModeOf(Browser& browser) {
  auto& web_contents = WebContentsOf(browser);
  return web_contents.GetDelegate()->GetDisplayMode(&web_contents);
}

// Reads the inner text of the "message" element. See
// `CreateBorderlessIwaBuilder`.
content::EvalJsResult ReadAppMessage(Browser& browser) {
  return content::EvalJs(
      &WebContentsOf(browser),
      "document.getElementById('message').innerText.trim();");
}

// Sets the `content_type` setting for `origin_url` to the given `setting`.
void SetContentSetting(
    Profile* profile,
    const GURL& origin_url,
    content_settings::mojom::ContentSettingsType content_type,
    ContentSetting setting) {
  CHECK_DEREF(HostContentSettingsMapFactory::GetForProfile(profile))
      .SetContentSettingDefaultScope(/*primary_url=*/origin_url,
                                     /*secondary_url=*/origin_url, content_type,
                                     setting);
}

}  // namespace

class BorderlessBrowserTest : public WebAppBrowserTestBase {
 public:
  BorderlessBrowserTest()
      : WebAppBrowserTestBase(
            {features::kIsolatedWebApps, blink::features::kWebAppBorderless},
            {}) {}

 protected:
  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    app_url_info_ =
        CreateBorderlessIwaBuilder().BuildBundle()->InstallChecked(profile());
  }

  Browser& LaunchAppInPathAndWait(std::string_view url_path) {
    Browser& browser =
        CHECK_DEREF(LaunchWebAppToURL(profile(), app_url_info_->app_id(),
                                      app_origin_url().Resolve(url_path)));
    CHECK(content::WaitForLoadStop(&WebContentsOf(browser)));
    return browser;
  }

  bool NavigateToAppPathAndWait(Browser& browser, std::string_view url_path) {
    return ui_test_utils::NavigateToURL(&browser,
                                        app_origin_url().Resolve(url_path)) &&
           content::WaitForLoadStop(&WebContentsOf(browser));
  }

  void SetAppWindowManagementPermission(ContentSetting setting) {
    SetContentSetting(profile(), app_origin_url(),
                      ContentSettingsType::WINDOW_MANAGEMENT, setting);
  }

 private:
  GURL app_origin_url() const { return app_url_info_->origin().GetURL(); }

  // Not null after the IWA is installed in `SetUpOnMainThread`.
  std::optional<IsolatedWebAppUrlInfo> app_url_info_;
};

IN_PROC_BROWSER_TEST_F(BorderlessBrowserTest, AppCanCreateStandaloneWindow) {
  SetAppWindowManagementPermission(CONTENT_SETTING_ALLOW);

  Browser& browser = LaunchAppInPathAndWait(kStandalonePagePath);
  EXPECT_EQ(DisplayModeOf(browser), blink::mojom::DisplayMode::kStandalone);
  EXPECT_EQ(ReadAppMessage(browser), kNotBorderlessMessage);
}

IN_PROC_BROWSER_TEST_F(BorderlessBrowserTest, AppCanCreateBorderlessWindow) {
  SetAppWindowManagementPermission(CONTENT_SETTING_ALLOW);

  Browser& browser = LaunchAppInPathAndWait(kBorderlessPagePath);
  EXPECT_EQ(DisplayModeOf(browser), blink::mojom::DisplayMode::kBorderless);
  EXPECT_EQ(ReadAppMessage(browser), kBorderlessMessage);
}

IN_PROC_BROWSER_TEST_F(BorderlessBrowserTest,
                       DisplayModeDoesNotChangeOnNavigation) {
  SetAppWindowManagementPermission(CONTENT_SETTING_ALLOW);

  {
    Browser& browser = LaunchAppInPathAndWait(kStandalonePagePath);
    EXPECT_EQ(DisplayModeOf(browser), blink::mojom::DisplayMode::kStandalone);

    EXPECT_TRUE(NavigateToAppPathAndWait(browser, kBorderlessPagePath));
    EXPECT_EQ(DisplayModeOf(browser), blink::mojom::DisplayMode::kStandalone);
  }

  {
    Browser& browser = LaunchAppInPathAndWait(kBorderlessPagePath);
    EXPECT_EQ(DisplayModeOf(browser), blink::mojom::DisplayMode::kBorderless);

    EXPECT_TRUE(NavigateToAppPathAndWait(browser, kStandalonePagePath));
    EXPECT_EQ(DisplayModeOf(browser), blink::mojom::DisplayMode::kBorderless);
  }
}

IN_PROC_BROWSER_TEST_F(BorderlessBrowserTest,
                       AppCannotCreateBorderlessWindowWithoutPermission) {
  SetAppWindowManagementPermission(CONTENT_SETTING_BLOCK);

  Browser& browser = LaunchAppInPathAndWait(kBorderlessPagePath);
  EXPECT_EQ(DisplayModeOf(browser), blink::mojom::DisplayMode::kStandalone);
  EXPECT_EQ(ReadAppMessage(browser), kNotBorderlessMessage);
}

}  // namespace web_app

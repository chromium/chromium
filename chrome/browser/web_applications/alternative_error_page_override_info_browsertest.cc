// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_icon_waiter.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/url_formatter/url_formatter.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "skia/ext/skia_utils_base.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"

// Class to test browser error page display info.
class AlternativeErrorPageOverrideInfoBrowserTest
    : public web_app::WebAppBrowserTestBase {
 public:
  // Helper function to prepare PWA and retrieve information from the
  // alternative error page function.
  content::mojom::AlternativeErrorPageOverrideInfoPtr GetErrorPageInfo(
      std::string_view html) {
    ChromeContentBrowserClient browser_client;
    content::ScopedContentBrowserClientSetting setting(&browser_client);

    const GURL app_url = embedded_test_server()->GetURL(html);
    web_app::NavigateViaLinkClickToURLAndWait(browser(), app_url);
    web_app::test::InstallPwaForCurrentUrl(browser());
    content::BrowserContext* context = browser()->profile();

    return browser_client.GetAlternativeErrorPageOverrideInfo(
        app_url, /*render_frame_host=*/nullptr, context,
        net::ERR_INTERNET_DISCONNECTED);
  }

 protected:
  const std::string yellow_ = skia::SkColorToHexString(SK_ColorYELLOW);
  const std::string green_ = skia::SkColorToHexString(SK_ColorGREEN);
  const std::string red_ = skia::SkColorToHexString(SK_ColorRED);
  const std::string blue_ = skia::SkColorToHexString(SK_ColorBLUE);
  const std::string black_ = skia::SkColorToHexString(SK_ColorBLACK);
  const std::string white_ = skia::SkColorToHexString(SK_ColorWHITE);

 private:
  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    WebAppBrowserTestBase::TearDownOnMainThread();
  }
};

// Testing url outside the scope of an installed app.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       NoManifest) {
  ChromeContentBrowserClient browser_client;
  content::ScopedContentBrowserClientSetting setting(&browser_client);

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/simple.html");
  content::BrowserContext* context = browser()->profile();

  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      browser_client.GetAlternativeErrorPageOverrideInfo(
          app_url, /*render_frame_host=*/nullptr, context,
          net::ERR_INTERNET_DISCONNECTED);

  // Expect mojom struct to be null.
  EXPECT_FALSE(info);
}

// Testing manifest with app short name.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithAppShortName) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info = GetErrorPageInfo(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_short_name_only.json");

  // Expect mojom struct with custom app short name.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.Find("app_short_name"),
            "Manifest");
}

// Testing app manifest with no app short name.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithNoAppShortName) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info = GetErrorPageInfo(
      "/banners/"
      "manifest_test_page.html?manifest=manifest.json");

  // Expect mojom struct with customized with app name.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.Find("app_short_name"),
            "Manifest test app");
}

// Testing app manifest with no app short name or app name.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithNoAppShortNameOrAppName) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info = GetErrorPageInfo(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_empty_name_short_name.json");

  // Expect mojom struct customized with HTML page title.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.Find("app_short_name"),
            "Web app banner test page");
}

// Testing app manifest with no app short name or app name, and HTML page
// has no title
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithNoAppShortNameOrAppNameOrTitle) {
  ChromeContentBrowserClient browser_client;
  content::ScopedContentBrowserClientSetting setting(&browser_client);

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/title1.html");
  web_app::NavigateViaLinkClickToURLAndWait(browser(), app_url);
  web_app::test::InstallPwaForCurrentUrl(browser());
  content::BrowserContext* context = browser()->profile();

  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      browser_client.GetAlternativeErrorPageOverrideInfo(
          app_url, /*render_frame_host=*/nullptr, context,
          net::ERR_INTERNET_DISCONNECTED);

  // Expect mojom struct customized with HTML page title.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.Find("app_short_name"),
            base::UTF16ToUTF8(url_formatter::FormatUrl(app_url)));
}

// Testing manifest with icon.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithIcon) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ChromeContentBrowserClient browser_client;
  content::ScopedContentBrowserClientSetting setting(&browser_client);
  const GURL app_url = embedded_test_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_one_icon.json");
  web_app::NavigateViaLinkClickToURLAndWait(browser(), app_url);
  web_app::test::InstallPwaForCurrentUrl(browser());
  Profile* profile = browser()->profile();
  web_app::WebAppProvider* web_app_provider =
      web_app::WebAppProvider::GetForTest(profile);
  const std::optional<webapps::AppId> app_id =
      web_app_provider->registrar_unsafe().FindAppWithUrlInScope(app_url);
  WebAppIconWaiter(profile, app_id.value()).Wait();
  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      browser_client.GetAlternativeErrorPageOverrideInfo(
          app_url, /*render_frame_host=*/nullptr, profile,
          net::ERR_INTERNET_DISCONNECTED);

  // Expect mojom struct with everything (except the icon) filled out.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.Find("app_short_name"),
            "Manifest test app");
  // This test may at first glance seem like an end-to-end test of the default
  // offline experience, but should be considered more of a unit test for just
  // the initial values provided to the default offline page. For a proper
  // end-to-end test, see WebAppOfflinePageIconShowing in the
  // WebAppOfflinePageTest suite.
  EXPECT_EQ(
      *info->alternative_error_page_params.Find("icon_url"),
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAACk"
      "lEQVR42mMAAQAABQABoIJXOQAAAABJRU5ErkJggg==");
  EXPECT_EQ(
      *info->alternative_error_page_params.Find("web_app_error_page_message"),
      "You're offline");
  EXPECT_EQ(*info->alternative_error_page_params.Find("supplementary_icon"),
            "offlineIcon");
}

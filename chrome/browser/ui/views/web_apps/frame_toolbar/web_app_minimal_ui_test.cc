// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/gurl.h"

namespace web_app {

class WebAppMinimalUITest : public WebAppBrowserTestBase {
 public:
  WebAppMinimalUITest() = default;

  WebAppMinimalUITest(const WebAppMinimalUITest&) = delete;
  WebAppMinimalUITest& operator=(const WebAppMinimalUITest&) = delete;

  BrowserView* CreateBrowserView(blink::mojom::DisplayMode display_mode) {
    auto web_app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL("https://example.org"));
    web_app_info->display_mode = display_mode;
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    webapps::AppId app_id = InstallWebApp(std::move(web_app_info));
    Browser* browser = LaunchWebAppBrowser(app_id);
    return BrowserView::GetBrowserViewForBrowser(browser);
  }
};

IN_PROC_BROWSER_TEST_F(WebAppMinimalUITest, Standalone) {
  BrowserView* browser_view =
      CreateBrowserView(blink::mojom::DisplayMode::kStandalone);
  ToolbarButtonProvider* provider = browser_view->toolbar_button_provider();
  EXPECT_FALSE(provider->GetBackButton());
  EXPECT_FALSE(provider->GetReloadButton());
}

IN_PROC_BROWSER_TEST_F(WebAppMinimalUITest, MinimalUi) {
  BrowserView* browser_view =
      CreateBrowserView(blink::mojom::DisplayMode::kMinimalUi);
  ToolbarButtonProvider* provider = browser_view->toolbar_button_provider();
  EXPECT_TRUE(provider->GetBackButton());
  EXPECT_TRUE(provider->GetReloadButton());
}

}  // namespace web_app

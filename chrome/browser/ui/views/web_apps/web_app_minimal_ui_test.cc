// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/gurl.h"

namespace web_app {

class WebAppMinimalUITest : public WebAppControllerBrowserTest {
 public:
  WebAppMinimalUITest() {
    scoped_feature_list_.InitWithFeatures({features::kDesktopMinimalUI}, {});
  }

  BrowserView* CreateBrowserView(blink::mojom::DisplayMode display_mode) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = GURL("https://example.org");
    web_app_info->display_mode = display_mode;
    web_app_info->open_as_window = true;
    AppId app_id = InstallWebApp(std::move(web_app_info));
    Browser* browser = LaunchWebAppBrowser(app_id);
    return BrowserView::GetBrowserViewForBrowser(browser);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(WebAppMinimalUITest);
};

IN_PROC_BROWSER_TEST_P(WebAppMinimalUITest, Standalone) {
  BrowserView* browser_view =
      CreateBrowserView(blink::mojom::DisplayMode::kStandalone);
  ToolbarButtonProvider* provider = browser_view->toolbar_button_provider();
  EXPECT_FALSE(!!provider->GetBackButton());
  EXPECT_FALSE(!!provider->GetReloadButton());
}

IN_PROC_BROWSER_TEST_P(WebAppMinimalUITest, MinimalUi) {
  BrowserView* browser_view =
      CreateBrowserView(blink::mojom::DisplayMode::kMinimalUi);
  ToolbarButtonProvider* provider = browser_view->toolbar_button_provider();
  EXPECT_TRUE(!!provider->GetBackButton());
  EXPECT_TRUE(!!provider->GetReloadButton());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebAppMinimalUITest,
    ::testing::Values(ControllerType::kHostedAppController,
                      ControllerType::kUnifiedControllerWithBookmarkApp,
                      ControllerType::kUnifiedControllerWithWebApp),
    ControllerTypeParamToString);

}  // namespace web_app

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace web_app {

class WebAppIconManagerBrowserTest : public InProcessBrowserTest {
 public:
  WebAppIconManagerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAsWithoutExtensions}, {});
  }

  ~WebAppIconManagerBrowserTest() override = default;

 protected:
  net::EmbeddedTestServer* https_server() { return &https_server_; }

  // InProcessBrowserTest:
  void SetUp() override {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;

  DISALLOW_COPY_AND_ASSIGN(WebAppIconManagerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebAppIconManagerBrowserTest, SingleIcon) {
  ASSERT_TRUE(https_server()->Start());
  const GURL app_url =
      https_server()->GetURL("/banners/manifest_test_page.html");

  AppId app_id;
  {
    std::unique_ptr<WebApplicationInfo> web_application_info =
        std::make_unique<WebApplicationInfo>();
    web_application_info->app_url = app_url;
    web_application_info->scope = app_url.GetWithoutFilename();
    web_application_info->open_as_window = true;

    {
      WebApplicationIconInfo info;
      info.width = icon_size::k32;
      info.height = icon_size::k32;
      info.data.allocN32Pixels(info.width, info.height, true);
      info.data.eraseColor(SK_ColorBLUE);
      web_application_info->icons.push_back(info);
    }

    InstallManager& install_manager =
        WebAppProviderBase::GetProviderBase(browser()->profile())
            ->install_manager();

    base::RunLoop run_loop;
    install_manager.InstallWebAppFromInfo(
        std::move(web_application_info), ForInstallableSite::kYes,
        WebappInstallSource::OMNIBOX_INSTALL_ICON,
        base::BindLambdaForTesting(
            [&app_id, &run_loop](const AppId& installed_app_id,
                                 InstallResultCode code) {
              EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
              app_id = installed_app_id;
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  WebAppBrowserController* controller;
  {
    apps::AppLaunchParams params(
        app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::NEW_WINDOW,
        apps::mojom::AppLaunchSource::kSourceTest);
    content::WebContents* contents =
        apps::LaunchService::Get(browser()->profile())->OpenApplication(params);
    controller = chrome::FindBrowserWithWebContents(contents)
                     ->app_controller()
                     ->AsWebAppBrowserController();
  }

  base::RunLoop run_loop;
  controller->SetReadIconCallbackForTesting(
      base::BindLambdaForTesting([controller, &run_loop]() {
        const SkBitmap* bitmap = controller->GetWindowAppIcon().bitmap();
        EXPECT_EQ(SK_ColorBLUE, bitmap->getColor(0, 0));
        EXPECT_EQ(32, bitmap->width());
        EXPECT_EQ(32, bitmap->height());
        run_loop.Quit();
      }));

  run_loop.Run();
}

}  // namespace web_app

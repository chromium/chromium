// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
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
#include "content/public/test/browser_test.h"
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

  void SetUpOnMainThread() override {
    app_service_test_.SetUp(browser()->profile());
  }

  // InProcessBrowserTest:
  void SetUp() override {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    InProcessBrowserTest::SetUp();
  }

  apps::AppServiceTest& app_service_test() { return app_service_test_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  apps::AppServiceTest app_service_test_;

  DISALLOW_COPY_AND_ASSIGN(WebAppIconManagerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebAppIconManagerBrowserTest, SingleIcon) {
  ASSERT_TRUE(https_server()->Start());
  const GURL start_url =
      https_server()->GetURL("/banners/manifest_test_page.html");

  AppId app_id;
  {
    std::unique_ptr<WebApplicationInfo> web_application_info =
        std::make_unique<WebApplicationInfo>();
    web_application_info->start_url = start_url;
    web_application_info->scope = start_url.GetWithoutFilename();
    web_application_info->title = base::ASCIIToUTF16("App Name");
    web_application_info->open_as_window = true;

    {
      SkBitmap bitmap;
      bitmap.allocN32Pixels(icon_size::k32, icon_size::k32, true);
      bitmap.eraseColor(SK_ColorBLUE);
      web_application_info->icon_bitmaps_any[icon_size::k32] =
          std::move(bitmap);
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

#if defined(OS_CHROMEOS)
  gfx::ImageSkia image_skia;
  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
    app_service_test().FlushMojoCalls();
    image_skia = app_service_test().LoadAppIconBlocking(
        apps::mojom::AppType::kWeb, app_id, web_app::kWebAppIconSmall);
  }
#endif

  WebAppBrowserController* controller;
  {
    apps::AppLaunchParams params(
        app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::NEW_WINDOW,
        apps::mojom::AppLaunchSource::kSourceTest);
    content::WebContents* contents =
        apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
            ->BrowserAppLauncher()
            ->LaunchAppWithParams(params);
    controller = chrome::FindBrowserWithWebContents(contents)
                     ->app_controller()
                     ->AsWebAppBrowserController();
  }

  base::RunLoop run_loop;

#if defined(OS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
    controller->SetReadIconCallbackForTesting(base::BindLambdaForTesting(
        [controller, &image_skia, &run_loop, this]() {
          EXPECT_TRUE(app_service_test().AreIconImageEqual(
              image_skia, controller->GetWindowAppIcon()));
          run_loop.Quit();
        }));
    run_loop.Run();
    return;
  }
#endif

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

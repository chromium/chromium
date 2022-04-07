// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/app_service/lacros_web_apps_controller.h"
#include "chrome/browser/web_applications/test/app_registration_waiter.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace web_app {

class LacrosWebAppsControllerBrowserTest : public WebAppControllerBrowserTest {
 public:
  LacrosWebAppsControllerBrowserTest() = default;
  ~LacrosWebAppsControllerBrowserTest() override = default;

 protected:
  // If ash is does not contain the relevant test controller functionality,
  // then there's nothing to do for this test.
  bool IsServiceAvailable() {
    DCHECK(IsWebAppsCrosapiEnabled());
    auto* const service = chromeos::LacrosService::Get();
    return service->GetInterfaceVersion(
               crosapi::mojom::TestController::Uuid_) >=
           static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                                kCloseAllBrowserWindowsMinVersion);
  }
};

// Test that the default context menu for a web app has the correct items.
IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, DefaultContextMenu) {
  if (!IsServiceAvailable())
    GTEST_SKIP() << "Unsupported ash version.";

  const AppId app_id =
      InstallPWA(https_server()->GetURL("/web_apps/basic.html"));
  AppRegistrationWaiter(profile(), app_id).Await();

  // No item should exist in the shelf before the web app is launched.
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/false);

  LaunchWebAppBrowser(app_id);

  // Wait for item to exist in shelf.
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/true);

  // Get the context menu.
  crosapi::mojom::TestControllerAsyncWaiter waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::TestController>()
          .get());
  std::vector<std::string> items;
  waiter.GetContextMenuForShelfItem(app_id, &items);
  ASSERT_EQ(5u, items.size());
  EXPECT_EQ(items[0], "New window");
  EXPECT_EQ(items[1], "Pin");
  EXPECT_EQ(items[2], "Close");
  EXPECT_EQ(items[3], "Uninstall");
  EXPECT_EQ(items[4], "App info");

  // Close app window.
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->is_type_app())
      browser->window()->Close();
  }

  // Wait for item to stop existing in shelf.
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/false);
}

// Test that ShowSiteSettings() launches the Settings SWA.
IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, AppManagement) {
  if (!IsServiceAvailable() ||
      chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::AppServiceProxy::Uuid_) <
          static_cast<int>(crosapi::mojom::AppServiceProxy::MethodMinVersions::
                               kShowAppManagementPageMinVersion)) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  const GURL app_url = https_server()->GetURL("/web_apps/basic.html");
  const AppId app_id = InstallPWA(app_url);
  AppRegistrationWaiter(profile(), app_id).Await();
  AppRegistrationWaiter(profile(), kOsSettingsAppId).Await();

  Browser* browser = LaunchWebAppBrowser(app_id);
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ChromePageInfoDelegate delegate(web_contents);

  // Wait for item to exist in shelf.
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/true);

  // Settings should not yet exist in the shelf.
  browser_test_util::WaitForShelfItem(kOsSettingsAppId, /*exists=*/false);

  delegate.ShowSiteSettings(app_url);

  // Settings should now exist in the shelf.
  browser_test_util::WaitForShelfItem(kOsSettingsAppId, /*exists=*/true);

  base::RunLoop run_loop;
  auto* const lacros_service = chromeos::LacrosService::Get();
  lacros_service->GetRemote<crosapi::mojom::TestController>()
      ->CloseAllBrowserWindows(
          base::BindLambdaForTesting([&run_loop](bool success) {
            EXPECT_TRUE(success);
            run_loop.Quit();
          }));
  run_loop.Run();

  // Settings should no longer exist in the shelf.
  browser_test_util::WaitForShelfItem(kOsSettingsAppId, /*exists=*/false);

  // Close app window.
  browser->window()->Close();

  // Wait for item to stop existing in shelf.
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/false);
}

}  // namespace web_app

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/app_service/lacros_web_apps_controller.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace web_app {

using LacrosWebAppsControllerBrowserTest = WebAppNavigationBrowserTest;

// Test that the default context menu for a web app has the correct items.
IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, DefaultContextMenu) {
  InstallTestWebApp();
  const AppId app_id = test_web_app_id();

  // No item should exist in the shelf before the web app is launched.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id, /*exists=*/false));

  OpenTestWebApp();

  // Wait for item to exist in shelf.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id, /*exists=*/true));

  // Get the context menu.
  base::test::TestFuture<const std::vector<std::string>&> items_future;
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::TestController>()
      ->GetContextMenuForShelfItem(app_id, items_future.GetCallback());

  auto items = items_future.Take();
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
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id, /*exists=*/false));
}

// Test that ShowSiteSettings() launches the Settings SWA.
IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, AppManagement) {
  InstallTestWebApp();
  const AppId app_id = test_web_app_id();
  apps::AppReadinessWaiter(profile(), kOsSettingsAppId).Await();

  Browser* browser = OpenTestWebApp();
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ChromePageInfoDelegate delegate(web_contents);

  // Wait for item to exist in shelf.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id, /*exists=*/true));

  // Settings should not yet exist in the shelf.
  ASSERT_TRUE(
      browser_test_util::WaitForShelfItem(kOsSettingsAppId, /*exists=*/false));

  delegate.ShowSiteSettings(web_contents->GetVisibleURL());

  // Settings should now exist in the shelf.
  ASSERT_TRUE(
      browser_test_util::WaitForShelfItem(kOsSettingsAppId, /*exists=*/true));

  base::test::TestFuture<bool> success_future;
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::TestController>()
      ->CloseAllBrowserWindows(success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());

  // Settings should no longer exist in the shelf.
  ASSERT_TRUE(
      browser_test_util::WaitForShelfItem(kOsSettingsAppId, /*exists=*/false));

  // Close app window.
  browser->window()->Close();

  // Wait for item to stop existing in shelf.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id, /*exists=*/false));
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, AppList) {
  // If ash is does not contain the relevant test controller functionality,
  // then there's nothing to do for this test.
  if (chromeos::LacrosService::Get()
          ->GetInterfaceVersion<crosapi::mojom::TestController>() <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kLaunchAppFromAppListMinVersion)) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  InstallTestWebApp();
  const AppId app_id = test_web_app_id();

  // No item should exist in the shelf before the web app is launched.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id, /*exists=*/false));

  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::TestController>()
      ->LaunchAppFromAppList(app_id);

  // Wait for item to exist in shelf.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id, /*exists=*/true));

  web_app::test::UninstallWebApp(profile(), app_id);

  // Wait for item to stop existing in shelf.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id, /*exists=*/false));
}

}  // namespace web_app

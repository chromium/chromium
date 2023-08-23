// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace web_app {

bool SelectContextMenuForShelfItem(const std::string& app_id, uint32_t index) {
  base::test::TestFuture<bool> success_future;
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::TestController>()
      ->SelectContextMenuForShelfItem(app_id, index,
                                      success_future.GetCallback());
  return success_future.Take();
}

std::vector<std::string> GetContextMenuForShelfItem(const std::string& app_id) {
  base::test::TestFuture<const std::vector<std::string>&> items_future;
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::TestController>()
      ->GetContextMenuForShelfItem(app_id, items_future.GetCallback());
  return items_future.Take();
}

using LacrosWebAppBrowserTest = WebAppControllerBrowserTest;

// Test that for a PWA with a file handler, App info from the Shelf context menu
// launches the Settings SWA. Regression test for https://crbug.com/1315958.
IN_PROC_BROWSER_TEST_F(LacrosWebAppBrowserTest, AppInfo) {
  constexpr uint32_t kAppInfoIndex = 4;
  constexpr uint32_t kCloseSettingsIndex = 1;

  const GURL app_url =
      https_server()->GetURL("/web_apps/file_handler_index.html");
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  EXPECT_EQ(provider().registrar_unsafe().GetAppFileHandlers(app_id)->size(),
            1U);

  LaunchWebAppBrowser(app_id);

  // Wait for item to exist in shelf.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id, /*exists=*/true));

  apps::AppReadinessWaiter(profile(), kOsSettingsAppId).Await();

  // Settings should not yet exist in the shelf.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(kOsSettingsAppId,
                                                  /*exists=*/false));

  ASSERT_TRUE(SelectContextMenuForShelfItem(app_id, kAppInfoIndex));

  ASSERT_TRUE(
      browser_test_util::WaitForShelfItem(kOsSettingsAppId, /*exists=*/true));

  {
    // Get the Settings context menu.
    auto items = GetContextMenuForShelfItem(kOsSettingsAppId);
    EXPECT_EQ(2u, items.size());
    EXPECT_EQ(items[0], "Pin");
    EXPECT_EQ(items[1], "Close");
  }

  // Close Settings window.
  ASSERT_TRUE(
      SelectContextMenuForShelfItem(kOsSettingsAppId, kCloseSettingsIndex));

  // Settings should no longer exist in the shelf.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(kOsSettingsAppId,
                                                  /*exists=*/false));

  UninstallWebApp(app_id);

  // Wait for item to stop existing in shelf.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id, /*exists=*/false));
}

// Regression test for crbug.com/1335266
IN_PROC_BROWSER_TEST_F(LacrosWebAppBrowserTest, Shortcut) {
  // The menu contains 5 items common across running web apps, then a separator
  // and label for each of the 6 shortcut entries.
  constexpr uint32_t kNumShortcutItems = 17U;
  constexpr uint32_t kShortcutOneIndex = 6;
  constexpr uint32_t kShortcutThreeIndex = 10;
  constexpr uint32_t kShortcutSixIndex = 16;

  const GURL app_url =
      https_server()->GetURL("/web_app_shortcuts/shortcuts.html");
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppShortcutsMenuItemInfos(app_id).size(),
      6U);

  // Wait for item to exist in shelf.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id, /*exists=*/true));

  {
    // Get the context menu.
    std::vector<std::string> items = GetContextMenuForShelfItem(app_id);
    EXPECT_EQ(kNumShortcutItems, items.size());
    EXPECT_EQ(items[kShortcutOneIndex], "One");
    EXPECT_EQ(items[kShortcutThreeIndex], "Three");
    EXPECT_EQ(items[kShortcutSixIndex], "Six");
  }

  {
    content::TestNavigationObserver navigation_observer(
        https_server()->GetURL("/web_app_shortcuts/shortcuts.html#one"));
    navigation_observer.StartWatchingNewWebContents();
    ASSERT_TRUE(SelectContextMenuForShelfItem(app_id, kShortcutOneIndex));
    navigation_observer.Wait();
  }

  {
    content::TestNavigationObserver navigation_observer(
        https_server()->GetURL("/web_app_shortcuts/shortcuts.html#three"));
    navigation_observer.StartWatchingNewWebContents();
    ASSERT_TRUE(SelectContextMenuForShelfItem(app_id, kShortcutThreeIndex));
    navigation_observer.Wait();
  }

  {
    content::TestNavigationObserver navigation_observer(
        https_server()->GetURL("/web_app_shortcuts/shortcuts.html#six"));
    navigation_observer.StartWatchingNewWebContents();
    ASSERT_TRUE(SelectContextMenuForShelfItem(app_id, kShortcutSixIndex));
    navigation_observer.Wait();
  }
}

}  // namespace web_app

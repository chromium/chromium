// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/version.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/test/app_registry_cache_waiter.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace web_app {

class LacrosWebAppBrowserTest : public WebAppControllerBrowserTest {
 public:
  LacrosWebAppBrowserTest() = default;
  ~LacrosWebAppBrowserTest() override = default;

 protected:
  // If ash is does not contain the relevant test controller functionality, then
  // there's nothing to do for this test. We require https://crrev.com/c/3688993
  // (SelectContextMenuForShelfItem bug fix) and https://crrev.com/c/3703077
  // (ApplyBackgroundAndMask fix for PWA shortcuts without icons).
  bool IsServiceAvailable() {
    DCHECK(IsWebAppsCrosapiEnabled());
    return chromeos::IsAshVersionAtLeastForTesting(
        base::Version({105, 0, 5120}));
  }
};

// Test that for a PWA with a file handler, App info from the Shelf context menu
// launches the Settings SWA. Regression test for https://crbug.com/1315958.
IN_PROC_BROWSER_TEST_F(LacrosWebAppBrowserTest, AppInfo) {
  if (!IsServiceAvailable())
    GTEST_SKIP() << "Unsupported ash version.";

  crosapi::mojom::TestControllerAsyncWaiter waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::TestController>()
          .get());

  auto selectContextMenu = [&waiter](const AppId& app_id, int index) {
    bool success = false;
    waiter.SelectContextMenuForShelfItem(app_id, index, &success);
    return success;
  };

  const uint32_t kAppInfoIndex = 4;
  const uint32_t kCloseSettingsIndex = 1;

  const GURL app_url =
      https_server()->GetURL("/web_apps/file_handler_index.html");
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  EXPECT_EQ(provider().registrar_unsafe().GetAppFileHandlers(app_id)->size(),
            1U);

  LaunchWebAppBrowser(app_id);

  // Wait for item to exist in shelf.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id, /*exists=*/true));

  AppReadinessWaiter(profile(), kOsSettingsAppId).Await();

  // Settings should not yet exist in the shelf.
  ASSERT_TRUE(
      browser_test_util::WaitForShelfItem(kOsSettingsAppId, /*exists=*/false));

  ASSERT_TRUE(selectContextMenu(app_id, kAppInfoIndex));

  ASSERT_TRUE(
      browser_test_util::WaitForShelfItem(kOsSettingsAppId, /*exists=*/true));

  {
    // Get the Settings context menu.
    std::vector<std::string> items;
    waiter.GetContextMenuForShelfItem(kOsSettingsAppId, &items);
    EXPECT_EQ(2u, items.size());
    EXPECT_EQ(items[0], "Pin");
    EXPECT_EQ(items[1], "Close");
  }

  // Close Settings window.
  ASSERT_TRUE(selectContextMenu(kOsSettingsAppId, kCloseSettingsIndex));

  // Settings should no longer exist in the shelf.
  ASSERT_TRUE(
      browser_test_util::WaitForShelfItem(kOsSettingsAppId, /*exists=*/false));

  UninstallWebApp(app_id);

  // Wait for item to stop existing in shelf.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id, /*exists=*/false));
}

// Regression test for crbug.com/1335266
IN_PROC_BROWSER_TEST_F(LacrosWebAppBrowserTest, Shortcut) {
  if (!IsServiceAvailable())
    GTEST_SKIP() << "Unsupported ash version.";

  // The menu contains 5 items common across running web apps, then a separator
  // and label for each of the 6 shortcut entries.
  const uint32_t kNumShortcutItems = 17U;
  const int kShortcutOneIndex = 6;
  const int kShortcutThreeIndex = 10;
  const int kShortcutSixIndex = 16;

  crosapi::mojom::TestControllerAsyncWaiter waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::TestController>()
          .get());

  const GURL app_url =
      https_server()->GetURL("/web_app_shortcuts/shortcuts.html");
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppShortcutsMenuItemInfos(app_id).size(),
      6U);

  // Wait for item to exist in shelf.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id, /*exists=*/true));

  auto selectContextMenu = [&](int index) {
    bool success = false;
    waiter.SelectContextMenuForShelfItem(app_id, index, &success);
    return success;
  };

  {
    // Get the context menu.
    std::vector<std::string> items;
    waiter.GetContextMenuForShelfItem(app_id, &items);
    EXPECT_EQ(kNumShortcutItems, items.size());
    EXPECT_EQ(items[kShortcutOneIndex], "One");
    EXPECT_EQ(items[kShortcutThreeIndex], "Three");
    EXPECT_EQ(items[kShortcutSixIndex], "Six");
  }

  {
    content::TestNavigationObserver navigation_observer(
        https_server()->GetURL("/web_app_shortcuts/shortcuts.html#one"));
    navigation_observer.StartWatchingNewWebContents();
    ASSERT_TRUE(selectContextMenu(kShortcutOneIndex));
    navigation_observer.Wait();
  }

  {
    content::TestNavigationObserver navigation_observer(
        https_server()->GetURL("/web_app_shortcuts/shortcuts.html#three"));
    navigation_observer.StartWatchingNewWebContents();
    ASSERT_TRUE(selectContextMenu(kShortcutThreeIndex));
    navigation_observer.Wait();
  }

  {
    content::TestNavigationObserver navigation_observer(
        https_server()->GetURL("/web_app_shortcuts/shortcuts.html#six"));
    navigation_observer.StartWatchingNewWebContents();
    ASSERT_TRUE(selectContextMenu(kShortcutSixIndex));
    navigation_observer.Wait();
  }
}

}  // namespace web_app

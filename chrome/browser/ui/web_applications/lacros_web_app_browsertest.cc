// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/version.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/test/app_registration_waiter.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace web_app {

class LacrosWebAppBrowserTest : public WebAppControllerBrowserTest {
 public:
  LacrosWebAppBrowserTest() = default;
  ~LacrosWebAppBrowserTest() override = default;

 protected:
  // If ash is does not contain the relevant test controller functionality, then
  // there's nothing to do for this test. We require https://crrev.com/c/3688993
  // or later (SelectContextMenuForShelfItem bug fix).
  bool IsServiceAvailable() {
    DCHECK(IsWebAppsCrosapiEnabled());
    return chromeos::IsAshVersionAtLeastForTesting(
        base::Version({104, 0, 5102}));
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
  EXPECT_EQ(provider().registrar().GetAppFileHandlers(app_id)->size(), 1U);

  LaunchWebAppBrowser(app_id);

  // Wait for item to exist in shelf.
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/true);

  AppRegistrationWaiter(profile(), kOsSettingsAppId).Await();

  // Settings should not yet exist in the shelf.
  browser_test_util::WaitForShelfItem(kOsSettingsAppId, /*exists=*/false);

  ASSERT_TRUE(selectContextMenu(app_id, kAppInfoIndex));

  browser_test_util::WaitForShelfItem(kOsSettingsAppId, /*exists=*/true);

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
  browser_test_util::WaitForShelfItem(kOsSettingsAppId, /*exists=*/false);

  UninstallWebApp(app_id);

  // Wait for item to stop existing in shelf.
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/false);
}

}  // namespace web_app

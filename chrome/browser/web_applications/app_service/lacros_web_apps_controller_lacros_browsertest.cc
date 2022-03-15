// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/app_service/lacros_web_apps_controller.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace web_app {

using LacrosWebAppsControllerBrowserTest = web_app::WebAppControllerBrowserTest;

// Test that the default context menu for a web app has the correct items.
IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, DefaultContextMenu) {
  // If ash is does not contain the relevant test controller functionality, then
  // there's nothing to do for this test.
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::TestController::Uuid_) <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kDoesItemExistInShelfMinVersion)) {
    LOG(WARNING) << "Unsupported ash version.";
    return;
  }

  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(IsWebAppsCrosapiEnabled());
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.Init();

  const AppId app_id =
      InstallPWA(embedded_test_server()->GetURL("/web_apps/basic.html"));

  // No item should exist in the shelf before the web app is launched.
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/false);

  crosapi::mojom::LaunchParamsPtr launch_params =
      crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = apps::mojom::LaunchSource::kFromTest;
  static_cast<crosapi::mojom::AppController&>(lacros_web_apps_controller)
      .Launch(std::move(launch_params), base::DoNothing());

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

}  // namespace web_app

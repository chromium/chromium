// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/test/app_registry_cache_waiter.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/app_constants/constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

using crosapi::mojom::ShelfItemState;

namespace {

constexpr char kFirstAppUrlHost[] = "first-pwa.test";
constexpr char kSecondAppUrlHost[] = "second-pwa.test";

}  // namespace

namespace web_app {

class LacrosWebAppShelfBrowserTest : public WebAppNavigationBrowserTest {
 public:
  LacrosWebAppShelfBrowserTest() = default;
  ~LacrosWebAppShelfBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppNavigationBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(https_server().Start());
  }

 protected:
  // If ash is does not contain the relevant test controller functionality, then
  // there's nothing to do for this test.
  bool IsServiceAvailable() {
    DCHECK(IsWebAppsCrosapiEnabled());
    if (chromeos::LacrosService::Get()->GetInterfaceVersion(
            crosapi::mojom::TestController::Uuid_) <
        static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                             kGetShelfItemStateMinVersion)) {
      LOG(WARNING) << "Unsupported ash version.";
      return false;
    }
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(LacrosWebAppShelfBrowserTest, Activation) {
  if (!IsServiceAvailable())
    GTEST_SKIP();

  const GURL app1_url =
      https_server().GetURL(kFirstAppUrlHost, "/web_apps/basic.html");
  const AppId app1_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app1_url);

  const GURL app2_url = https_server().GetURL(
      kSecondAppUrlHost, "/web_apps/standalone/basic.html");
  const AppId app2_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app2_url);

  AppReadinessWaiter(profile(), app1_id).Await();
  Browser* app_browser1 = LaunchWebAppBrowser(profile(), app1_id);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser1, app1_id));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app1_id, static_cast<uint32_t>(ShelfItemState::kActive)));

  AppReadinessWaiter(profile(), app2_id).Await();
  LaunchWebAppBrowser(profile(), app2_id);
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app2_id, static_cast<uint32_t>(ShelfItemState::kActive)));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app1_id, static_cast<uint32_t>(ShelfItemState::kRunning)));

  CloseAndWait(app_browser1);
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app1_id, static_cast<uint32_t>(ShelfItemState::kNormal)));

  test::UninstallWebApp(profile(), app2_id);
  AppReadinessWaiter(profile(), app2_id, apps::Readiness::kUninstalledByUser)
      .Await();
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app2_id, static_cast<uint32_t>(ShelfItemState::kNormal)));

  test::UninstallWebApp(profile(), app1_id);
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppShelfBrowserTest, BadgeShown) {
  if (!IsServiceAvailable())
    GTEST_SKIP();

  const GURL app_url = https_server().GetURL(kFirstAppUrlHost,
                                             "/web_apps/minimal_ui/basic.html");
  const AppId app_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app_url);

  AppReadinessWaiter(profile(), app_id).Await();
  Browser* app_browser = LaunchWebAppBrowser(profile(), app_id);
  content::WebContents* const web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app_id, static_cast<uint32_t>(ShelfItemState::kActive)));

  ASSERT_TRUE(content::ExecuteScript(web_contents, "navigator.setAppBadge();"));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app_id, static_cast<uint32_t>(ShelfItemState::kActive) |
                  static_cast<uint32_t>(ShelfItemState::kNotification)));

  ASSERT_TRUE(
      content::ExecuteScript(web_contents, "navigator.clearAppBadge();"));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app_id, static_cast<uint32_t>(ShelfItemState::kActive)));

  test::UninstallWebApp(profile(), app_id);
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app_id, static_cast<uint32_t>(ShelfItemState::kNormal)));
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppShelfBrowserTest, RunningInTab) {
  if (!IsServiceAvailable())
    GTEST_SKIP();

  crosapi::mojom::TestController* const test_controller =
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::TestController>()
          .get();
  crosapi::mojom::TestControllerAsyncWaiter waiter(test_controller);
  const GURL app1_url = https_server().GetURL(
      kFirstAppUrlHost, "/web_apps/standalone/basic.html");
  const AppId app1_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app1_url);

  const GURL app2_url =
      https_server().GetURL(kSecondAppUrlHost, "/web_apps/basic.html");
  const AppId app2_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app2_url);

  {
    auto& sync_bridge = WebAppProvider::GetForTest(profile())->sync_bridge();

    Browser* app_browser1 = LaunchWebAppBrowser(profile(), app1_id);
    ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
        app1_id, static_cast<uint32_t>(ShelfItemState::kActive)));
    waiter.PinOrUnpinItemInShelf(app1_id, /*pin=*/true);
    CloseAndWait(app_browser1);
    sync_bridge.SetAppUserDisplayMode(app1_id, UserDisplayMode::kBrowser,
                                      /*is_user_action=*/true);
    AppWindowModeWaiter(profile(), app1_id, apps::WindowMode::kBrowser).Await();

    Browser* app_browser2 = LaunchWebAppBrowser(profile(), app2_id);
    ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
        app2_id, static_cast<uint32_t>(ShelfItemState::kActive)));
    waiter.PinOrUnpinItemInShelf(app2_id, /*pin=*/true);
    CloseAndWait(app_browser2);
    sync_bridge.SetAppUserDisplayMode(app2_id, UserDisplayMode::kBrowser,
                                      /*is_user_action=*/true);
    AppWindowModeWaiter(profile(), app2_id, apps::WindowMode::kBrowser).Await();
  }

  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app_constants::kLacrosAppId,
      static_cast<uint32_t>(ShelfItemState::kActive)));

  test_controller->LaunchAppFromAppList(app1_id);
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app1_id, static_cast<uint32_t>(ShelfItemState::kActive)));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app_constants::kLacrosAppId,
      static_cast<uint32_t>(ShelfItemState::kRunning)));

  test_controller->LaunchAppFromAppList(app2_id);
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app2_id, static_cast<uint32_t>(ShelfItemState::kActive)));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app1_id, static_cast<uint32_t>(ShelfItemState::kRunning)));

  EXPECT_EQ(BrowserList::GetInstance()->size(), 1U);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(),
                                      TabCloseTypes::CLOSE_NONE);
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app2_id, static_cast<uint32_t>(ShelfItemState::kNormal)));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app1_id, static_cast<uint32_t>(ShelfItemState::kActive)));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app_constants::kLacrosAppId,
      static_cast<uint32_t>(ShelfItemState::kRunning)));

  test::UninstallWebApp(profile(), app1_id);
  test::UninstallWebApp(profile(), app2_id);
}

}  // namespace web_app

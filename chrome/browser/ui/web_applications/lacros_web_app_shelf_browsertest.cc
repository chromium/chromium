// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/app_constants/constants.h"
#include "components/webapps/browser/test/service_worker_registration_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/page_transition_types.h"
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
    if (chromeos::LacrosService::Get()
            ->GetInterfaceVersion<crosapi::mojom::TestController>() <
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
  const webapps::AppId app1_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app1_url);

  const GURL app2_url = https_server().GetURL(
      kSecondAppUrlHost, "/web_apps/standalone/basic.html");
  const webapps::AppId app2_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app2_url);

  apps::AppReadinessWaiter(profile(), app1_id).Await();
  Browser* app_browser1 = LaunchWebAppBrowser(app1_id);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser1, app1_id));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app1_id, static_cast<uint32_t>(ShelfItemState::kActive)));

  ASSERT_TRUE(AddTabAtIndex(/*index=*/1, app1_url, ui::PAGE_TRANSITION_TYPED));

  apps::AppReadinessWaiter(profile(), app2_id).Await();
  LaunchWebAppBrowser(app2_id);
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app2_id, static_cast<uint32_t>(ShelfItemState::kActive)));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app1_id, static_cast<uint32_t>(ShelfItemState::kRunning)));

  CloseAndWait(app_browser1);
  // A tab open at app1_url is not sufficient for the app to be considered
  // running.
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app1_id, static_cast<uint32_t>(ShelfItemState::kNormal)));

  test::UninstallWebApp(profile(), app2_id);
  apps::AppReadinessWaiter(profile(), app2_id,
                           apps::Readiness::kUninstalledByUser)
      .Await();
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app2_id, static_cast<uint32_t>(ShelfItemState::kNormal)));

  test::UninstallWebApp(profile(), app1_id);
}

// Navigating out of scope in an app window does not affect which app is
// considered running.
IN_PROC_BROWSER_TEST_F(LacrosWebAppShelfBrowserTest, Navigation) {
  if (!IsServiceAvailable())
    GTEST_SKIP();

  const GURL app1_url =
      https_server().GetURL(kFirstAppUrlHost, "/web_apps/basic.html");
  const webapps::AppId app1_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app1_url);

  const GURL app2_url = https_server().GetURL(
      kSecondAppUrlHost, "/web_app_shortcuts/shortcuts.html");
  const webapps::AppId app2_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app2_url);

  GURL out_of_scope_url = https_server().GetURL("/empty.html");

  Browser* app_browser1 = LaunchWebAppBrowser(app1_id);
  {
    NavigateParams params(app_browser1, out_of_scope_url,
                          ui::PAGE_TRANSITION_LINK);
    params.tabstrip_index = app_browser1->tab_strip_model()->active_index();
    params.disposition = WindowOpenDisposition::CURRENT_TAB;
    Navigate(&params);
    ASSERT_TRUE(
        content::WaitForLoadStop(params.navigated_or_inserted_contents));
    EXPECT_EQ(app_browser1->tab_strip_model()->count(), 1);
    EXPECT_EQ(app_browser1->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetLastCommittedURL(),
              out_of_scope_url);
  }
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app1_id, static_cast<uint32_t>(ShelfItemState::kActive)));

  LaunchWebAppBrowser(app2_id);
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app2_id, static_cast<uint32_t>(ShelfItemState::kActive)));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app1_id, static_cast<uint32_t>(ShelfItemState::kRunning)));

  app_browser1->window()->Activate();
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app1_id, static_cast<uint32_t>(ShelfItemState::kActive)));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app2_id, static_cast<uint32_t>(ShelfItemState::kRunning)));

  test::UninstallWebApp(profile(), app1_id);
  test::UninstallWebApp(profile(), app2_id);
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppShelfBrowserTest, BadgeShown) {
  if (!IsServiceAvailable())
    GTEST_SKIP();

  const GURL app_url = https_server().GetURL(kFirstAppUrlHost,
                                             "/web_apps/minimal_ui/basic.html");
  const webapps::AppId app_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app_url);

  apps::AppReadinessWaiter(profile(), app_id).Await();
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  content::WebContents* const web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app_id, static_cast<uint32_t>(ShelfItemState::kActive)));

  ASSERT_TRUE(content::ExecJs(web_contents, "navigator.setAppBadge();",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app_id, static_cast<uint32_t>(ShelfItemState::kActive) |
                  static_cast<uint32_t>(ShelfItemState::kNotification)));

  ASSERT_TRUE(content::ExecJs(web_contents, "navigator.clearAppBadge();",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app_id, static_cast<uint32_t>(ShelfItemState::kActive)));

  test::UninstallWebApp(profile(), app_id);
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app_id, static_cast<uint32_t>(ShelfItemState::kNormal)));
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppShelfBrowserTest, RunningInTab) {
  if (!IsServiceAvailable())
    GTEST_SKIP();

  auto& test_controller = chromeos::LacrosService::Get()
                              ->GetRemote<crosapi::mojom::TestController>();
  const GURL app1_url = https_server().GetURL(
      kFirstAppUrlHost, "/web_apps/standalone/basic.html");
  const webapps::AppId app1_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app1_url);

  const GURL app2_url =
      https_server().GetURL(kSecondAppUrlHost, "/web_apps/basic.html");
  const webapps::AppId app2_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app2_url);

  {
    auto& sync_bridge =
        WebAppProvider::GetForTest(profile())->sync_bridge_unsafe();

    Browser* app_browser1 = LaunchWebAppBrowser(app1_id);
    ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
        app1_id, static_cast<uint32_t>(ShelfItemState::kActive)));
    {
      base::test::TestFuture<bool> success_future;
      test_controller->PinOrUnpinItemInShelf(app1_id, /*pin=*/true,
                                             success_future.GetCallback());
      EXPECT_TRUE(success_future.Get());
    }
    CloseAndWait(app_browser1);
    sync_bridge.SetAppUserDisplayMode(app1_id, mojom::UserDisplayMode::kBrowser,
                                      /*is_user_action=*/true);
    apps::AppWindowModeWaiter(profile(), app1_id, apps::WindowMode::kBrowser)
        .Await();

    Browser* app_browser2 = LaunchWebAppBrowser(app2_id);
    ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
        app2_id, static_cast<uint32_t>(ShelfItemState::kActive)));
    {
      base::test::TestFuture<bool> success_future;
      test_controller->PinOrUnpinItemInShelf(app2_id, /*pin=*/true,
                                             success_future.GetCallback());
      EXPECT_TRUE(success_future.Get());
    }
    CloseAndWait(app_browser2);
    sync_bridge.SetAppUserDisplayMode(app2_id, mojom::UserDisplayMode::kBrowser,
                                      /*is_user_action=*/true);
    apps::AppWindowModeWaiter(profile(), app2_id, apps::WindowMode::kBrowser)
        .Await();
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

  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(),
                                      TabCloseTypes::CLOSE_NONE);
  ASSERT_TRUE(AddTabAtIndex(/*index=*/1, app2_url, ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app1_id, static_cast<uint32_t>(ShelfItemState::kNormal)));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app2_id, static_cast<uint32_t>(ShelfItemState::kActive)));

  // Navigation is sufficient to change which app is considered running.
  {
    NavigateParams params(browser(), app1_url, ui::PAGE_TRANSITION_TYPED);
    params.tabstrip_index = tab_strip_model->active_index();
    params.disposition = WindowOpenDisposition::CURRENT_TAB;
    Navigate(&params);
    ASSERT_TRUE(
        content::WaitForLoadStop(params.navigated_or_inserted_contents));
  }

  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app2_id, static_cast<uint32_t>(ShelfItemState::kNormal)));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app1_id, static_cast<uint32_t>(ShelfItemState::kActive)));

  test::UninstallWebApp(profile(), app1_id);
  test::UninstallWebApp(profile(), app2_id);
}

// Tests that a web page without a manifest may be used to create a shortcut.
// Tests that a web page with a manifest etc. may be used to install a PWA.
// Tests that opening a shortcut in a tab does not make it appear in the Shelf.
// Tests that web apps opened in windows do appear in the Shelf.
IN_PROC_BROWSER_TEST_F(LacrosWebAppShelfBrowserTest, CreateShortcut) {
  if (!IsServiceAvailable())
    GTEST_SKIP();

  ASSERT_TRUE(embedded_test_server()->Start());
  crosapi::mojom::TestController* const test_controller =
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::TestController>()
          .get();
  auto& sync_bridge =
      WebAppProvider::GetForTest(profile())->sync_bridge_unsafe();

  GURL app1_url(
      embedded_test_server()->GetURL("/banners/scope_a/no_manifest.html"));
  GURL app2_url(
      embedded_test_server()->GetURL("/banners/scope_b/scope_b.html"));
  webapps::AppId app1_id;
  Browser* app1_browser;
  {
    web_app::ServiceWorkerRegistrationWaiter registration_waiter(profile(),
                                                                 app1_url);
    ASSERT_TRUE(
        AddTabAtIndex(/*index=*/1, app1_url, ui::PAGE_TRANSITION_TYPED));
    registration_waiter.AwaitRegistration();

    ASSERT_TRUE(
        AddTabAtIndex(/*index=*/2, app2_url, ui::PAGE_TRANSITION_TYPED));
    ASSERT_TRUE(base::test::RunUntil([&] {
      return web_app::GetAppMenuCommandState(IDC_INSTALL_PWA, browser()) ==
             kEnabled;
    }));

    // Install app1 shortcut.
    browser()->tab_strip_model()->ActivateTabAt(/*index=*/1);
    EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, browser()), kEnabled);
    EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, browser()), kNotPresent);

    SetAutoAcceptWebAppDialogForTesting(
        /*auto_accept=*/true,
        /*auto_open_in_window=*/true);
    ui_test_utils::BrowserChangeObserver browser_change_observer(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    WebAppTestInstallObserver install_observer(profile());
    install_observer.BeginListening();
    CHECK(chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT));
    app1_id = install_observer.Wait();
    app1_browser = browser_change_observer.Wait();
    EXPECT_TRUE(AppBrowserController::IsForWebApp(app1_browser, app1_id));
    SetAutoAcceptWebAppDialogForTesting(
        /*auto_accept=*/false,
        /*auto_open_in_window=*/false);
  }

  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app1_id, static_cast<uint32_t>(ShelfItemState::kActive)));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app_constants::kLacrosAppId,
      static_cast<uint32_t>(ShelfItemState::kRunning)));

  // Launch app1 in a browser tab (only).
  {
    sync_bridge.SetAppUserDisplayMode(app1_id, mojom::UserDisplayMode::kBrowser,
                                      /*is_user_action=*/false);
    apps::AppWindowModeWaiter(profile(), app1_id, apps::WindowMode::kBrowser)
        .Await();

    app1_browser->window()->Close();

    ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
        app1_id, static_cast<uint32_t>(ShelfItemState::kNormal)));

    ui_test_utils::TabAddedWaiter waiter(browser());
    test_controller->LaunchAppFromAppList(app1_id);
    waiter.Wait();
  }

  // Install app2 PWA.
  webapps::AppId app2_id;
  Browser* app2_browser;
  {
    browser()->tab_strip_model()->ActivateTabAt(/*index=*/1);
    EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, browser()), kEnabled);
    EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, browser()), kEnabled);

    SetAutoAcceptPWAInstallConfirmationForTesting(/*auto_accept=*/true);
    ui_test_utils::BrowserChangeObserver browser_change_observer(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    WebAppTestInstallObserver observer(profile());
    observer.BeginListening();
    CHECK(chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA));
    app2_id = observer.Wait();
    app2_browser = browser_change_observer.Wait();
    EXPECT_TRUE(AppBrowserController::IsForWebApp(app2_browser, app2_id));
    SetAutoAcceptPWAInstallConfirmationForTesting(
        /*auto_accept=*/false);
  }

  // App1 is open in a tab, but does not appear in the shelf.
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      app1_url);
  EXPECT_EQ(
      app2_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      app2_url);
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app2_id, static_cast<uint32_t>(ShelfItemState::kActive)));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app1_id, static_cast<uint32_t>(ShelfItemState::kNormal)));
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app_constants::kLacrosAppId,
      static_cast<uint32_t>(ShelfItemState::kRunning)));
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app1_id, /*exists=*/false));
}

}  // namespace web_app

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

class WebAppShelfBrowserTest : public InProcessBrowserTest {
 public:
  webapps::AppId InstallTestWebApp(
      const GURL& start_url,
      web_app::mojom::UserDisplayMode user_display_mode) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->user_display_mode = user_display_mode;
    const webapps::AppId app_id =
        web_app::test::InstallWebApp(profile(), std::move(web_app_info));
    apps::AppReadinessWaiter(profile(), app_id).Await();
    return app_id;
  }

  void PinToShelf(const webapps::AppId& app_id) {
    ui_test_utils::BrowserChangeObserver observer(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    auto* const proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    proxy->LaunchAppWithParams(apps::AppLaunchParams(
        app_id, apps::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromOmnibox));
    Browser* app_browser = observer.Wait();
    ash::ShelfModel::Get()->PinExistingItemWithID(app_id);
    app_browser->window()->Close();
  }

  Profile* profile() { return browser()->profile(); }
};

IN_PROC_BROWSER_TEST_F(WebAppShelfBrowserTest, SwitchingBetweenApps) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL start_a =
      embedded_test_server()->GetURL("/banners/scope_a/page_1.html");
  const GURL start_b =
      embedded_test_server()->GetURL("/banners/scope_b/scope_b.html");
  const GURL start_c = embedded_test_server()->GetURL("/web_apps/basic.html");
  // Outside the scope of any web app:
  const GURL start_d = embedded_test_server()->GetURL("/simple.html");

  const webapps::AppId app_a =
      InstallTestWebApp(start_a, web_app::mojom::UserDisplayMode::kBrowser);
  const webapps::AppId app_b =
      InstallTestWebApp(start_b, web_app::mojom::UserDisplayMode::kBrowser);
  const webapps::AppId app_c =
      InstallTestWebApp(start_c, web_app::mojom::UserDisplayMode::kStandalone);

  auto* const proxy = apps::AppServiceProxyFactory::GetForProfile(profile());

  // Web apps only appear in Shelf when pinned or running in app windows.
  // We don't need to pin |app_c| as it will be running in an app window.
  PinToShelf(app_a);
  PinToShelf(app_b);

  content::WebContents* contents_a;
  {
    ui_test_utils::TabAddedWaiter waiter(browser());
    proxy->Launch(app_a,
                  /*event_flags=*/0, apps::LaunchSource::kFromAppListGrid);
    waiter.Wait();
    contents_a = browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::WebContents* contents_b;
  {
    ui_test_utils::TabAddedWaiter waiter(browser());
    proxy->Launch(app_b,
                  /*event_flags=*/0, apps::LaunchSource::kFromAppListGrid);
    waiter.Wait();
    contents_b = browser()->tab_strip_model()->GetActiveWebContents();
  }

  Browser* browser_c;
  content::WebContents* contents_c;
  {
    ui_test_utils::BrowserChangeObserver observer(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    ui_test_utils::AllBrowserTabAddedWaiter waiter;
    proxy->Launch(app_c,
                  /*event_flags=*/0, apps::LaunchSource::kFromAppListGrid);
    browser_c = observer.Wait();
    contents_c = waiter.Wait();
  }

  content::WebContents* contents_d;
  {
    ui_test_utils::TabAddedWaiter waiter(browser());
    NavigateParams params(browser(), start_d, ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    ui_test_utils::NavigateToURL(&params);
    waiter.Wait();
    contents_d = browser()->tab_strip_model()->GetActiveWebContents();
  }

  // The Shelf model contains 3 web apps, and the Chrome browser.
  auto* const shelf_model = ash::ShelfModel::Get();
  EXPECT_EQ(shelf_model->item_count(), 4);
  for (const ash::ShelfItem& item : shelf_model->items()) {
    EXPECT_EQ(item.status, ash::STATUS_RUNNING);
  }

  ash::RootWindowController* const controller =
      ash::Shell::GetRootWindowControllerWithDisplayId(
          display::Screen::GetScreen()->GetPrimaryDisplay().id());
  ash::ShelfView* const shelf_view =
      controller->shelf()->GetShelfViewForTesting();
  const ash::ShelfAppButton* const button_a =
      shelf_view->GetShelfAppButton(ash::ShelfID(app_a));
  const ash::ShelfAppButton* const button_b =
      shelf_view->GetShelfAppButton(ash::ShelfID(app_b));
  const ash::ShelfAppButton* const button_c =
      shelf_view->GetShelfAppButton(ash::ShelfID(app_c));
  const ash::ShelfAppButton* const button_chrome =
      shelf_view->GetShelfAppButton(ash::ShelfID(app_constants::kChromeAppId));

  browser()->ActivateContents(contents_a);
  EXPECT_EQ(button_a->state(), ash::ShelfAppButton::STATE_ACTIVE);
  EXPECT_EQ(button_b->state(), ash::ShelfAppButton::STATE_RUNNING);
  EXPECT_EQ(button_c->state(), ash::ShelfAppButton::STATE_RUNNING);
  EXPECT_EQ(button_chrome->state(), ash::ShelfAppButton::STATE_RUNNING);

  browser()->ActivateContents(contents_b);
  EXPECT_EQ(button_a->state(), ash::ShelfAppButton::STATE_RUNNING);
  EXPECT_EQ(button_b->state(), ash::ShelfAppButton::STATE_ACTIVE);
  EXPECT_EQ(button_c->state(), ash::ShelfAppButton::STATE_RUNNING);
  EXPECT_EQ(button_chrome->state(), ash::ShelfAppButton::STATE_RUNNING);

  browser_c->ActivateContents(contents_c);
  EXPECT_EQ(button_a->state(), ash::ShelfAppButton::STATE_RUNNING);
  EXPECT_EQ(button_b->state(), ash::ShelfAppButton::STATE_RUNNING);
  EXPECT_EQ(button_c->state(), ash::ShelfAppButton::STATE_ACTIVE);
  EXPECT_EQ(button_chrome->state(), ash::ShelfAppButton::STATE_RUNNING);

  browser()->ActivateContents(contents_d);
  EXPECT_EQ(button_a->state(), ash::ShelfAppButton::STATE_RUNNING);
  EXPECT_EQ(button_b->state(), ash::ShelfAppButton::STATE_RUNNING);
  EXPECT_EQ(button_c->state(), ash::ShelfAppButton::STATE_RUNNING);
  EXPECT_EQ(button_chrome->state(), ash::ShelfAppButton::STATE_ACTIVE);

  browser()->window()->Close();
  EXPECT_EQ(button_a->state(), ash::ShelfAppButton::STATE_NORMAL);
  EXPECT_EQ(button_b->state(), ash::ShelfAppButton::STATE_NORMAL);
  EXPECT_EQ(button_c->state(), ash::ShelfAppButton::STATE_ACTIVE);
  EXPECT_EQ(button_chrome->state(), ash::ShelfAppButton::STATE_RUNNING);
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace web_app {

class LaunchWebAppBrowserTest : public WebAppBrowserTestBase {
 public:
  LaunchWebAppBrowserTest() = default;
  ~LaunchWebAppBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(LaunchWebAppBrowserTest, OpenLinkInWebApp) {
  const GURL start_url("https://app.site.test/example/index");
  const webapps::AppId app_id = InstallPWA(start_url);
  apps::AppReadinessWaiter(profile(), app_id).Await();

  size_t num_browsers = chrome::GetBrowserCount(browser()->profile());
  const int num_tabs = browser()->tab_strip_model()->count();
  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  const GURL initial_url = initial_tab->GetLastCommittedURL();
  ui_test_utils::BrowserChangeObserver browser_change_observer(
      /*browser=*/nullptr,
      ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);

  {
    ui_test_utils::UrlLoadObserver url_observer(start_url);
    content::ContextMenuParams params;
    params.page_url = GURL("https://www.example.com/");
    params.link_url = start_url;
    TestRenderViewContextMenu menu(*initial_tab->GetPrimaryMainFrame(), params);
    menu.Init();
    menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP,
                        0 /* event_flags */);
    url_observer.Wait();
  }

  Browser* const app_browser = browser_change_observer.Wait();
  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(++num_browsers, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_NE(browser(), app_browser);
  EXPECT_EQ(initial_url, initial_tab->GetLastCommittedURL());
  EXPECT_EQ(start_url, app_browser->tab_strip_model()
                           ->GetActiveWebContents()
                           ->GetLastCommittedURL());

  UninstallWebApp(app_id);
  apps::AppReadinessWaiter(profile(), app_id,
                           apps::Readiness::kUninstalledByUser)
      .Await();
}

}  // namespace web_app

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class AppServiceInternalsBrowserTest : public InProcessBrowserTest {
 private:
  // Stop test from installing OS hooks.
  web_app::OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
};

IN_PROC_BROWSER_TEST_F(AppServiceInternalsBrowserTest, LoadsWebUiPage) {
  // Install a Web App, so that the app-service-internals page has at least one
  // app visible.
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_apps/standalone/basic.html");
  web_app::InstallWebAppFromPage(browser(), app_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(web_contents);
  NavigateParams params(browser(), GURL("chrome://app-service-internals"),
                        ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);
  observer.WaitForNavigationFinished();
}

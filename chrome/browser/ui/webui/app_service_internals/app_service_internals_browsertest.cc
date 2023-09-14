// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#include "base/test/scoped_path_override.h"
#endif  // BUILDFLAG(IS_WIN)

class AppServiceInternalsBrowserTest : public InProcessBrowserTest {
#if BUILDFLAG(IS_WIN)
 private:
  // This is used to prevent creating shortcuts in the start menu dir.
  base::ScopedPathOverride override_start_dir_{base::DIR_START_MENU};
#endif  // BUILDFLAG(IS_WIN)
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

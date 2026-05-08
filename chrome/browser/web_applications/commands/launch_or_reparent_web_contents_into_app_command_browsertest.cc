// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/launch_or_reparent_web_contents_into_app_command.h"

#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace web_app {

using LaunchOrReparentWebContentsIntoAppCommandBrowserTest =
    WebAppBrowserTestBase;

IN_PROC_BROWSER_TEST_F(LaunchOrReparentWebContentsIntoAppCommandBrowserTest,
                       ReparentsWhenInScope) {
  GURL app_url = embedded_https_test_server().GetURL("/web_apps/basic.html");
  webapps::AppId app_id =
      test::InstallDummyWebApp(profile(), "Test App", app_url);

  // Navigate to the app URL in a regular browser tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  size_t initial_browser_count =
      GlobalBrowserCollection::GetInstance()->GetSize();

  ui_test_utils::BrowserCreatedObserver observer;

  base::test::TestFuture<LaunchOrReparentResult> future;
  provider().scheduler().LaunchOrReparentWebContentsIntoApp(
      app_id, web_contents->GetWeakPtr(), future.GetCallback());

  EXPECT_EQ(LaunchOrReparentResult::kReparented, future.Get());

  // TODO(crbug.com/492656527): Wait for commands to complete to make sure
  // the command finishes.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // Verify that a new browser was created.
  EXPECT_EQ(initial_browser_count + 1,
            GlobalBrowserCollection::GetInstance()->GetSize());

  // Find the new browser and verify it is for the app.
  Browser* app_browser = observer.Wait();
  EXPECT_TRUE(app_browser);
  EXPECT_NE(app_browser, browser());
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id));
}

IN_PROC_BROWSER_TEST_F(LaunchOrReparentWebContentsIntoAppCommandBrowserTest,
                       LaunchesWhenOutOfScope) {
  GURL app_url = embedded_https_test_server().GetURL("/web_apps/basic.html");
  webapps::AppId app_id =
      test::InstallDummyWebApp(profile(), "Test App", app_url);

  // Navigate to an out-of-scope URL in a regular browser tab.
  GURL out_of_scope_url = embedded_https_test_server().GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), out_of_scope_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  size_t initial_browser_count =
      GlobalBrowserCollection::GetInstance()->GetSize();

  ui_test_utils::BrowserCreatedObserver observer;

  base::test::TestFuture<LaunchOrReparentResult> future;
  provider().scheduler().LaunchOrReparentWebContentsIntoApp(
      app_id, web_contents->GetWeakPtr(), future.GetCallback());

  EXPECT_EQ(LaunchOrReparentResult::kLaunched, future.Get());

  // TODO(crbug.com/492656527): Wait for commands to complete to make sure
  // the scheduled LaunchApp command finishes.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // Verify that a new browser was created.
  EXPECT_EQ(initial_browser_count + 1,
            GlobalBrowserCollection::GetInstance()->GetSize());

  // Find the new browser and verify it is for the app.
  Browser* app_browser = observer.Wait();
  EXPECT_TRUE(app_browser);
  EXPECT_NE(app_browser, browser());
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id));
}

}  // namespace web_app

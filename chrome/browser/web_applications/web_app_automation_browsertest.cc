// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PROCESS_SINGLETON)
namespace web_app {

class WebAppAutomationBrowserTest : public WebAppBrowserTestBase {
 public:
  WebAppAutomationBrowserTest() = default;
  ~WebAppAutomationBrowserTest() override = default;

  GURL test_url() { return https_server()->GetURL("/web_apps/basic.html"); }

  base::CommandLine GetWebAppCommandLine(const webapps::AppId& app_id,
                                         bool enable_automation) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(switches::kAppId, app_id);
    command_line.AppendSwitchASCII(switches::kProfileDirectory, "");
    if (enable_automation) {
      command_line.AppendSwitch(switches::kEnableAutomation);
    }
    return command_line;
  }
};

IN_PROC_BROWSER_TEST_F(WebAppAutomationBrowserTest,
                       OnlyExistingProcessWithAutomation) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAutomation);

  webapps::AppId app_id = InstallWebAppFromPage(browser(), test_url());

  base::CommandLine command_line =
      GetWebAppCommandLine(app_id, /*enable_automation=*/false);
  ASSERT_FALSE(ChromeBrowserMainParts::ProcessSingletonNotificationCallback(
      command_line, /*current_directory=*/{}));
}

IN_PROC_BROWSER_TEST_F(WebAppAutomationBrowserTest,
                       OnlyNewProcessWithAutomation) {
  webapps::AppId app_id = InstallWebAppFromPage(browser(), test_url());

  base::CommandLine command_line =
      GetWebAppCommandLine(app_id, /*enable_automation=*/true);
  command_line.AppendSwitch(switches::kEnableAutomation);

  ASSERT_FALSE(ChromeBrowserMainParts::ProcessSingletonNotificationCallback(
      command_line, /*current_directory=*/{}));
}

IN_PROC_BROWSER_TEST_F(WebAppAutomationBrowserTest,
                       ExistingAndNewProcessWithAutomation) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAutomation);

  webapps::AppId app_id = InstallWebAppFromPage(browser(), test_url());

  base::CommandLine command_line =
      GetWebAppCommandLine(app_id, /*enable_automation=*/true);
  ui_test_utils::BrowserChangeObserver browser_added_waiter(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  ASSERT_TRUE(ChromeBrowserMainParts::ProcessSingletonNotificationCallback(
      command_line, /*current_directory=*/{}));
  EXPECT_TRUE(
      AppBrowserController::IsForWebApp(browser_added_waiter.Wait(), app_id));
}

}  // namespace web_app
#endif  // BUILDFLAG(ENABLE_PROCESS_SINGLETON)

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/shortcut.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_PROCESS_SINGLETON)
namespace web_app {

class WebAppAutomationBrowserTest : public WebAppBrowserTestBase {
 public:
  WebAppAutomationBrowserTest() = default;
  ~WebAppAutomationBrowserTest() override = default;

  GURL test_url() { return https_server()->GetURL("/web_apps/basic.html"); }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

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
  ASSERT_FALSE(ChromeBrowserMainParts::ProcessSingletonNotificationForTesting(
      command_line));
}

IN_PROC_BROWSER_TEST_F(WebAppAutomationBrowserTest,
                       OnlyNewProcessWithAutomation) {
  webapps::AppId app_id = InstallWebAppFromPage(browser(), test_url());

  base::CommandLine command_line =
      GetWebAppCommandLine(app_id, /*enable_automation=*/true);
  command_line.AppendSwitch(switches::kEnableAutomation);

  ASSERT_FALSE(ChromeBrowserMainParts::ProcessSingletonNotificationForTesting(
      command_line));
}

IN_PROC_BROWSER_TEST_F(WebAppAutomationBrowserTest,
                       ExistingAndNewProcessWithAutomation) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAutomation);

  webapps::AppId app_id = InstallWebAppFromPage(browser(), test_url());

  base::CommandLine command_line =
      GetWebAppCommandLine(app_id, /*enable_automation=*/true);
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  ASSERT_TRUE(ChromeBrowserMainParts::ProcessSingletonNotificationForTesting(
      command_line));
  EXPECT_TRUE(AppBrowserController::IsForWebApp(browser_created_observer.Wait(),
                                                app_id));
}

#if BUILDFLAG(IS_WIN)
class WebAppAutomationShortcutBrowserTest
    : public WebAppAutomationBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  WebAppAutomationShortcutBrowserTest() = default;
  ~WebAppAutomationShortcutBrowserTest() override = default;

  bool enable_automation() const { return GetParam(); }

  void ValidateShortcut(const base::FilePath& shortcut_path,
                        webapps::AppId app_id,
                        bool has_automation_switch) {
    EXPECT_TRUE(base::PathExists(shortcut_path))
        << "Shortcut path does not exist: " << shortcut_path.value();
    std::wstring cmd_line_string;
    EXPECT_TRUE(
        base::win::ResolveShortcut(shortcut_path, nullptr, &cmd_line_string));
    cmd_line_string = L"program " + cmd_line_string;
    base::CommandLine shortcut_cmd_line =
        base::CommandLine::FromString(cmd_line_string);
    EXPECT_TRUE(
        shortcut_cmd_line.HasSwitch(switches::kProfileDirectory) &&
        shortcut_cmd_line.GetSwitchValuePath(switches::kProfileDirectory) ==
            profile()->GetPath().BaseName());
    EXPECT_TRUE(shortcut_cmd_line.HasSwitch(switches::kAppId) &&
                shortcut_cmd_line.GetSwitchValueASCII(switches::kAppId) ==
                    app_id);
    EXPECT_EQ(shortcut_cmd_line.HasSwitch(switches::kEnableAutomation),
              has_automation_switch);
  }
};

IN_PROC_BROWSER_TEST_P(WebAppAutomationShortcutBrowserTest, ShortcutCreation) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  if (enable_automation()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableAutomation);
  }
  webapps::AppId app_id = InstallWebAppFromPage(browser(), test_url());

  EXPECT_EQ(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
            provider()->registrar_unsafe().GetInstallState(app_id));
  EXPECT_TRUE(os_integration_override().IsShortcutCreated(
      profile(), app_id,
      provider()->registrar_unsafe().GetAppShortName(app_id)));

  base::FilePath desktop_shortcut_path =
      os_integration_override().GetShortcutPath(
          profile(), os_integration_override().desktop(), app_id,
          provider()->registrar_unsafe().GetAppShortName(app_id));
  base::FilePath app_menu_shortcut_path =
      os_integration_override().GetShortcutPath(
          profile(), os_integration_override().application_menu(), app_id,
          provider()->registrar_unsafe().GetAppShortName(app_id));
  ValidateShortcut(desktop_shortcut_path, app_id,
                   /*has_automation_switch=*/enable_automation());
  ValidateShortcut(app_menu_shortcut_path, app_id,
                   /*has_automation_switch=*/enable_automation());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppAutomationShortcutBrowserTest,
    testing::Bool(),
    [](const testing::TestParamInfo<
        WebAppAutomationShortcutBrowserTest::ParamType>& info) {
      return info.param ? "WithAutomation" : "WithoutAutomation";
    });
#endif  // BUILDFLAG(IS_WIN)

}  // namespace web_app
#endif  // BUILDFLAG(ENABLE_PROCESS_SINGLETON)

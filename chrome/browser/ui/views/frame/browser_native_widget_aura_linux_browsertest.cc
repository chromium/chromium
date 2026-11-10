// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"
#include "ui/ozone/public/ozone_platform.h"
#include "url/gurl.h"

namespace {

// Returns the res_name and res_class of the X11 WM_CLASS property of the
// window of `browser`.
std::vector<std::string> GetWmClass(BrowserWindowInterface* browser) {
  const x11::Window window =
      static_cast<x11::Window>(browser->GetWindow()
                                   ->GetNativeWindow()
                                   ->GetHost()
                                   ->GetAcceleratedWidget());
  std::vector<char> data;
  if (!x11::Connection::Get()->GetArrayProperty(window, x11::Atom::WM_CLASS,
                                                &data)) {
    return {};
  }
  // WM_CLASS is two consecutive NUL-terminated strings.
  return base::SplitString(std::string(data.begin(), data.end()),
                           std::string(1, '\0'), base::KEEP_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

class BrowserNativeWidgetAuraLinuxWmClassTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    if (ui::OzonePlatform::GetPlatformNameForTest() != "x11") {
      GTEST_SKIP() << "WM_CLASS only exists on X11";
    }
  }

 protected:
  // Installs and launches a web app, and returns its window.
  BrowserWindowInterface* LaunchWebApp(std::string* desktop_id) {
    Profile* const profile = browser()->GetProfile();
    const webapps::AppId app_id = web_app::test::InstallDummyWebApp(
        profile, "Test App", GURL("https://app.test/"));
    *desktop_id = shell_integration_linux::GetXdgAppIdForWebApp(
        web_app::GenerateApplicationNameFromAppId(app_id), profile->GetPath());
    return web_app::LaunchWebAppBrowser(profile, app_id);
  }

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

// Browser windows of the default profile keep the browser's WM_CLASS, so
// window manager rules that match it keep working.
IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetAuraLinuxWmClassTest, BrowserWindow) {
  EXPECT_EQ(GetWmClass(browser()),
            (std::vector<std::string>{
                shell_integration_linux::GetProgramClassName(),
                shell_integration_linux::GetProgramClassClass(), ""}));
}

// Both parts of the WM_CLASS of installed web app windows are the ID of the
// app's desktop entry, so taskbars that group windows by res_class show them
// separately from the browser.
IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetAuraLinuxWmClassTest, WebAppWindow) {
  std::string desktop_id;
  BrowserWindowInterface* const app_browser = LaunchWebApp(&desktop_id);
  ASSERT_TRUE(app_browser);
  EXPECT_EQ(GetWmClass(app_browser),
            (std::vector<std::string>{desktop_id, desktop_id, ""}));
}

class BrowserNativeWidgetAuraLinuxWmClassSwitchTest
    : public BrowserNativeWidgetAuraLinuxWmClassTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    BrowserNativeWidgetAuraLinuxWmClassTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kWmClass, "CustomClass");
  }
};

// --class sets the res_class of all windows, including web app windows.
IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetAuraLinuxWmClassSwitchTest,
                       WebAppWindow) {
  std::string desktop_id;
  BrowserWindowInterface* const app_browser = LaunchWebApp(&desktop_id);
  ASSERT_TRUE(app_browser);
  EXPECT_EQ(GetWmClass(app_browser),
            (std::vector<std::string>{desktop_id, "CustomClass", ""}));
  EXPECT_EQ(
      GetWmClass(browser()),
      (std::vector<std::string>{shell_integration_linux::GetProgramClassName(),
                                "CustomClass", ""}));
}

}  // namespace

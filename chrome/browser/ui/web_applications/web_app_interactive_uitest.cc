// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/run_until.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/find_in_page/find_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/display/screen.h"
#include "ui/display/screen_base.h"
#include "ui/display/test/virtual_display_util.h"

namespace web_app {
namespace {
constexpr const char kExampleURL[] = "http://example.org/";
}

class WebAppInteractiveUiTest : public WebAppBrowserTestBase {};

// Disabled everywhere except ChromeOS, Mac and Windows because those are the
// only platforms with functional display mocking at the moment. While a partial
// solution is possible using display::Screen::SetScreenInstance on other
// platforms, window placement doesn't work right with a faked Screen
// instance. See: //docs/ui/display/multiscreen_testing.md
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#define MAYBE_TabOpensOnCorrectDisplayMultiScreen \
  TabOpensOnCorrectDisplayMultiScreen
#else
#define MAYBE_TabOpensOnCorrectDisplayMultiScreen \
  DISABLED_TabOpensOnCorrectDisplayMultiScreen
#endif
#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/371121282): Re-enable the test.
// TODO(crbug.com/365126887): Re-enable the test.
#undef MAYBE_TabOpensOnCorrectDisplayMultiScreen
#define MAYBE_TabOpensOnCorrectDisplayMultiScreen \
  DISABLED_TabOpensOnCorrectDisplayMultiScreen
#endif  // BUILDFLAG(IS_WIN)
// Tests that PWAs that open in a tab open tabs on the correct display.
IN_PROC_BROWSER_TEST_F(WebAppInteractiveUiTest,
                       MAYBE_TabOpensOnCorrectDisplayMultiScreen) {
  std::unique_ptr<display::test::VirtualDisplayUtil> virtual_display_util;
  if (display::Screen::Get()->GetNumDisplays() < 2) {
    if ((virtual_display_util = display::test::VirtualDisplayUtil::TryCreate(
             display::Screen::Get()))) {
      virtual_display_util->AddDisplay(
          display::test::VirtualDisplayUtil::k1024x768);
    } else {
      GTEST_SKIP() << "Skipping test; unavailable multi-screen support.";
    }
  }

  // Install test app.
  const webapps::AppId app_id = InstallPWA(GURL(kExampleURL));

  // Figure out what display the original tabbed browser was created on, as well
  // as what the display Id is for a second display.
  Browser* original_browser = browser();
  const std::vector<display::Display>& displays =
      display::Screen::Get()->GetAllDisplays();
  display::Display original_display =
      display::Screen::Get()->GetDisplayNearestWindow(
          original_browser->window()->GetNativeWindow());
  display::Display other_display;
  for (const auto& d : displays) {
    if (d.id() != original_display.id()) {
      other_display = d;
      break;
    }
  }
  ASSERT_TRUE(other_display.is_valid());

  // By default opening a PWA in a tab should open on the same display as the
  // existing tabbed browser, and thus in the same browser window.
  Browser* app_browser = LaunchBrowserForWebAppInTab(app_id);
  EXPECT_EQ(app_browser, original_browser);
  EXPECT_EQ(2, original_browser->tab_strip_model()->count());

  {
    // Setting the display for new windows only effects what display a browser
    // window is opened on on Chrome OS. On other platforms a new tab should
    // be opened in the existing window regardless of what display we set for
    // new windows.
    display::ScopedDisplayForNewWindows scoped_display(other_display.id());
    app_browser = LaunchBrowserForWebAppInTab(app_id);

#if BUILDFLAG(IS_CHROMEOS)
    EXPECT_NE(app_browser, original_browser);
    EXPECT_EQ(
        other_display.id(),
        display::Screen::Get()
            ->GetDisplayNearestWindow(app_browser->window()->GetNativeWindow())
            .id());
    EXPECT_EQ(2, original_browser->tab_strip_model()->count());
    EXPECT_EQ(1, app_browser->tab_strip_model()->count());

    // A second launch should re-use the same browser window.
    Browser* app_browser2 = LaunchBrowserForWebAppInTab(app_id);
    EXPECT_EQ(app_browser, app_browser2);
    EXPECT_EQ(2, original_browser->tab_strip_model()->count());
    EXPECT_EQ(2, app_browser->tab_strip_model()->count());
#else
    EXPECT_EQ(app_browser, original_browser);
    EXPECT_EQ(3, original_browser->tab_strip_model()->count());
#endif
  }

  {
    // Forcing the app to launch on the original display should open a new tab
    // in the original browser.
    display::ScopedDisplayForNewWindows scoped_display(original_display.id());
    app_browser = LaunchBrowserForWebAppInTab(app_id);
    EXPECT_EQ(app_browser, original_browser);
#if BUILDFLAG(IS_CHROMEOS)
    EXPECT_EQ(3, original_browser->tab_strip_model()->count());
#else
    EXPECT_EQ(4, original_browser->tab_strip_model()->count());
#endif
  }
}

// Tests that opening and closing the Find bar triggers the correct focus
// events in PWA windows.
// - Opening find bar: OnWebContentsLostFocus called once, OnWebContentsFocused
//   NOT called
// - Closing find bar: OnWebContentsFocused called once, OnWebContentsLostFocus
//   NOT called
IN_PROC_BROWSER_TEST_F(WebAppInteractiveUiTest, FindBarFocusEvents) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("app.site.test", "/simple.html");
  auto web_app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
  web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  const webapps::AppId app_id = InstallWebApp(std::move(web_app_info));
  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Create focus event tracker.
  ui_test_utils::WebContentsFocusEventTracker focus_tracker(web_contents);
  EXPECT_TRUE(WaitForLoadStop(web_contents));
#if BUILDFLAG(IS_MAC)
  // On Mac, PWA windows auto-gain focus on launch. Need to wait for the initial
  // focus event to complete before opening the find bar.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return focus_tracker.focused_count() == 1; }));
#endif  // BUILDFLAG(IS_MAC)

  // Reset tracker for the open operation.
  focus_tracker.Reset();
  // Open the find bar.
  chrome::Find(app_browser);
  // Wait for the lost focus event to be processed correctly.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return focus_tracker.lost_focus_count() == 1 &&
           focus_tracker.focused_count() == 0;
  }));

  // Reset tracker for the close operation.
  focus_tracker.Reset();
  // Close the find bar.
  app_browser->GetFeatures().GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);
  // Wait for the focus event to be processed correctly.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return focus_tracker.focused_count() == 1 &&
           focus_tracker.lost_focus_count() == 0;
  }));
}

}  // namespace web_app

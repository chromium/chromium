// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/display/screen_base.h"
#include "ui/display/test/scoped_screen_override.h"
#include "ui/display/test/test_screen.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "ui/display/test/display_manager_test_api.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class WindowPlacementTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "WindowPlacement");
  }
};

// TODO(crbug.com/1183791): Disabled on non-ChromeOS because of races with
// SetScreenInstance and observers not being notified.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_OnScreensChangeEvent DISABLED_OnScreensChangeEvent
#else
#define MAYBE_OnScreensChangeEvent OnScreensChangeEvent
#endif
IN_PROC_BROWSER_TEST_F(WindowPlacementTest, MAYBE_OnScreensChangeEvent) {
  // Updates the display configuration to add a secondary display.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+1-801x802");
#else
  display::ScreenBase screen;
  screen.display_list().AddDisplay({1, gfx::Rect(100, 1, 801, 802)},
                                   display::DisplayList::Type::PRIMARY);
  display::test::ScopedScreenOverride screen_override(&screen);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/simple.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // TODO(crbug.com/1119974): this test could be in content_browsertests
  // and not browser_tests if permission controls were supported.

  // Auto-accept the Window Placement permission request.
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  auto initial_result = std::vector<base::Value>();
  initial_result.emplace_back(801);
  ASSERT_EQ(initial_result, EvalJs(tab, R"(
      var screensInterface;
      var promiseForEvent = (target, evt) => {
        return new Promise((resolve) => {
          const handler = (e) => {
            target.removeEventListener(evt, handler);
            resolve(e);
          };
          target.addEventListener(evt, handler);
        });
      }
      var makeScreensChangePromise = () => {
        return promiseForEvent(screensInterface, 'screenschange');
      };
      var getScreenWidths = () => {
        return screensInterface.screens.map((d) => d.width).sort();
      };

      (async () => {
          screensInterface = await self.getScreens();
          return getScreenWidths();
      })();
  )"));

  // Add a second display.
  EXPECT_TRUE(
      ExecJs(tab, R"(var screensChange = makeScreensChangePromise();)"));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-801x802,901+100-802x802");
#else
  screen.display_list().AddDisplay({2, gfx::Rect(901, 100, 802, 802)},
                                   display::DisplayList::Type::PRIMARY);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

  {
    auto result = std::vector<base::Value>();
    result.emplace_back(801);
    result.emplace_back(802);

    EXPECT_EQ(result, EvalJs(tab, R"(
      (async () => {
          await screensChange;
          return getScreenWidths();
      })();
    )"));
  }

  // Remove the first display.
  EXPECT_TRUE(
      ExecJs(tab, R"(var screensChange = makeScreensChangePromise();)"));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("901+100-802x802");
#else
  // Make the second display primary so we can remove the first.
  EXPECT_EQ(screen.display_list().displays().size(), 2u);
  screen.display_list().RemoveDisplay(1);
  EXPECT_EQ(screen.display_list().displays().size(), 1u);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  {
    auto result = std::vector<base::Value>();
    result.emplace_back(802);

    EXPECT_EQ(result, EvalJs(tab, R"(
      (async () => {
          await screensChange;
          return getScreenWidths();
      })();
    )"));
  }

  // Remove one display, add two displays.
  // TODO(enne): we add two displays here because DisplayManagerTestApi
  // would consider remove+add to just be an update (with the same id).
  // An alternative is to modify DisplayManagerTestApi to let us set ids.
  EXPECT_TRUE(
      ExecJs(tab, R"(var screensChange = makeScreensChangePromise();)"));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("0+0-803x600,1000+0-804x600");
#else
  screen.display_list().RemoveDisplay(2);
  screen.display_list().AddDisplay({3, gfx::Rect(0, 4, 803, 600)},
                                   display::DisplayList::Type::PRIMARY);
  screen.display_list().AddDisplay({4, gfx::Rect(0, 4, 804, 600)},
                                   display::DisplayList::Type::NOT_PRIMARY);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

  {
    auto result = std::vector<base::Value>();
    result.emplace_back(803);
    result.emplace_back(804);

    EXPECT_EQ(result, EvalJs(tab, R"(
      (async () => {
          await screensChange;
          return getScreenWidths();
      })();
    )"));
  }
}

// TODO(crbug.com/1183791): Disabled on non-ChromeOS because of races with
// SetScreenInstance and observers not being notified.
// TODO(crbug.com/1194700): Disabled on Mac because of GetScreenInfos staleness.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_OnCurrentScreenChangeEvent DISABLED_OnCurrentScreenChangeEvent
#else
#define MAYBE_OnCurrentScreenChangeEvent OnCurrentScreenChangeEvent
#endif
// Test that the oncurrentscreenchange handler fires correctly for screen
// changes and property updates.
IN_PROC_BROWSER_TEST_F(WindowPlacementTest, MAYBE_OnCurrentScreenChangeEvent) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-801x802,901+100-802x802");
#else
  display::ScreenBase screen;
  screen.display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                   display::DisplayList::Type::PRIMARY);
  screen.display_list().AddDisplay({2, gfx::Rect(901, 100, 802, 802)},
                                   display::DisplayList::Type::NOT_PRIMARY);
  display::test::ScopedScreenOverride screen_override(&screen);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/simple.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // TODO(crbug.com/1119974): this test could be in content_browsertests
  // and not browser_tests if permission controls were supported.

  // Auto-accept the Window Placement permission request.
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_EQ(801, EvalJs(tab, R"(
      var screensInterface;
      var promiseForEvent = (target, evt) => {
        return new Promise((resolve) => {
          const handler = (e) => {
            target.removeEventListener(evt, handler);
            resolve(e);
          };
          target.addEventListener(evt, handler);
        });
      }
      var makeCurrentScreenChangePromise = () => {
        return promiseForEvent(screensInterface, 'currentscreenchange');
      };
      (async () => {
          screensInterface = await self.getScreens();
          return screensInterface.currentScreen.width;
      })();
  )"));

  // Switch to a second display.  This should fire an event.
  EXPECT_TRUE(ExecJs(tab, R"(var change = makeCurrentScreenChangePromise();)"));

  const gfx::Rect new_bounds(1000, 150, 600, 500);
  browser()->window()->SetBounds(new_bounds);

  EXPECT_EQ(802, EvalJs(tab, R"(
      (async () => {
          await change;
          return screensInterface.currentScreen.width;
      })();
    )"));

  // Update the second display to have a height of 300.  Validate that a change
  // event is fired when attributes of the current screen change.
  EXPECT_TRUE(ExecJs(tab, R"(var change = makeCurrentScreenChangePromise();)"));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-801x802,901+100-802x300");
#else
  screen.display_list().UpdateDisplay({2, gfx::Rect(901, 100, 802, 300)},
                                      display::DisplayList::Type::NOT_PRIMARY);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  EXPECT_EQ(300, EvalJs(tab, R"(
      (async () => {
          await change;
          return screensInterface.currentScreen.height;
      })();
    )"));
}

// TODO(crbug.com/1183791): Disabled on non-ChromeOS because of races with
// SetScreenInstance and observers not being notified.
// TODO(crbug.com/1194700): Disabled on Mac because of GetScreenInfos staleness.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ScreenAdvancedOnChange DISABLED_ScreenAdvancedOnChange
#else
#define MAYBE_ScreenAdvancedOnChange ScreenAdvancedOnChange
#endif
// Test that onchange events for individual screens in the screen list are
// supported.
IN_PROC_BROWSER_TEST_F(WindowPlacementTest, MAYBE_ScreenAdvancedOnChange) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-801x802,901+100-802x802");
#else
  display::ScreenBase screen;
  screen.display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                   display::DisplayList::Type::PRIMARY);
  screen.display_list().AddDisplay({2, gfx::Rect(901, 100, 802, 802)},
                                   display::DisplayList::Type::NOT_PRIMARY);
  display::test::ScopedScreenOverride screen_override(&screen);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/simple.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // TODO(crbug.com/1119974): this test could be in content_browsertests
  // and not browser_tests if permission controls were supported.

  // Auto-accept the Window Placement permission request.
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_EQ(true, EvalJs(tab, R"(
      var screensInterface;
      var promiseForEvent = (target, evt) => {
        return new Promise((resolve) => {
          const handler = (e) => {
            target.removeEventListener(evt, handler);
            resolve(e);
          };
          target.addEventListener(evt, handler);
        });
      }
      var screenChanges0 = 0;
      var screenChanges1 = 0;
      (async () => {
        screensInterface = await self.getScreens();
        if (screensInterface.screens.length !== 2)
          return false;
        // Add some event listeners for individual screens.
        screensInterface.screens[0].addEventListener('change', () => {
          screenChanges0++;
        });
        screensInterface.screens[1].addEventListener('change', () => {
          screenChanges1++;
        });
        return true;
      })();
  )"));

  // Update only the first display to have a different height.
  EXPECT_TRUE(ExecJs(tab,
                     R"(
    var change0 = promiseForEvent(screensInterface.screens[0], 'change');
    )"));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-801x301,901+100-802x802");
#else
  screen.display_list().UpdateDisplay({1, gfx::Rect(100, 100, 801, 301)},
                                      display::DisplayList::Type::PRIMARY);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  EXPECT_EQ(301, EvalJs(tab, R"(
      (async () => {
          await change0;
          // Only screen[0] should have changed.
          if (screenChanges0 !== 1)
            return -1;
          if (screenChanges1 !== 0)
            return -2;
          return screensInterface.screens[0].height;
      })();
    )"));

  // Update only the second display to have a different height.
  EXPECT_TRUE(ExecJs(tab,
                     R"(
    var change1 = promiseForEvent(screensInterface.screens[1], 'change');
    )"));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-801x301,901+100-802x302");
#else
  screen.display_list().UpdateDisplay({2, gfx::Rect(901, 100, 802, 302)},
                                      display::DisplayList::Type::NOT_PRIMARY);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  EXPECT_EQ(302, EvalJs(tab, R"(
      (async () => {
          await change1;
          // Both screens have one change.
          if (screenChanges0 !== 1)
            return -1;
          if (screenChanges1 !== 1)
            return -2;
          return screensInterface.screens[1].height;
      })();
    )"));

  // Change the width of both displays at the same time.
  EXPECT_TRUE(ExecJs(tab,
                     R"(
    var change0 = promiseForEvent(screensInterface.screens[0], 'change');
    var change1 = promiseForEvent(screensInterface.screens[1], 'change');
    )"));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-401x301,901+100-402x302");
#else
  screen.display_list().UpdateDisplay({1, gfx::Rect(100, 100, 401, 301)},
                                      display::DisplayList::Type::PRIMARY);
  screen.display_list().UpdateDisplay({2, gfx::Rect(901, 100, 402, 302)},
                                      display::DisplayList::Type::NOT_PRIMARY);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  EXPECT_EQ(true, EvalJs(tab, R"(
      (async () => {
          await change0;
          await change1;
          // Both screens have two changes
          if (screenChanges0 !== 2)
            return false;
          if (screenChanges1 !== 2)
            return false;
          if (screensInterface.screens[0].width !== 401)
            return false;
          if (screensInterface.screens[1].width !== 402)
            return false;
          return true;
      })();
    )"));
}

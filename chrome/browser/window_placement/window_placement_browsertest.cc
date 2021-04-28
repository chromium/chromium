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
#include "ui/display/test/display_manager_test_api.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class WindowPlacementTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "WindowPlacement");
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }
};

// TODO(crbug.com/1183791): Disabled on non-ChromeOS because of races with
// SetScreenInstance and observers not being notified.
// TODO(crbug.com/1194700): Disabled on Mac because of GetScreenInfos staleness.
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
        return promiseForEvent(screensInterface, 'change');
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

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

namespace {

// Tests of window placement for popup browser windows. Test fixtures are run
// with and without the experimental WindowPlacement blink feature.
class PopupBrowserTest : public InProcessBrowserTest,
                         public ::testing::WithParamInterface<bool> {
 protected:
  PopupBrowserTest() = default;
  ~PopupBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisablePopupBlocking);
    const bool enable_window_placement = GetParam();
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        enable_window_placement ? "enable-blink-features"
                                : "disable-blink-features",
        "WindowPlacement");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PopupBrowserTest);
};

INSTANTIATE_TEST_SUITE_P(/* no prefix */, PopupBrowserTest, ::testing::Bool());

// Ensure that popup windows are clamped within the available screen space.
IN_PROC_BROWSER_TEST_P(PopupBrowserTest, WindowClampedToScreenSpace) {
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* screen = display::Screen::GetScreen();
  const auto& displays = screen->GetAllDisplays();
  EXPECT_GE(displays.size(), 1U) << "Expected at least one display";
  const auto display =
      screen->GetDisplayNearestWindow(browser()->window()->GetNativeWindow());
  EXPECT_TRUE(display.bounds().Contains(browser()->window()->GetBounds()))
      << "The browser window bounds should be contained by its display";

  // Attempt to open a window outside the bounds of the originator's display.
  const char OPEN_POPUP_OFFSCREEN[] =
      "var l = screen.availLeft + screen.availWidth + 100;"
      "var t = screen.availTop + 100;"
      "window.open(\"\", \"\","
      "            \"left=\"+l+\",top=\"+t+\",width=300,height=300\");";
  contents->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      base::ASCIIToUTF16(OPEN_POPUP_OFFSCREEN));
  Browser* popup = ui_test_utils::WaitForBrowserToOpen();
  EXPECT_NE(popup, browser());

  // The popup window should be clamped within the available screen space.
  // With experimental WindowPlacement, the popup may be on another display.
  const auto popup_display =
      screen->GetDisplayNearestWindow(popup->window()->GetNativeWindow());
  if (!GetParam())
    EXPECT_EQ(display, popup_display);

  auto popup_display_bounds = popup_display.bounds();
#if defined(OS_WIN)
  // TODO(crbug.com/1023054) Windows should more strictly clamp popup bounds.
  popup_display_bounds.Inset(-20, -20);
#endif  // OS_WIN
  EXPECT_TRUE(popup_display_bounds.Contains(popup->window()->GetBounds()));
}

}  // namespace

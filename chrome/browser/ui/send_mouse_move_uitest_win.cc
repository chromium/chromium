// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

class SendMouseMoveUITest : public InProcessBrowserTest {
 protected:
  SendMouseMoveUITest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SendMouseMoveUITest);
};

// This test positions the mouse at every point on the screen. It is not meant
// to be run on the bots, as it takes too long. Run it manually as needed to
// verify ui_controls::SendMouseMoveNotifyWhenDone with:
//   interactive_ui_tests.exe --single-process-tests \
// --gtest_also_run_disabled_tests \
// --gtest_filter=SendMouseMoveUITest.DISABLED_Fullscreen
IN_PROC_BROWSER_TEST_F(SendMouseMoveUITest, DISABLED_Fullscreen) {
  // Make the browser fullscreen so that we can position the mouse anywhere on
  // the display, as ui_controls::SendMouseMoveNotifyWhenDone can only provide
  // notifications when the mouse is moved over a window belonging to the
  // current process.
  chrome::ToggleFullscreenMode(browser());

  display::Screen* const screen = display::Screen::GetScreen();
  const gfx::Rect screen_bounds = screen->GetPrimaryDisplay().bounds();
  for (long scan_y = screen_bounds.y(),
            bound_y = scan_y + screen_bounds.height();
       scan_y < bound_y; ++scan_y) {
    for (long scan_x = screen_bounds.x(),
              bound_x = scan_x + screen_bounds.width();
         scan_x < bound_x; ++scan_x) {
      SCOPED_TRACE(testing::Message()
                   << "(" << scan_x << ", " << scan_y << ")");
      // Move the pointer.
      base::RunLoop run_loop;
      EXPECT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
          scan_x, scan_y, run_loop.QuitClosure()));
      run_loop.Run();

      // Check it.
      EXPECT_EQ(screen->GetCursorScreenPoint(), gfx::Point(scan_x, scan_y));
    }
  }
}

// Test that the mouse can be positioned at a few locations on the screen.
IN_PROC_BROWSER_TEST_F(SendMouseMoveUITest, Probe) {
  // Make the browser fullscreen so that we can position the mouse anywhere on
  // the display, as ui_controls::SendMouseMoveNotifyWhenDone can only provide
  // notifications when the mouse is moved over a window belonging to the
  // current process.
  chrome::ToggleFullscreenMode(browser());

  display::Screen* const screen = display::Screen::GetScreen();
  const gfx::Rect screen_bounds = screen->GetPrimaryDisplay().bounds();

  // Position the mouse at the corners and the center.
  const gfx::Point kPoints[] = {
      screen_bounds.origin(),
      gfx::Point(screen_bounds.right() - 1, screen_bounds.y()),
      gfx::Point(screen_bounds.x(), screen_bounds.bottom() - 1),
      gfx::Point(screen_bounds.right() - 1, screen_bounds.bottom() - 1),
      screen_bounds.CenterPoint()};

  for (const auto& point : kPoints) {
    SCOPED_TRACE(testing::Message()
                 << "(" << point.x() << ", " << point.y() << ")");
    // Move the pointer.
    base::RunLoop run_loop;
    EXPECT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        point.x(), point.y(), run_loop.QuitClosure()));
    run_loop.Run();

    // Check it.
    EXPECT_EQ(screen->GetCursorScreenPoint(), point);
  }
}

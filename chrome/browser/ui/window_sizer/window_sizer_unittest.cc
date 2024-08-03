// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer_common_unittest.h"

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kWindowTilePixels = WindowSizer::kWindowTilePixels;
}

// Test that the window is sized appropriately for the first run experience
// where the default window bounds calculation is invoked.
TEST(WindowSizerTest, DefaultSizeCase) {
  { // 4:3 monitor case, 1024x768, no taskbar
    gfx::Rect window_bounds =
        WindowSizerTestUtil().WithMonitorBounds(p1024x768).GetWindowBounds();
    EXPECT_EQ(gfx::Rect(kWindowTilePixels, kWindowTilePixels,
                        1024 - kWindowTilePixels * 2,
                        768 - kWindowTilePixels * 2),
              window_bounds);
  }

  { // 4:3 monitor case, 1024x768, taskbar on bottom
    gfx::Rect window_bounds = WindowSizerTestUtil()
                                  .WithMonitorBounds(p1024x768)
                                  .WithMonitorWorkArea(taskbar_bottom_work_area)
                                  .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(kWindowTilePixels, kWindowTilePixels,
                        1024 - kWindowTilePixels * 2,
                        (taskbar_bottom_work_area.height() -
                         kWindowTilePixels * 2)),
              window_bounds);
  }

  { // 4:3 monitor case, 1024x768, taskbar on right
    gfx::Rect window_bounds = WindowSizerTestUtil()
                                  .WithMonitorBounds(p1024x768)
                                  .WithMonitorWorkArea(taskbar_right_work_area)
                                  .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(kWindowTilePixels, kWindowTilePixels,
                        taskbar_right_work_area.width() - kWindowTilePixels*2,
                        768 - kWindowTilePixels * 2),
              window_bounds);
  }

  { // 4:3 monitor case, 1024x768, taskbar on left
    gfx::Rect window_bounds = WindowSizerTestUtil()
                                  .WithMonitorBounds(p1024x768)
                                  .WithMonitorWorkArea(taskbar_left_work_area)
                                  .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(taskbar_left_work_area.x() + kWindowTilePixels,
                        kWindowTilePixels,
                        taskbar_left_work_area.width() - kWindowTilePixels * 2,
                        (taskbar_left_work_area.height() -
                         kWindowTilePixels * 2)),
              window_bounds);
  }

  { // 4:3 monitor case, 1024x768, taskbar on top
    gfx::Rect window_bounds = WindowSizerTestUtil()
                                  .WithMonitorBounds(p1024x768)
                                  .WithMonitorWorkArea(taskbar_top_work_area)
                                  .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(kWindowTilePixels,
                        taskbar_top_work_area.y() + kWindowTilePixels,
                        1024 - kWindowTilePixels * 2,
                        taskbar_top_work_area.height() - kWindowTilePixels * 2),
              window_bounds);
  }

  { // 4:3 monitor case, 1280x1024
    gfx::Rect window_bounds =
        WindowSizerTestUtil().WithMonitorBounds(p1280x1024).GetWindowBounds();
    EXPECT_EQ(gfx::Rect(kWindowTilePixels, kWindowTilePixels,
                        WindowSizer::kWindowMaxDefaultWidth,
                        1024 - kWindowTilePixels * 2),
              window_bounds);
  }

  { // 4:3 monitor case, 1600x1200
    gfx::Rect window_bounds =
        WindowSizerTestUtil().WithMonitorBounds(p1600x1200).GetWindowBounds();
    EXPECT_EQ(gfx::Rect(kWindowTilePixels, kWindowTilePixels,
                        WindowSizer::kWindowMaxDefaultWidth,
                        1200 - kWindowTilePixels * 2),
              window_bounds);
  }

  {  // 16:10 monitor case, 1680x1050
     // On all platforms except the Mac, for this size monitor the WindowSizer
     // returns a width that allows side-by-side positioning of two browser
     // windows.
#if BUILDFLAG(IS_MAC)
    const int expected_window_width = WindowSizer::kWindowMaxDefaultWidth;
#else
    const int window_width = 1680;
    const int expected_window_width =
        window_width / 2 - static_cast<int>(kWindowTilePixels * 1.5);
#endif  // BUILDFLAG(IS_MAC)
    gfx::Rect window_bounds =
        WindowSizerTestUtil().WithMonitorBounds(p1680x1050).GetWindowBounds();
    EXPECT_EQ(gfx::Rect(kWindowTilePixels, kWindowTilePixels,
                        expected_window_width, 1050 - kWindowTilePixels * 2),
              window_bounds);
  }

  {  // 16:10 monitor case, 1920x1200
     // On all platforms except the Mac, for this size monitor the WindowSizer
     // returns a width that allows side-by-side positioning of two browser
     // windows.
#if BUILDFLAG(IS_MAC)
    const int expected_window_width = WindowSizer::kWindowMaxDefaultWidth;
#else
    const int window_width = 1920;
    const int expected_window_width =
        window_width / 2 - static_cast<int>(kWindowTilePixels * 1.5);
#endif  // BUILDFLAG(IS_MAC)
    gfx::Rect window_bounds =
        WindowSizerTestUtil().WithMonitorBounds(p1920x1200).GetWindowBounds();
    EXPECT_EQ(gfx::Rect(kWindowTilePixels, kWindowTilePixels,
                        expected_window_width, 1200 - kWindowTilePixels * 2),
              window_bounds);
  }
}

// Test that the next opened window is positioned appropriately given the
// bounds of an existing window of the same type.
TEST(WindowSizerTest, LastWindowBoundsCase) {
  { // normal, in the middle of the screen somewhere.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithLastActiveBounds(
                gfx::Rect(kWindowTilePixels, kWindowTilePixels, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(kWindowTilePixels * 2,
                        kWindowTilePixels * 2, 500, 400), window_bounds);
  }

  { // taskbar on top.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithMonitorWorkArea(taskbar_top_work_area)
            .WithLastActiveBounds(
                gfx::Rect(kWindowTilePixels, kWindowTilePixels, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(kWindowTilePixels * 2,
                        std::max(kWindowTilePixels * 2,
                                 34 /* toolbar height */),
                        500, 400), window_bounds);
  }

  {  // Too small to satisfy the minimum visibility condition.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithLastActiveBounds(
                gfx::Rect(kWindowTilePixels, kWindowTilePixels, 29, 29))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(kWindowTilePixels * 2,
                        kWindowTilePixels * 2,
                        30 /* not 29 */,
                        30 /* not 29 */),
              window_bounds);
  }
}

// Test that the window opened is sized appropriately given persisted sizes.
TEST(WindowSizerTest, PersistedBoundsCase) {
  { // normal, in the middle of the screen somewhere.
    gfx::Rect initial_bounds(kWindowTilePixels, kWindowTilePixels, 500, 400);
    gfx::Rect window_bounds = WindowSizerTestUtil()
                                  .WithMonitorBounds(p1024x768)
                                  .WithPersistedBounds(initial_bounds)
                                  .GetWindowBounds();
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // Normal.
    gfx::Rect initial_bounds(0, 0, 1024, 768);
    gfx::Rect window_bounds = WindowSizerTestUtil()
                                  .WithMonitorBounds(p1024x768)
                                  .WithPersistedBounds(initial_bounds)
                                  .GetWindowBounds();
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // normal, on non-primary monitor in negative coords.
    gfx::Rect initial_bounds(-600, 10, 500, 400);
    gfx::Rect window_bounds = WindowSizerTestUtil()
                                  .WithMonitorBounds(p1024x768, left_s1024x768)
                                  .WithPersistedBounds(initial_bounds)
                                  .GetWindowBounds();
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // normal, on non-primary monitor in negative coords.
    gfx::Rect initial_bounds(-1024, 0, 1024, 768);
    gfx::Rect window_bounds = WindowSizerTestUtil()
                                  .WithMonitorBounds(p1024x768, left_s1024x768)
                                  .WithPersistedBounds(initial_bounds)
                                  .GetWindowBounds();
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  {  // Non-primary monitor resolution has changed, but the monitor still
    // completely contains the window.

    gfx::Rect initial_bounds(1074, 50, 600, 500);
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768, gfx::Rect(1024, 0, 800, 600))
            .WithPersistedBounds(initial_bounds)
            .WithPersistedWorkArea(right_s1024x768)
            .GetWindowBounds();
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  {  // Non-primary monitor resolution has changed, and the window is partially
    // off-screen.

    gfx::Rect initial_bounds(1274, 50, 600, 500);
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768, gfx::Rect(1024, 0, 800, 600))
            .WithPersistedBounds(initial_bounds)
            .WithPersistedWorkArea(right_s1024x768)
            .GetWindowBounds();
    EXPECT_EQ("1224,50 600x500", window_bounds.ToString());
  }

  { // Non-primary monitor resoultion has changed, and the window is now too
    // large for the monitor.

    gfx::Rect initial_bounds(1274, 50, 900, 700);
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768, gfx::Rect(1024, 0, 800, 600))
            .WithPersistedBounds(initial_bounds)
            .WithPersistedWorkArea(right_s1024x768)
            .GetWindowBounds();
    EXPECT_EQ("1024,0 800x600", window_bounds.ToString());
  }

  { // width and height too small
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(
                gfx::Rect(kWindowTilePixels, kWindowTilePixels, 29, 29))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(kWindowTilePixels, kWindowTilePixels,
                        30 /* not 29 */, 30 /* not 29 */),
              window_bounds);
  }

#if BUILDFLAG(IS_MAC)
  { // Saved state is too tall to possibly be resized.  Mac resizers
    // are at the bottom of the window, and no piece of a window can
    // be moved higher than the menubar.  (Perhaps the user changed
    // resolution to something smaller before relaunching Chrome?)
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(
                gfx::Rect(kWindowTilePixels, kWindowTilePixels, 30, 5000))
            .GetWindowBounds();
    EXPECT_EQ(p1024x768.height(), window_bounds.height());
  }
#endif  // BUILDFLAG(IS_MAC)
}

#if !BUILDFLAG(IS_MAC)
// Tests that there is no crash and the window is sized and positioned correctly
// with extremely small work area and bounds.
TEST(WindowSizerTest, TinyWorkAreaAndBounds) {
  {
    const gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(gfx::Rect(0, 0, 40, 30))
            .WithLastActiveBounds(gfx::Rect(30, 20, 20, 15))
            .GetWindowBounds();
    // The window size should be adjusted to satisfy the minimum size and
    // repositioned to be fully visible inside the display.
    EXPECT_EQ(gfx::Rect(10, 0, 30, 30), window_bounds);
  }

  {
    const gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(gfx::Rect(0, 0, 20, 15))
            .WithLastActiveBounds(gfx::Rect(10, 10, 30, 40))
            .GetWindowBounds();
    // The window should be adjusted to fit the entire work area.
    EXPECT_EQ(gfx::Rect(0, 0, 20, 15), window_bounds);
  }
}
#endif  // !BUILDFLAG(IS_MAC)

//////////////////////////////////////////////////////////////////////////////
// The following unittests have different results on Mac/non-Mac because we
// reposition windows aggressively on Mac.  The *WithAggressiveReposition tests
// are run on Mac, and the *WithNonAggressiveRepositioning tests are run on
// other platforms.

#if BUILDFLAG(IS_MAC)
TEST(WindowSizerTest, LastWindowOffscreenWithAggressiveRepositioning) {
  { // taskbar on left.  The new window overlaps slightly with the taskbar, so
    // it is moved to be flush with the left edge of the work area.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithMonitorWorkArea(taskbar_left_work_area)
            .WithLastActiveBounds(
                gfx::Rect(kWindowTilePixels, kWindowTilePixels, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(taskbar_left_work_area.x(),
                        kWindowTilePixels * 2, 500, 400), window_bounds);
  }

  { // offset would put the new window offscreen at the bottom
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithLastActiveBounds(gfx::Rect(10, 729, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(10 + kWindowTilePixels,
                        0 /* not 729 + kWindowTilePixels */,
                        500, 400),
              window_bounds);
  }

  { // offset would put the new window offscreen at the right
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithLastActiveBounds(gfx::Rect(985, 10, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(0 /* not 985 + kWindowTilePixels*/,
                        10 + kWindowTilePixels,
                        500, 400),
              window_bounds);
  }

  { // offset would put the new window offscreen at the bottom right
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithLastActiveBounds(gfx::Rect(985, 729, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(0 /* not 985 + kWindowTilePixels*/,
                        0 /* not 729 + kWindowTilePixels*/,
                        500, 400),
              window_bounds);
  }
}

TEST(WindowSizerTest, PersistedWindowOffscreenWithAggressiveRepositioning) {
  { // off the left
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(-471, 50, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(0 /* not -471 */, 50, 500, 400), window_bounds);
  }

  { // off the top
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(50, -370, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(50, 0, 500, 400), window_bounds);
  }

  { // off the right
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(995, 50, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(0 /* not 995 */, 50, 500, 400), window_bounds);
  }

  { // off the bottom
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(50, 739, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(50, 0 /* not 739 */, 500, 400), window_bounds);
  }

  { // off the topleft
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(-471, -371, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(0 /* not -471 */, 0 /* not -371 */, 500, 400),
              window_bounds);
  }

  { // off the topright
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(995, -371, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(0 /* not 995 */, 0 /* not -371 */, 500, 400),
                        window_bounds);
  }

  { // off the bottomleft
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(-471, 739, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(0 /* not -471 */, 0 /* not 739 */, 500, 400),
                        window_bounds);
  }

  { // off the bottomright
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(995, 739, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(0 /* not 995 */, 0 /* not 739 */, 500, 400),
                        window_bounds);
  }

  { // entirely off left
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(-700, 50, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(0 /* not -700 */, 50, 500, 400), window_bounds);
  }

  { // entirely off left (monitor was detached since last run)
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(-700, 50, 500, 400))
            .WithPersistedWorkArea(left_s1024x768)
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(0, 50, 500, 400), window_bounds);
  }

  { // entirely off top
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(50, -500, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(50, 0, 500, 400), window_bounds);
  }

  { // entirely off top (monitor was detached since last run)
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(50, -500, 500, 400))
            .WithPersistedWorkArea(top_s1024x768)
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(50, 0, 500, 400), window_bounds);
  }

  { // entirely off right
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(1200, 50, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(0 /* not 1200 */, 50, 500, 400), window_bounds);
  }

  { // entirely off right (monitor was detached since last run)
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(1200, 50, 500, 400))
            .WithPersistedWorkArea(right_s1024x768)
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(524 /* not 1200 */, 50, 500, 400), window_bounds);
  }

  { // entirely off bottom
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(50, 800, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(50, 0 /* not 800 */, 500, 400), window_bounds);
  }

  { // entirely off bottom (monitor was detached since last run)
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(50, 800, 500, 400))
            .WithPersistedWorkArea(bottom_s1024x768)
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(50, 368 /* not 800 */, 500, 400), window_bounds);
  }

  { // wider than the screen. off both the left and right
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(-100, 50, 2000, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(0 /* not -100 */, 50, 2000, 400), window_bounds);
  }
}
#else
TEST(WindowSizerTest, LastWindowOffscreenWithNonAggressiveRepositioning) {
  { // taskbar on left.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithMonitorWorkArea(taskbar_left_work_area)
            .WithLastActiveBounds(
                gfx::Rect(kWindowTilePixels, kWindowTilePixels, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(kWindowTilePixels * 2,
                        kWindowTilePixels * 2, 500, 400), window_bounds);
  }

  // Linux does not tile windows, so tile adjustment tests don't make sense.
#if !BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_MAC)
  { // offset would put the new window offscreen at the bottom but the minimum
    // visibility condition is barely satisfied without relocation.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithLastActiveBounds(gfx::Rect(10, 728, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(10 + kWindowTilePixels, 738,
                        500, 400), window_bounds);
  }

  { // offset would put the new window offscreen at the bottom and the minimum
    // visibility condition is satisfied by relocation.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithLastActiveBounds(gfx::Rect(10, 729, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(10 + kWindowTilePixels, 738 /* not 739 */, 500, 400),
              window_bounds);
  }

  { // offset would put the new window offscreen at the right but the minimum
    // visibility condition is barely satisfied without relocation.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithLastActiveBounds(gfx::Rect(984, 10, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(994, 10 + kWindowTilePixels, 500, 400), window_bounds);
  }

  { // offset would put the new window offscreen at the right and the minimum
    // visibility condition is satisfied by relocation.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithLastActiveBounds(gfx::Rect(985, 10, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(994 /* not 995 */, 10 + kWindowTilePixels,
                        500, 400), window_bounds);
  }

  { // offset would put the new window offscreen at the bottom right and the
    // minimum visibility condition is satisfied by relocation.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithLastActiveBounds(gfx::Rect(985, 729, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(994 /* not 995 */, 738 /* not 739 */, 500, 400),
              window_bounds);
  }
#endif  // !BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_MAC)
}

#endif  // BUILDFLAG(IS_MAC)

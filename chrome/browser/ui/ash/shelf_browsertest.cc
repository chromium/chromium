// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

// Class that waits for a change in shelf auto-hide state.
class ShelfAutoHideWaiter : public ShelfLayoutManagerObserver {
 public:
  explicit ShelfAutoHideWaiter(Shelf* shelf) : shelf_(shelf) {
    shelf_->shelf_layout_manager()->AddObserver(this);
  }

  ~ShelfAutoHideWaiter() override {
    shelf_->shelf_layout_manager()->RemoveObserver(this);
  }

  void WaitForState(ShelfAutoHideState expected_state) {
    if (shelf_->GetAutoHideState() == expected_state) {
      return;
    }
    expected_state_ = expected_state;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  // ShelfLayoutManagerObserver:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override {
    if (run_loop_ && new_state == expected_state_) {
      run_loop_->Quit();
    }
  }

  const raw_ptr<Shelf> shelf_;
  ShelfAutoHideState expected_state_ = SHELF_AUTO_HIDE_SHOWN;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class ShelfBrowserTest : public InProcessBrowserTest,
                         public testing::WithParamInterface<bool> {
 public:
  ShelfBrowserTest() = default;
  ShelfBrowserTest(const ShelfBrowserTest&) = delete;
  ShelfBrowserTest& operator=(const ShelfBrowserTest&) = delete;
  ~ShelfBrowserTest() override = default;

  bool IsRTL() { return GetParam(); }

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    if (IsRTL()) {
      command_line->AppendSwitchASCII(::switches::kForceUIDirection, "rtl");
    }
    // The tests assume no windows are open at test startup.
    command_line->AppendSwitch(::switches::kNoStartupWindow);
  }
};

INSTANTIATE_TEST_SUITE_P(Rtl, ShelfBrowserTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(ShelfBrowserTest, AutoHideSmoke) {
  Shelf* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  ASSERT_TRUE(shelf);
  ASSERT_EQ(shelf->auto_hide_behavior(), ShelfAutoHideBehavior::kNever);
  ASSERT_EQ(shelf->GetVisibilityState(), SHELF_VISIBLE);
  ShelfAutoHideWaiter shelf_waiter(shelf);

  // Ensure the mouse is not over the shelf.
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root_window);
  generator.MoveMouseTo(0, 0);

  // Enable auto-hide.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(shelf->GetVisibilityState(), SHELF_AUTO_HIDE);

  // No windows are open, so the shelf is visible.
  shelf_waiter.WaitForState(SHELF_AUTO_HIDE_SHOWN);

  // Open a browser window.
  Browser* browser = CreateBrowser(ProfileManager::GetLastUsedProfile());
  ASSERT_TRUE(browser);

  // The shelf auto-hides.
  shelf_waiter.WaitForState(SHELF_AUTO_HIDE_HIDDEN);

  // Move the mouse to the bottom right corner of the screen.
  generator.MoveMouseTo(root_window->bounds().bottom_right());

  // The shelf shows.
  shelf_waiter.WaitForState(SHELF_AUTO_HIDE_SHOWN);

  // Move the mouse to the top left of the screen again, outside the shelf.
  generator.MoveMouseTo(0, 0);

  // The shelf auto-hides.
  shelf_waiter.WaitForState(SHELF_AUTO_HIDE_HIDDEN);

  // Close the browser window.
  CloseAllBrowsers();

  // The shelf shows.
  shelf_waiter.WaitForState(SHELF_AUTO_HIDE_SHOWN);
}

////////////////////////////////////////////////////////////////////////////////

using ShelfTabletModeBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ShelfTabletModeBrowserTest, AutoHideSmoke) {
  Shelf* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  ASSERT_TRUE(shelf);
  ASSERT_EQ(shelf->auto_hide_behavior(), ShelfAutoHideBehavior::kNever);
  ASSERT_EQ(shelf->GetVisibilityState(), SHELF_VISIBLE);
  ShelfAutoHideWaiter shelf_waiter(shelf);

  // Enable auto-hide.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(shelf->GetVisibilityState(), SHELF_AUTO_HIDE);

  // The test starts with a browser window open, so the shelf auto-hides.
  shelf_waiter.WaitForState(SHELF_AUTO_HIDE_HIDDEN);

  // Enter tablet mode.
  ShellTestApi().SetTabletModeEnabledForTest(true);

  // Swipe up from the bottom-center of the display.
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root_window);
  gfx::Point start_point = root_window->bounds().bottom_center();
  generator.set_current_screen_location(start_point);
  generator.PressMoveAndReleaseTouchBy(0, -80);

  // The shelf shows.
  shelf_waiter.WaitForState(SHELF_AUTO_HIDE_SHOWN);

  // Swipe down from the last point to the bottom of the screen.
  generator.PressMoveAndReleaseTouchBy(0, 80);

  // The shelf hides.
  shelf_waiter.WaitForState(SHELF_AUTO_HIDE_HIDDEN);

  // Close the browser window.
  CloseAllBrowsers();

  // The shelf shows.
  shelf_waiter.WaitForState(SHELF_AUTO_HIDE_SHOWN);
}

////////////////////////////////////////////////////////////////////////////////

class ShelfGuestSessionBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "hash");
    command_line->AppendSwitchASCII(
        switches::kLoginUser, user_manager::GuestAccountId().GetUserEmail());
  }
};

// Tests that in guest session, shelf alignment could be initialized to bottom
// aligned, instead of bottom locked (crbug.com/699661).
IN_PROC_BROWSER_TEST_F(ShelfGuestSessionBrowserTest, ShelfAlignment) {
  // Check the alignment pref for the primary display.
  ShelfAlignment alignment = GetShelfAlignmentPref(
      browser()->profile()->GetPrefs(),
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(ShelfAlignment::kBottom, alignment);

  // Check the locked state, which is not exposed via prefs.
  EXPECT_FALSE(ShelfTestApi().IsAlignmentBottomLocked());
}

}  // namespace
}  // namespace ash

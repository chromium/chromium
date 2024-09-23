// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/test/active_window_waiter.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ash {
namespace {

class WmpBrowserTest : public InProcessBrowserTest {
 public:
  WmpBrowserTest() {
    // No need for a browser window.
    set_launch_browser_for_testing(nullptr);
  }
};

IN_PROC_BROWSER_TEST_F(WmpBrowserTest, DragAndDropWindow) {
  // Ensure the OS Settings app is installed.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  ASSERT_TRUE(profile);
  SystemWebAppManager::GetForTest(profile)->InstallSystemAppsForTesting();

  // Launch the OS Settings app.
  ActiveWindowWaiter window_waiter(Shell::GetPrimaryRootWindow());
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile);
  aura::Window* window = window_waiter.Wait();
  ASSERT_TRUE(window);

  // Starting in the window caption area, drag down and to the right.
  gfx::Rect original_bounds = window->GetBoundsInScreen();
  gfx::Point start = original_bounds.top_center() + gfx::Vector2d(0, 10);
  const gfx::Vector2d kDragOffset(50, 50);
  gfx::Point end = start + kDragOffset;

  // Drag the window with the mouse, using several mouse move steps to better
  // simulate production.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseTo(start);
  generator.PressLeftButton();
  generator.MoveMouseTo(end, /*count=*/5);
  generator.ReleaseLeftButton();

  // Window bounds should have changed by the offset of the drag.
  gfx::Rect new_bounds = window->GetBoundsInScreen();
  gfx::Rect expected_bounds = original_bounds + kDragOffset;
  EXPECT_EQ(new_bounds, expected_bounds);
}

}  // namespace
}  // namespace ash

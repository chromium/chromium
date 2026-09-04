// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/test/active_window_waiter.h"
#include "ash/wm/overview/overview_item_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "base/check_deref.h"
#include "base/test/run_until.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"
#include "chromeos/ash/experiences/settings_ui/settings_app_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/aura/window.h"
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
  ash::SettingsAppManager::Get()->Open(
      CHECK_DEREF(
          ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile)),
      {});
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

using OverviewWindowPropertiesBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(OverviewWindowPropertiesBrowserTest,
                       OccludedWindowTransformDoesNotUpdateScreenRects) {
  ash::BrowserController* controller = ash::BrowserController::GetInstance();
  ASSERT_TRUE(controller);
  ash::BrowserDelegate* delegate = controller->GetLastUsedBrowser();
  ASSERT_TRUE(delegate);
  aura::Window* native_window = delegate->GetNativeWindow();
  ASSERT_TRUE(native_window);
  content::WebContents* web_contents = delegate->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  native_window->SetBounds(gfx::Rect(100, 100, 200, 200));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_contents, "window.screenX").ExtractInt() ==
               100 &&
           content::EvalJs(web_contents, "window.screenY").ExtractInt() == 100;
  }));
  int initial_screen_x =
      content::EvalJs(web_contents, "window.screenX").ExtractInt();
  int initial_screen_y =
      content::EvalJs(web_contents, "window.screenY").ExtractInt();

  // Open a new browser window that completely covers the first window.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  ASSERT_TRUE(profile);
  BrowserWindowInterface* covering_browser = CreateBrowser(profile);
  ASSERT_TRUE(covering_browser);
  ash::BrowserDelegate* covering_delegate = controller->GetLastUsedBrowser();
  ASSERT_TRUE(covering_delegate);
  ASSERT_NE(delegate, covering_delegate);
  aura::Window* covering_window = covering_delegate->GetNativeWindow();
  ASSERT_TRUE(covering_window);

  covering_window->SetBounds(gfx::Rect(50, 50, 300, 300));
  // Make sure the original window position was not rearranged.
  native_window->SetBounds(gfx::Rect(100, 100, 200, 200));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_contents, "window.screenX").ExtractInt() ==
               100 &&
           content::EvalJs(web_contents, "window.screenY").ExtractInt() == 100;
  }));

  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  int overview_screen_x =
      content::EvalJs(web_contents, "window.screenX").ExtractInt();
  int overview_screen_y =
      content::EvalJs(web_contents, "window.screenY").ExtractInt();
  EXPECT_EQ(initial_screen_x, overview_screen_x);
  EXPECT_EQ(initial_screen_y, overview_screen_y);

  ash::OverviewItemBase* item = ash::GetOverviewItemForWindow(native_window);
  ASSERT_TRUE(item);
  gfx::Point center =
      gfx::ToRoundedPoint(item->GetTransformedBounds().CenterPoint());
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  generator.MoveMouseTo(center);
  generator.ClickLeftButton();
  ash::WaitForOverviewExitAnimation();

  int final_screen_x =
      content::EvalJs(web_contents, "window.screenX").ExtractInt();
  int final_screen_y =
      content::EvalJs(web_contents, "window.screenY").ExtractInt();
  EXPECT_EQ(initial_screen_x, final_screen_x);
  EXPECT_EQ(initial_screen_y, final_screen_y);
}

}  // namespace
}  // namespace ash

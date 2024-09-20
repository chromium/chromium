// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/float_controller.h"

#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/float/float_test_api.h"
#include "ash/wm/window_state.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_chromeos.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#include "chrome/test/base/ash/util/ash_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace {

// Tuck a window to the bottom right corner by generating a fling.
void TuckWindow(aura::Window* window) {
  auto* frame_view = static_cast<BrowserNonClientFrameViewChromeOS*>(
      views::Widget::GetWidgetForNativeView(window)
          ->non_client_view()
          ->frame_view());
  const gfx::Point start =
      frame_view->GetBoundsInScreen().top_center() + gfx::Vector2d(0, 10);
  const gfx::Vector2d offset(10, 10);
  ui::test::EventGenerator event_generator(window->GetRootWindow());
  event_generator.GestureTapAt(start);
  event_generator.GestureScrollSequence(start, start + offset,
                                        base::Milliseconds(10), /*steps=*/1);
}

}  // namespace

using FloatControllerBrowserTest = InProcessBrowserTest;

// Tests that repeated tucking of a floated window in tablet mode does not cause
// the window to freeze. Regression test for b/278917878.
IN_PROC_BROWSER_TEST_F(FloatControllerBrowserTest,
                       TuckingBrowserDoesNotFreezeWindow) {
  ash::test::InstallSystemAppsForTesting(browser()->profile());

  // Open two SWAs. The bug was a result of the window targeters installed by
  // the window tucker and immersive mode not being reinstalled in the correct
  // order. More details in b/278917878.
  ash::test::CreateSystemWebApp(browser()->profile(),
                                ash::SystemWebAppType::FILE_MANAGER);
  aura::Window* browser_window1 =
      BrowserList::GetInstance()->GetLastActive()->window()->GetNativeWindow();

  ash::test::CreateSystemWebApp(browser()->profile(),
                                ash::SystemWebAppType::SETTINGS);
  aura::Window* browser_window2 =
      BrowserList::GetInstance()->GetLastActive()->window()->GetNativeWindow();

  ASSERT_NE(browser()->window()->GetNativeWindow(), browser_window1);
  ASSERT_NE(browser()->window()->GetNativeWindow(), browser_window2);
  ASSERT_NE(browser_window1, browser_window2);

  auto* float_controller = ash::Shell::Get()->float_controller();

  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  // Float and then tuck the background window repeatedly. This emulates the
  // steps listed in the bug.
  ui::test::EventGenerator event_generator(browser_window1->GetRootWindow());
  event_generator.PressAndReleaseKeyAndModifierKeys(
      ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(ash::WindowState::Get(browser_window2)->IsFloated());
  TuckWindow(browser_window2);
  ash::ShellTestApi().WaitForWindowFinishAnimating(browser_window2);
  ASSERT_TRUE(
      float_controller->IsFloatedWindowTuckedForTablet(browser_window2));

  // Float `browser_window1` using accelerator and tuck it.
  wm::ActivateWindow(browser_window1);
  event_generator.PressAndReleaseKeyAndModifierKeys(
      ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(ash::WindowState::Get(browser_window1)->IsFloated());
  ASSERT_TRUE(ash::WindowState::Get(browser_window2)->IsMaximized());
  TuckWindow(browser_window1);
  ash::ShellTestApi().WaitForWindowFinishAnimating(browser_window2);
  ASSERT_TRUE(
      float_controller->IsFloatedWindowTuckedForTablet(browser_window1));

  // Float `browser_window2` using accelerator and tuck it. At each point,
  // `TuckWindow` should tuck the window otherwise the window has frozen and the
  // test will hang.
  wm::ActivateWindow(browser_window2);
  event_generator.PressAndReleaseKeyAndModifierKeys(
      ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(ash::WindowState::Get(browser_window2)->IsFloated());
  ASSERT_TRUE(ash::WindowState::Get(browser_window1)->IsMaximized());
  TuckWindow(browser_window2);
  ash::ShellTestApi().WaitForWindowFinishAnimating(browser_window2);
  ASSERT_TRUE(
      float_controller->IsFloatedWindowTuckedForTablet(browser_window2));
}

// Tests that flinging down from the client area to move a floated window does
// not open the web ui tab strip.
IN_PROC_BROWSER_TEST_F(FloatControllerBrowserTest,
                       FlingDownDoesNotOpenTabStrip) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  // A floated window is magnetized to the bottom right by default.
  aura::Window* window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(window->GetRootWindow(), window);
  event_generator.PressAndReleaseKeyAndModifierKeys(
      ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(ash::WindowState::Get(window)->IsFloated());
  ASSERT_EQ(ash::FloatController::MagnetismCorner::kBottomRight,
            ash::FloatTestApi::GetMagnetismCornerForBounds(
                window->GetBoundsInScreen()));

  // Drag the window up to the top right.
  auto get_draggable_point = [](aura::Window* window) {
    return gfx::Point(window->GetBoundsInScreen().CenterPoint().x(),
                      window->GetBoundsInScreen().y() + 10);
  };
  event_generator.set_current_screen_location(get_draggable_point(window));
  event_generator.PressMoveAndReleaseTouchBy(0, -200);
  ASSERT_EQ(ash::FloatController::MagnetismCorner::kTopRight,
            ash::FloatTestApi::GetMagnetismCornerForBounds(
                window->GetBoundsInScreen()));

  // Drag the window back down. Test that it doesn't open the tab strip.
  event_generator.set_current_screen_location(get_draggable_point(window));
  event_generator.PressMoveAndReleaseTouchBy(0, 200);
  ASSERT_EQ(ash::FloatController::MagnetismCorner::kBottomRight,
            ash::FloatTestApi::GetMagnetismCornerForBounds(
                window->GetBoundsInScreen()));
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  EXPECT_FALSE(browser_view->webui_tab_strip()->GetVisible());
}

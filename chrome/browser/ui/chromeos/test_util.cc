// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/test_util.h"

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "ash/public/cpp/split_view_test_api.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_pin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_chromeos.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/immersive_mode_tester.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ui/base/window_properties.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

void ChromeOSBrowserUITest::SetUpDefaultCommandLine(
    base::CommandLine* command_line) {
  MixinBasedInProcessBrowserTest::SetUpDefaultCommandLine(command_line);
  command_line->AppendSwitch(ash::switches::kAshEnableTabletMode);
}

void ChromeOSBrowserUITest::TearDownOnMainThread() {
  if (InTabletMode()) {
    ExitTabletMode();
  }
  MixinBasedInProcessBrowserTest::TearDownOnMainThread();
}

bool ChromeOSBrowserUITest::InTabletMode() {
  return display::Screen::GetScreen()->InTabletMode();
}

void ChromeOSBrowserUITest::EnterTabletMode() {
  SetTabletMode(true);
}

void ChromeOSBrowserUITest::ExitTabletMode() {
  SetTabletMode(false);
}

void ChromeOSBrowserUITest::SetTabletMode(bool enable) {
  CHECK_NE(InTabletMode(), enable);
  if (enable) {
    ash::TabletModeControllerTestApi().EnterTabletMode();
  } else {
    ash::TabletModeControllerTestApi().LeaveTabletMode();
  }
  CHECK_EQ(InTabletMode(), enable);
}

void ChromeOSBrowserUITest::EnterOverviewMode() {
  SetOverviewMode(true);
}

void ChromeOSBrowserUITest::ExitOverviewMode() {
  SetOverviewMode(false);
}

void ChromeOSBrowserUITest::SetOverviewMode(bool enable) {
  if (enable) {
    ash::Shell::Get()->overview_controller()->StartOverview(
        ash::OverviewStartAction::kTests);
  } else {
    ash::Shell::Get()->overview_controller()->EndOverview(
        ash::OverviewEndAction::kTests);
  }
}

bool ChromeOSBrowserUITest::IsSnapWindowSupported() {
  return true;
}

void ChromeOSBrowserUITest::SnapWindow(aura::Window* window,
                                       ash::SnapPosition position) {
  CHECK(IsSnapWindowSupported());
  ash::SplitViewTestApi().SnapWindow(window, position);
}

void ChromeOSBrowserUITest::PinWindow(aura::Window* window, bool trusted) {
  ::PinWindow(window, trusted);
}

bool ChromeOSBrowserUITest::IsIsShelfVisibleSupported() {
  return true;
}

bool ChromeOSBrowserUITest::IsShelfVisible() {
  CHECK(IsIsShelfVisibleSupported());
  return ash::ShelfTestApi().IsVisible();
}

void ChromeOSBrowserUITest::DeactivateWidget(views::Widget* widget) {
  widget->Deactivate();
}

void ChromeOSBrowserUITest::EnterImmersiveFullscreenMode(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  ASSERT_FALSE(browser_view->IsFullscreen());

  ImmersiveModeController* immersive_mode_controller =
      browser_view->immersive_mode_controller();
  ASSERT_FALSE(immersive_mode_controller->IsEnabled());

  ui_test_utils::ToggleFullscreenModeAndWait(browser);
  // TODO(crbug.com/40942067): Simplify waiting once the two states are merged.
  ImmersiveModeTester(browser).WaitForFullscreenToEnter();
  ASSERT_TRUE(immersive_mode_controller->IsEnabled());
  ASSERT_TRUE(browser_view->IsFullscreen());
}

void ChromeOSBrowserUITest::ExitImmersiveFullscreenMode(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  ASSERT_TRUE(browser_view->IsFullscreen());

  ImmersiveModeController* immersive_mode_controller =
      browser_view->immersive_mode_controller();
  ASSERT_TRUE(immersive_mode_controller->IsEnabled());

  ui_test_utils::ToggleFullscreenModeAndWait(browser);
  // TODO(crbug.com/40942067): Simplify waiting once the two states are merged.
  ImmersiveModeTester(browser).WaitForFullscreenToExit();
  ASSERT_FALSE(immersive_mode_controller->IsEnabled());
  ASSERT_FALSE(browser_view->IsFullscreen());
}

void ChromeOSBrowserUITest::EnterTabFullscreenMode(
    Browser* browser,
    content::WebContents* web_contents) {
  ui_test_utils::FullscreenWaiter waiter(browser, {.tab_fullscreen = true});
  static_cast<content::WebContentsDelegate*>(browser)
      ->EnterFullscreenModeForTab(web_contents->GetPrimaryMainFrame(), {});
  waiter.Wait();
}

void ChromeOSBrowserUITest::ExitTabFullscreenMode(
    Browser* browser,
    content::WebContents* web_contents) {
  ui_test_utils::FullscreenWaiter waiter(browser, {.tab_fullscreen = false});
  browser->exclusive_access_manager()
      ->fullscreen_controller()
      ->ExitFullscreenModeForTab(web_contents);
  waiter.Wait();
}

BrowserNonClientFrameViewChromeOS* ChromeOSBrowserUITest::GetFrameViewChromeOS(
    BrowserView* browser_view) {
  // We know we're using ChromeOS, so static cast.
  auto* frame_view = static_cast<BrowserNonClientFrameViewChromeOS*>(
      browser_view->GetWidget()->non_client_view()->frame_view());
  DCHECK(frame_view);
  return frame_view;
}

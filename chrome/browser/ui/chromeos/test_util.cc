// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/test_util.h"

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "ash/public/cpp/split_view_test_api.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "chrome/browser/ash/crosapi/test_controller_ash.h"
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "chrome/browser/ui/lacros/window_properties.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/lacros/lacros_service.h"
#include "ui/display/display_observer.h"
#endif

namespace {
#if BUILDFLAG(IS_CHROMEOS_LACROS)

bool IsCrosApiSupported(uint32_t min_version) {
  return chromeos::LacrosService::Get()
             ->GetInterfaceVersion<crosapi::mojom::TestController>() >=
         static_cast<int>(min_version);
}

chromeos::WindowStateType WindowStateTypeFromSnapPosition(
    crosapi::mojom::SnapPosition position) {
  switch (position) {
    case crosapi::mojom::SnapPosition::kPrimary:
      return chromeos::WindowStateType::kPrimarySnapped;
    case crosapi::mojom::SnapPosition::kSecondary:
      return chromeos::WindowStateType::kSecondarySnapped;
  }
}

// Runs the specified callback when a change to tablet state is detected.
// TODO(b/323790202): Make this more robust.
class TabletModeWatcher : public display::DisplayObserver {
 public:
  explicit TabletModeWatcher(base::RepeatingClosure cb,
                             display::TabletState current_tablet_state)
      : cb_(cb), current_tablet_state_(current_tablet_state) {}
  void OnDisplayTabletStateChanged(display::TabletState state) override {
    // Skip if the notified TabletState is same as the current state.
    // This required since it may notify the current tablet state when the
    // observer is added (e.g. WaylandScreen::AddObserver()). In such cases, we
    // need to ignore the initial notification so that we can only catch
    // meaningful notifications for testing.
    if (current_tablet_state_ == state) {
      return;
    }

    cb_.Run();
  }

 private:
  base::RepeatingClosure cb_;
  display::TabletState current_tablet_state_;
};

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}  // namespace

void ChromeOSBrowserUITest::SetUpDefaultCommandLine(
    base::CommandLine* command_line) {
  MixinBasedInProcessBrowserTest::SetUpDefaultCommandLine(command_line);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  command_line->AppendSwitch(ash::switches::kAshEnableTabletMode);
#endif
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (enable) {
    ash::TabletModeControllerTestApi().EnterTabletMode();
  } else {
    ash::TabletModeControllerTestApi().LeaveTabletMode();
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  base::RunLoop run_loop;
  TabletModeWatcher watcher(run_loop.QuitClosure(),
                            display::Screen::GetScreen()->GetTabletState());
  display::Screen::GetScreen()->AddObserver(&watcher);
  auto& test_controller = chromeos::LacrosService::Get()
                              ->GetRemote<crosapi::mojom::TestController>();
  auto waiter =
      crosapi::mojom::TestControllerAsyncWaiter(test_controller.get());
  if (enable) {
    waiter.EnterTabletMode();
  } else {
    waiter.ExitTabletMode();
  }
  run_loop.Run();
  display::Screen::GetScreen()->RemoveObserver(&watcher);
#endif
  CHECK_EQ(InTabletMode(), enable);
}

void ChromeOSBrowserUITest::EnterOverviewMode() {
  SetOverviewMode(true);
}

void ChromeOSBrowserUITest::ExitOverviewMode() {
  SetOverviewMode(false);
}

void ChromeOSBrowserUITest::SetOverviewMode(bool enable) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (enable) {
    ash::Shell::Get()->overview_controller()->StartOverview(
        ash::OverviewStartAction::kTests);
  } else {
    ash::Shell::Get()->overview_controller()->EndOverview(
        ash::OverviewEndAction::kTests);
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  auto& test_controller = chromeos::LacrosService::Get()
                              ->GetRemote<crosapi::mojom::TestController>();
  auto waiter =
      crosapi::mojom::TestControllerAsyncWaiter(test_controller.get());
  if (enable) {
    waiter.EnterOverviewMode();
  } else {
    waiter.ExitOverviewMode();
  }
#endif
}

bool ChromeOSBrowserUITest::IsSnapWindowSupported() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else
  return IsCrosApiSupported(
      crosapi::mojom::TestController::MethodMinVersions::kSnapWindowMinVersion);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ChromeOSBrowserUITest::SnapWindow(aura::Window* window,
                                       crosapi::mojom::SnapPosition position) {
  CHECK(IsSnapWindowSupported());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::SplitViewTestApi().SnapWindow(
      window, mojo::ConvertTo<ash::SnapPosition>(position));
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
  auto& test_controller = chromeos::LacrosService::Get()
                              ->GetRemote<crosapi::mojom::TestController>();
  crosapi::mojom::TestControllerAsyncWaiter(test_controller.get())
      .SnapWindow(lacros_window_utility::GetRootWindowUniqueId(window),
                  position);
  auto expected_state = WindowStateTypeFromSnapPosition(position);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return window->GetProperty(chromeos::kWindowStateTypeKey) == expected_state;
  }));
#endif
}

void ChromeOSBrowserUITest::PinWindow(aura::Window* window, bool trusted) {
  ::PinWindow(window, trusted);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto expected_type = trusted ? chromeos::WindowPinType::kTrustedPinned
                               : chromeos::WindowPinType::kPinned;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return window->GetProperty(lacros::kWindowPinTypeKey) == expected_type;
  }));
#endif
}

bool ChromeOSBrowserUITest::IsIsShelfVisibleSupported() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
  return IsCrosApiSupported(crosapi::mojom::TestController::MethodMinVersions::
                                kIsShelfVisibleMinVersion);
#endif
}

bool ChromeOSBrowserUITest::IsShelfVisible() {
  CHECK(IsIsShelfVisibleSupported());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::ShelfTestApi().IsVisible();
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
  auto& test_controller = chromeos::LacrosService::Get()
                              ->GetRemote<crosapi::mojom::TestController>();
  return crosapi::mojom::TestControllerAsyncWaiter(test_controller.get())
      .IsShelfVisible();
#endif
}

void ChromeOSBrowserUITest::DeactivateWidget(views::Widget* widget) {
  widget->Deactivate();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  views::test::WaitForWidgetActive(widget, false);
#endif
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

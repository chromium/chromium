// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include "ash/public/cpp/test/shell_test_api.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ui/base/window_properties.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using testing::Combine;
using testing::Values;
using testing::WithParamInterface;

namespace {

bool IsInImmersive(aura::Window* window) {
  return window->GetProperty(chromeos::kImmersiveIsActive);
}

}  // namespace

class AcceleratorCommandsFullscreenBrowserTest
    : public WithParamInterface<ui::mojom::WindowShowState>,
      public InProcessBrowserTest {
 public:
  AcceleratorCommandsFullscreenBrowserTest()
      : initial_show_state_(GetParam()) {}

  AcceleratorCommandsFullscreenBrowserTest(
      const AcceleratorCommandsFullscreenBrowserTest&) = delete;
  AcceleratorCommandsFullscreenBrowserTest& operator=(
      const AcceleratorCommandsFullscreenBrowserTest&) = delete;

  virtual ~AcceleratorCommandsFullscreenBrowserTest() {}

  // Sets |widget|'s show state to |initial_show_state_|.
  void SetToInitialShowState(views::Widget* widget) {
    if (initial_show_state_ == ui::mojom::WindowShowState::kMaximized) {
      widget->Maximize();
    } else {
      widget->Restore();
    }
  }

  // Returns true if |widget|'s show state is |initial_show_state_|.
  bool IsInitialShowState(const views::Widget* widget) const {
    if (initial_show_state_ == ui::mojom::WindowShowState::kMaximized) {
      return widget->IsMaximized();
    } else {
      return !widget->IsMaximized() && !widget->IsFullscreen() &&
             !widget->IsMinimized();
    }
  }

 private:
  ui::mojom::WindowShowState initial_show_state_;
};

// Test that toggling window fullscreen works properly.
IN_PROC_BROWSER_TEST_P(AcceleratorCommandsFullscreenBrowserTest,
                       ToggleFullscreen) {
  // 1) Browser windows.
  aura::Window* window = browser()->window()->GetNativeWindow();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  ASSERT_TRUE(browser()->is_type_normal());
  ASSERT_TRUE(widget->IsActive());
  SetToInitialShowState(widget);
  EXPECT_TRUE(IsInitialShowState(widget));

  ash::ShellTestApi().ToggleFullscreen();
  EXPECT_TRUE(IsInImmersive(window));

  ash::ShellTestApi().ToggleFullscreen();
  EXPECT_FALSE(IsInImmersive(window));
  EXPECT_TRUE(IsInitialShowState(widget));

  // 2) ash::ShellTestApi().ToggleFullscreen() should have no effect on windows
  // which cannot be maximized.
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorNone);
  ash::ShellTestApi().ToggleFullscreen();
  EXPECT_TRUE(IsInitialShowState(widget));

  // 3) Hosted apps.
  Browser::CreateParams browser_create_params(
      Browser::CreateParams::CreateForApp("Test", true /* trusted_source */,
                                          gfx::Rect(), browser()->profile(),
                                          true));

  Browser* app_host_browser = Browser::Create(browser_create_params);
  ASSERT_FALSE(app_host_browser->is_type_popup());
  ASSERT_TRUE(app_host_browser->is_type_app());
  AddBlankTabAndShow(app_host_browser);
  window = app_host_browser->window()->GetNativeWindow();
  widget = views::Widget::GetWidgetForNativeWindow(window);
  ASSERT_TRUE(widget->IsActive());
  SetToInitialShowState(widget);
  EXPECT_TRUE(IsInitialShowState(widget));

  ash::ShellTestApi().ToggleFullscreen();
  EXPECT_TRUE(IsInImmersive(window));

  ash::ShellTestApi().ToggleFullscreen();
  EXPECT_FALSE(IsInImmersive(window));
  EXPECT_TRUE(IsInitialShowState(widget));

  // 4) Popup browser windows.
  browser_create_params =
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true);
  Browser* popup_browser = Browser::Create(browser_create_params);
  ASSERT_TRUE(popup_browser->is_type_popup());
  ASSERT_FALSE(popup_browser->is_type_app());
  AddBlankTabAndShow(popup_browser);
  window = popup_browser->window()->GetNativeWindow();
  widget = views::Widget::GetWidgetForNativeWindow(window);
  ASSERT_TRUE(widget->IsActive());
  SetToInitialShowState(widget);
  EXPECT_TRUE(IsInitialShowState(widget));

  ash::ShellTestApi().ToggleFullscreen();
  EXPECT_TRUE(IsInImmersive(window));

  ash::ShellTestApi().ToggleFullscreen();
  EXPECT_FALSE(IsInImmersive(window));
  EXPECT_TRUE(IsInitialShowState(widget));

  // 5) Miscellaneous windows (e.g. task manager).
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  params.delegate = new views::WidgetDelegateView;
  params.delegate->SetCanMaximize(true);
  params.delegate->SetCanFullscreen(true);
  views::Widget misc_widget;
  widget = &misc_widget;
  widget->Init(std::move(params));
  widget->Show();
  window = widget->GetNativeWindow();

  ASSERT_TRUE(widget->IsActive());
  SetToInitialShowState(widget);
  EXPECT_TRUE(IsInitialShowState(widget));

  ash::ShellTestApi().ToggleFullscreen();
  EXPECT_TRUE(IsInImmersive(window));

  ash::ShellTestApi().ToggleFullscreen();
  EXPECT_FALSE(IsInImmersive(window));
  EXPECT_TRUE(IsInitialShowState(widget));

  // TODO(pkotwicz|oshima): Make toggling fullscreen restore the window to its
  // show state prior to entering fullscreen.
  EXPECT_FALSE(widget->IsFullscreen());
}

INSTANTIATE_TEST_SUITE_P(InitiallyRestored,
                         AcceleratorCommandsFullscreenBrowserTest,
                         Values(ui::mojom::WindowShowState::kNormal));
INSTANTIATE_TEST_SUITE_P(InitiallyMaximized,
                         AcceleratorCommandsFullscreenBrowserTest,
                         Values(ui::mojom::WindowShowState::kMaximized));

class AcceleratorCommandsPlatformAppFullscreenBrowserTest
    : public WithParamInterface<ui::mojom::WindowShowState>,
      public extensions::PlatformAppBrowserTest {
 public:
  AcceleratorCommandsPlatformAppFullscreenBrowserTest()
      : initial_show_state_(GetParam()) {}

  AcceleratorCommandsPlatformAppFullscreenBrowserTest(
      const AcceleratorCommandsPlatformAppFullscreenBrowserTest&) = delete;
  AcceleratorCommandsPlatformAppFullscreenBrowserTest& operator=(
      const AcceleratorCommandsPlatformAppFullscreenBrowserTest&) = delete;

  virtual ~AcceleratorCommandsPlatformAppFullscreenBrowserTest() {}

  // Sets |app_window|'s show state to |initial_show_state_|.
  void SetToInitialShowState(extensions::AppWindow* app_window) {
    if (initial_show_state_ == ui::mojom::WindowShowState::kMaximized) {
      app_window->Maximize();
    } else {
      app_window->Restore();
    }
  }

  // Returns true if |app_window|'s show state is |initial_show_state_|.
  bool IsInitialShowState(extensions::AppWindow* app_window) const {
    if (initial_show_state_ == ui::mojom::WindowShowState::kMaximized) {
      return app_window->GetBaseWindow()->IsMaximized();
    } else {
      return ui::BaseWindow::IsRestored(*app_window->GetBaseWindow());
    }
  }

 private:
  ui::mojom::WindowShowState initial_show_state_;
};

// Test the behavior of platform apps when ToggleFullscreen() is called.
IN_PROC_BROWSER_TEST_P(AcceleratorCommandsPlatformAppFullscreenBrowserTest,
                       ToggleFullscreen) {
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("minimal", "Launched");

  {
    // Test that ToggleFullscreen() toggles a platform's app's fullscreen
    // state and that it additionally puts the app into immersive fullscreen
    // if put_all_windows_in_immersive() returns true.
    extensions::AppWindow::CreateParams params;
    params.frame = extensions::AppWindow::FRAME_CHROME;
    extensions::AppWindow* app_window =
        CreateAppWindowFromParams(browser()->profile(), extension, params);
    extensions::NativeAppWindow* native_app_window =
        app_window->GetBaseWindow();
    SetToInitialShowState(app_window);
    ASSERT_TRUE(app_window->GetBaseWindow()->IsActive());
    EXPECT_TRUE(IsInitialShowState(app_window));

    ash::ShellTestApi().ToggleFullscreen();
    EXPECT_TRUE(native_app_window->IsFullscreen());
    EXPECT_TRUE(IsInImmersive(native_app_window->GetNativeWindow()));

    ash::ShellTestApi().ToggleFullscreen();
    EXPECT_FALSE(native_app_window->IsFullscreen());
    EXPECT_TRUE(IsInitialShowState(app_window));

    CloseAppWindow(app_window);
  }

  {
    // Repeat the test, but make sure that frameless platform apps are never put
    // into immersive fullscreen.
    extensions::AppWindow::CreateParams params;
    params.frame = extensions::AppWindow::FRAME_NONE;
    extensions::AppWindow* app_window =
        CreateAppWindowFromParams(browser()->profile(), extension, params);
    extensions::NativeAppWindow* native_app_window =
        app_window->GetBaseWindow();
    ASSERT_TRUE(app_window->GetBaseWindow()->IsActive());
    SetToInitialShowState(app_window);
    EXPECT_TRUE(IsInitialShowState(app_window));

    ash::ShellTestApi().ToggleFullscreen();
    EXPECT_TRUE(native_app_window->IsFullscreen());
    EXPECT_FALSE(IsInImmersive(native_app_window->GetNativeWindow()));

    ash::ShellTestApi().ToggleFullscreen();
    EXPECT_FALSE(native_app_window->IsFullscreen());
    EXPECT_TRUE(IsInitialShowState(app_window));

    CloseAppWindow(app_window);
  }
}

INSTANTIATE_TEST_SUITE_P(InitiallyRestored,
                         AcceleratorCommandsPlatformAppFullscreenBrowserTest,
                         Values(ui::mojom::WindowShowState::kNormal));
INSTANTIATE_TEST_SUITE_P(InitiallyMaximized,
                         AcceleratorCommandsPlatformAppFullscreenBrowserTest,
                         Values(ui::mojom::WindowShowState::kMaximized));

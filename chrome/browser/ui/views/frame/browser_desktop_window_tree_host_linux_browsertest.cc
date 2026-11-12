// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_linux.h"

#include "base/command_line.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/platform_window/platform_window.h"

namespace {

class BrowserDesktopWindowTreeHostLinuxTest : public InProcessBrowserTest {
 public:
  BrowserDesktopWindowTreeHostLinuxTest() = default;
  BrowserDesktopWindowTreeHostLinuxTest(
      const BrowserDesktopWindowTreeHostLinuxTest&) = delete;
  BrowserDesktopWindowTreeHostLinuxTest& operator=(
      const BrowserDesktopWindowTreeHostLinuxTest&) = delete;
  ~BrowserDesktopWindowTreeHostLinuxTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    // TODO(crbug.com/574355652) This cannot be reliably tested on X11 because
    // of the browser not being notified about window minimization.
    command_line->AppendSwitchNative(::switches::kOzonePlatform,
                                     ::switches::kHeadless);
    command_line->AppendSwitch(::switches::kHeadless);
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  ui::PlatformWindow* platform_window() {
    auto* host = static_cast<aura::WindowTreeHostPlatform*>(
        browser_view()->GetWidget()->GetNativeWindow()->GetHost());
    return host->platform_window();
  }

  ui::PlatformWindowDelegate* GetPlatformWindowDelegate() {
    return views::DesktopWindowTreeHostPlatform::GetHostForWidget(
        browser_view()
            ->GetWidget()
            ->GetNativeWindow()
            ->GetHost()
            ->GetAcceleratedWidget());
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BrowserDesktopWindowTreeHostLinuxTest,
                       TabFullscreenMinimizeAndRestore) {
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  FullscreenController* fullscreen_controller =
      ExclusiveAccessManager::From(browser())->fullscreen_controller();

  ui_test_utils::FullscreenWaiter waiter(
      browser(), {.browser_fullscreen = false, .tab_fullscreen = true});
  fullscreen_controller->EnterFullscreenModeForTab(
      web_contents->GetPrimaryMainFrame());
  waiter.Wait();

  EXPECT_TRUE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_TRUE(fullscreen_controller->IsTabFullscreen());
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());

  browser_view()->GetWidget()->Minimize();
  EXPECT_TRUE(browser_view()->GetWidget()->IsMinimized());
  EXPECT_TRUE(fullscreen_controller->IsTabFullscreen());
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());

  // Restore the window to fullscreen.
  platform_window()->SetFullscreen(true, display::kInvalidDisplayId);
  EXPECT_TRUE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_TRUE(fullscreen_controller->IsTabFullscreen());
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());

  ui_test_utils::FullscreenWaiter exit_waiter(
      browser(), {.browser_fullscreen = false, .tab_fullscreen = false});
  fullscreen_controller->ExitFullscreenModeForTab(web_contents);
  exit_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(BrowserDesktopWindowTreeHostLinuxTest,
                       TabFullscreenMinimizeAndUnminimizeToNormal) {
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  FullscreenController* fullscreen_controller =
      ExclusiveAccessManager::From(browser())->fullscreen_controller();

  ui_test_utils::FullscreenWaiter waiter(
      browser(), {.browser_fullscreen = false, .tab_fullscreen = true});
  fullscreen_controller->EnterFullscreenModeForTab(
      web_contents->GetPrimaryMainFrame());
  waiter.Wait();

  EXPECT_TRUE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_TRUE(fullscreen_controller->IsTabFullscreen());

  browser_view()->GetWidget()->Minimize();
  EXPECT_TRUE(browser_view()->GetWidget()->IsMinimized());
  EXPECT_TRUE(fullscreen_controller->IsTabFullscreen());

  // Unminimize to normal show state instead of fullscreen.
  browser_view()->GetWidget()->Restore();
  EXPECT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
}

// Maximized and fullscreen Picture-in-Picture windows have no frame border
// (https://crbug.com/573760545). Some compositors (e.g. Hyprland) maximize
// every window when it maps.
IN_PROC_BROWSER_TEST_F(BrowserDesktopWindowTreeHostLinuxTest,
                       PictureInPictureMaximizedHasNoFrameBorder) {
  BrowserWindowInterface* pip_browser =
      CreateBrowserWindow(BrowserWindowCreateParams::CreateForPictureInPicture(
          "", /*trusted_source=*/false, browser()->GetProfile(),
          /*user_gesture=*/true));
  pip_browser->GetWindow()->Show();
  BrowserView* pip_browser_view =
      BrowserView::GetBrowserViewForBrowser(pip_browser);
  views::Widget* widget = pip_browser_view->GetWidget();
  BrowserFrameView* frame_view =
      pip_browser_view->browser_widget()->GetFrameView();

  auto frame_border_width = [&] {
    return frame_view->width() - frame_view->GetBoundsForClientView().width();
  };
  ASSERT_GT(frame_border_width(), 0);

  // Picture-in-Picture windows can't be maximized by Chrome, but the
  // compositor may still maximize them.
  pip_browser_view->SetCanMaximize(true);
  widget->Maximize();
  ASSERT_TRUE(widget->IsMaximized());
  EXPECT_EQ(frame_border_width(), 0);
  EXPECT_EQ(frame_view->GetBoundsForClientView().x(), 0);

  widget->Restore();
  ASSERT_FALSE(widget->IsMaximized());
  EXPECT_GT(frame_border_width(), 0);

  CloseBrowserSynchronously(pip_browser);
}

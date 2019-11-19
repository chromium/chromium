// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"

#include "ash/public/cpp/immersive/immersive_fullscreen_controller_test_api.h"
#include "ash/public/cpp/window_pin_type.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_host.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/webview/webview.h"

class ImmersiveModeControllerAshTest : public TestWithBrowserView {
 public:
  ImmersiveModeControllerAshTest()
      : TestWithBrowserView(Browser::TYPE_NORMAL) {}
  ~ImmersiveModeControllerAshTest() override {}

  // TestWithBrowserView override:
  void SetUp() override {
    TestWithBrowserView::SetUp();

    browser()->window()->Show();

    controller_ = browser_view()->immersive_mode_controller();
    ash::ImmersiveFullscreenControllerTestApi(
        static_cast<ImmersiveModeControllerAsh*>(controller_)->controller())
        .SetupForTest();
  }

  // Returns the bounds of |view| in widget coordinates.
  gfx::Rect GetBoundsInWidget(views::View* view) {
    return view->ConvertRectToWidget(view->GetLocalBounds());
  }

  // Toggle the browser's fullscreen state.
  void ToggleFullscreen() {
    // The fullscreen change notification is sent asynchronously. The
    // notification is used to trigger changes in whether the shelf is auto
    // hidden and whether a "light bar" version of the tab strip is used when
    // the top-of-window views are hidden.
    FullscreenNotificationObserver waiter(browser());
    chrome::ToggleFullscreenMode(browser());
    waiter.Wait();
  }

  // Set whether the browser is in tab fullscreen.
  void SetTabFullscreen(bool tab_fullscreen) {
    content::WebContents* web_contents =
        browser_view()->contents_web_view()->GetWebContents();
    FullscreenNotificationObserver waiter(browser());
    if (tab_fullscreen) {
      browser()
          ->exclusive_access_manager()
          ->fullscreen_controller()
          ->EnterFullscreenModeForTab(web_contents, GURL());
    } else {
      browser()
          ->exclusive_access_manager()
          ->fullscreen_controller()
          ->ExitFullscreenModeForTab(web_contents);
    }
    waiter.Wait();
  }

  // Attempt revealing the top-of-window views.
  void AttemptReveal() {
    if (!revealed_lock_.get()) {
      revealed_lock_.reset(controller_->GetRevealedLock(
          ImmersiveModeControllerAsh::ANIMATE_REVEAL_NO));
    }
  }

  // Attempt unrevealing the top-of-window views.
  void AttemptUnreveal() {
    revealed_lock_.reset();
  }

  ImmersiveModeController* controller() { return controller_; }

 private:
  // Not owned.
  ImmersiveModeController* controller_;

  std::unique_ptr<ImmersiveRevealedLock> revealed_lock_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAshTest);
};

// Test the layout and visibility of the tabstrip, toolbar and TopContainerView
// in immersive fullscreen.
TEST_F(ImmersiveModeControllerAshTest, Layout) {
  AddTab(browser(), GURL("about:blank"));

  TabStrip* tabstrip = browser_view()->tabstrip();
  ToolbarView* toolbar = browser_view()->toolbar();
  views::WebView* contents_web_view = browser_view()->contents_web_view();

  // Immersive fullscreen starts out disabled.
  ASSERT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  ASSERT_FALSE(controller()->IsEnabled());

  // By default, the tabstrip and toolbar should be visible.
  EXPECT_TRUE(tabstrip->GetVisible());
  EXPECT_TRUE(toolbar->GetVisible());
  EXPECT_EQ(
      0, browser_view()->contents_web_view()->holder()->GetHitTestTopInset());

  ToggleFullscreen();
  EXPECT_TRUE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_FALSE(toolbar->GetVisible());
  // The browser's top chrome is completely offscreen with tapstrip visible.
  EXPECT_TRUE(tabstrip->GetVisible());
  // Tabstrip and top container view should be completely offscreen.
  EXPECT_EQ(0, GetBoundsInWidget(tabstrip).bottom());
  EXPECT_EQ(0, GetBoundsInWidget(browser_view()->top_container()).bottom());
  EXPECT_EQ(
      0, browser_view()->contents_web_view()->holder()->GetHitTestTopInset());

  // Since the tab strip and tool bar are both hidden in immersive fullscreen
  // mode, the web contents should extend to the edge of screen.
  EXPECT_EQ(0, GetBoundsInWidget(contents_web_view).y());

  // Revealing the top-of-window views should set the tab strip back to the
  // normal style and show the toolbar.
  AttemptReveal();
  EXPECT_TRUE(controller()->IsRevealed());
  EXPECT_TRUE(tabstrip->GetVisible());
  EXPECT_TRUE(toolbar->GetVisible());
  EXPECT_NE(
      0, browser_view()->contents_web_view()->holder()->GetHitTestTopInset());

  // The TopContainerView should be flush with the top edge of the widget. If
  // it is not flush with the top edge the immersive reveal animation looks
  // wonky.
  EXPECT_EQ(0, GetBoundsInWidget(browser_view()->top_container()).y());

  // The web contents should be at the same y position as they were when the
  // top-of-window views were hidden.
  EXPECT_EQ(0, GetBoundsInWidget(contents_web_view).y());

  // Repeat the test for when in both immersive fullscreen and tab fullscreen.
  SetTabFullscreen(true);
  // Hide and reveal the top-of-window views so that they get relain out.
  AttemptUnreveal();
  AttemptReveal();

  // The tab strip and toolbar should still be visible and the TopContainerView
  // should still be flush with the top edge of the widget.
  EXPECT_TRUE(controller()->IsRevealed());
  EXPECT_TRUE(tabstrip->GetVisible());
  EXPECT_TRUE(toolbar->GetVisible());
  EXPECT_EQ(0, GetBoundsInWidget(browser_view()->top_container()).y());

  // The web contents should be flush with the top edge of the widget when in
  // both immersive and tab fullscreen.
  EXPECT_EQ(0, GetBoundsInWidget(contents_web_view).y());

  // Hide the top-of-window views. Tabstrip is still considered as visible.
  AttemptUnreveal();
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_FALSE(toolbar->GetVisible());
  EXPECT_TRUE(tabstrip->GetVisible());

  // The web contents should still be flush with the edge of the widget.
  EXPECT_EQ(0, GetBoundsInWidget(contents_web_view).y());

  // Exiting both immersive and tab fullscreen should show the tab strip and
  // toolbar.
  ToggleFullscreen();
  EXPECT_EQ(
      0, browser_view()->contents_web_view()->holder()->GetHitTestTopInset());
  EXPECT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_TRUE(tabstrip->GetVisible());
  EXPECT_TRUE(toolbar->GetVisible());
}

// Test that the browser commands which are usually disabled in fullscreen are
// are enabled in immersive fullscreen.
TEST_F(ImmersiveModeControllerAshTest, EnabledCommands) {
  ASSERT_FALSE(controller()->IsEnabled());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPEN_CURRENT_URL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ABOUT));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_LOCATION));

  ToggleFullscreen();
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPEN_CURRENT_URL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ABOUT));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_LOCATION));
}

// Test that restoring a window properly exits immersive fullscreen.
TEST_F(ImmersiveModeControllerAshTest, ExitUponRestore) {
  ASSERT_FALSE(controller()->IsEnabled());
  ToggleFullscreen();
  AttemptReveal();
  ASSERT_TRUE(controller()->IsEnabled());
  ASSERT_TRUE(controller()->IsRevealed());
  ASSERT_TRUE(browser_view()->GetWidget()->IsFullscreen());

  browser_view()->GetWidget()->Restore();
  EXPECT_FALSE(controller()->IsEnabled());
}

// Ensure the circular tab-loading throbbers are not painted as layers in
// immersive fullscreen, since the tab strip may animate in or out without
// moving the layers.
TEST_F(ImmersiveModeControllerAshTest, LayeredSpinners) {
  AddTab(browser(), GURL("about:blank"));

  TabStrip* tabstrip = browser_view()->tabstrip();

  // Immersive fullscreen starts out disabled; layers are OK.
  EXPECT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_TRUE(tabstrip->CanPaintThrobberToLayer());

  ToggleFullscreen();
  EXPECT_TRUE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(tabstrip->CanPaintThrobberToLayer());

  ToggleFullscreen();
  EXPECT_TRUE(tabstrip->CanPaintThrobberToLayer());
}

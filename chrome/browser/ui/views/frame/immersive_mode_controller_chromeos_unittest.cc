// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_chromeos.h"

#include "ash/wm/window_pin_util.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/chromeos/test_util.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_chromeos.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_tester.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_host.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_test_api.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/events/event.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/webview/webview.h"

class ImmersiveModeControllerChromeosTest : public TestWithBrowserView {
 public:
  ImmersiveModeControllerChromeosTest()
      : TestWithBrowserView(Browser::TYPE_NORMAL) {}

  ImmersiveModeControllerChromeosTest(
      const ImmersiveModeControllerChromeosTest&) = delete;
  ImmersiveModeControllerChromeosTest& operator=(
      const ImmersiveModeControllerChromeosTest&) = delete;

  ~ImmersiveModeControllerChromeosTest() override {}

  // TestWithBrowserView override:
  void SetUp() override {
    TestWithBrowserView::SetUp();

    browser()->window()->Show();

    controller_ = browser_view()->immersive_mode_controller();
    chromeos::ImmersiveFullscreenControllerTestApi(
        static_cast<ImmersiveModeControllerChromeos*>(controller_)
            ->controller())
        .SetupForTest();
  }

  // Returns the bounds of |view| in widget coordinates.
  gfx::Rect GetBoundsInWidget(views::View* view) {
    return view->ConvertRectToWidget(view->GetLocalBounds());
  }

  // Attempt revealing the top-of-window views.
  void AttemptReveal() {
    if (!revealed_lock_.get()) {
      revealed_lock_ = controller_->GetRevealedLock(
          ImmersiveModeControllerChromeos::ANIMATE_REVEAL_NO);
    }
  }

  // Attempt unrevealing the top-of-window views.
  void AttemptUnreveal() { revealed_lock_.reset(); }

  ImmersiveModeController* controller() { return controller_; }

 private:
  // Not owned.
  raw_ptr<ImmersiveModeController, DanglingUntriaged> controller_;

  std::unique_ptr<ImmersiveRevealedLock> revealed_lock_;
};

// Test the layout and visibility of the tabstrip, toolbar and TopContainerView
// in immersive fullscreen.
TEST_F(ImmersiveModeControllerChromeosTest, Layout) {
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

  ChromeOSBrowserUITest::EnterImmersiveFullscreenMode(browser());
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
  ChromeOSBrowserUITest::EnterTabFullscreenMode(
      browser(), browser_view()->contents_web_view()->GetWebContents());
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
  ChromeOSBrowserUITest::ExitImmersiveFullscreenMode(browser());
  EXPECT_EQ(
      0, browser_view()->contents_web_view()->holder()->GetHitTestTopInset());
  EXPECT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_TRUE(tabstrip->GetVisible());
  EXPECT_TRUE(toolbar->GetVisible());
}

// Verifies that transitioning from fullscreen to trusted pinned disables the
// immersive controls.
TEST_F(ImmersiveModeControllerChromeosTest, FullscreenToLockedTransition) {
  AddTab(browser(), GURL("about:blank"));
  // Start in fullscreen.
  ChromeOSBrowserUITest::EnterImmersiveFullscreenMode(browser());
  // ImmersiveController is enabled in fullscreen.
  EXPECT_TRUE(controller()->IsEnabled());

  // Transition to locked fullscreen.
  ChromeOSBrowserUITest::PinWindow(
      browser_view()->GetWidget()->GetNativeWindow(), /*trusted=*/true);
  // ImmersiveController is disabled in TrustedPinned so that it cannot be
  // exited.
  EXPECT_FALSE(controller()->IsEnabled());
}

// Verifies that transitioning from fullscreen to trusted pinned keeps immersive
// controls when the webapp is locked for OnTask. Only relevant for non-web
// browser scenarios.
TEST_F(ImmersiveModeControllerChromeosTest,
       FullscreenToLockedTransitionWhenLockedForOnTask) {
  browser()->SetLockedForOnTask(true);
  AddTab(browser(), GURL("about:blank"));
  // Start in fullscreen and verify ImmersiveController is enabled.
  ChromeOSBrowserUITest::EnterImmersiveFullscreenMode(browser());
  EXPECT_TRUE(controller()->IsEnabled());

  // Transition to locked fullscreen and verify ImmersiveController remains
  // enabled.
  ChromeOSBrowserUITest::PinWindow(
      browser_view()->GetWidget()->GetNativeWindow(), /*trusted=*/true);
  EXPECT_TRUE(controller()->IsEnabled());
}

// Test that the browser commands which are usually disabled in fullscreen are
// are enabled in immersive fullscreen.
TEST_F(ImmersiveModeControllerChromeosTest, EnabledCommands) {
  ASSERT_FALSE(controller()->IsEnabled());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPEN_CURRENT_URL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ABOUT));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_LOCATION));

  ChromeOSBrowserUITest::EnterImmersiveFullscreenMode(browser());
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPEN_CURRENT_URL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ABOUT));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_LOCATION));
}

// Test that restoring a window properly exits immersive fullscreen.
TEST_F(ImmersiveModeControllerChromeosTest, ExitUponRestore) {
  ASSERT_FALSE(controller()->IsEnabled());
  ChromeOSBrowserUITest::EnterImmersiveFullscreenMode(browser());
  AttemptReveal();
  ASSERT_TRUE(controller()->IsEnabled());
  ASSERT_TRUE(controller()->IsRevealed());
  ASSERT_TRUE(browser_view()->GetWidget()->IsFullscreen());

  browser_view()->GetWidget()->Restore();
  ImmersiveModeTester(browser()).WaitForFullscreenToExit();
}

// Ensure the circular tab-loading throbbers are not painted as layers in
// immersive fullscreen, since the tab strip may animate in or out without
// moving the layers.
TEST_F(ImmersiveModeControllerChromeosTest, LayeredSpinners) {
  AddTab(browser(), GURL("about:blank"));

  TabStrip* tabstrip = browser_view()->tabstrip();

  // Immersive fullscreen starts out disabled; layers are OK.
  EXPECT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_TRUE(tabstrip->CanPaintThrobberToLayer());

  ChromeOSBrowserUITest::EnterImmersiveFullscreenMode(browser());
  EXPECT_TRUE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(tabstrip->CanPaintThrobberToLayer());

  ChromeOSBrowserUITest::ExitImmersiveFullscreenMode(browser());
  EXPECT_TRUE(tabstrip->CanPaintThrobberToLayer());
}

// Ensure SetEnable is called when needed even when the previous request is
// passed from different client.
TEST_F(ImmersiveModeControllerChromeosTest, CallEnableForWidgetWhenNeeded) {
  ASSERT_FALSE(controller()->IsEnabled());
  chromeos::ImmersiveFullscreenController::EnableForWidget(
      browser_view()->frame(), /*enabled=*/true);
  ASSERT_TRUE(controller()->IsEnabled());
  controller()->SetEnabled(/*enabled=*/false);
  ASSERT_FALSE(controller()->IsEnabled());
}

class ImmersiveModeControllerChromeosWebUITabStripTest
    : public ImmersiveModeControllerChromeosTest {
 public:
  ImmersiveModeControllerChromeosWebUITabStripTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kWebUITabStrip);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensures the WebUI tab strip can be opened during immersive reveal.
// Regression test for crbug.com/1096569 where it couldn't be opened.
TEST_F(ImmersiveModeControllerChromeosWebUITabStripTest, CanOpen) {
  AddTab(browser(), GURL("about:blank"));

  // The WebUI tab strip is only used in touch mode.
  ui::TouchUiController::TouchUiScoperForTesting touch_mode_override(true);

  WebUITabStripContainerView* const webui_tab_strip =
      browser_view()->webui_tab_strip();
  ASSERT_TRUE(webui_tab_strip);
  EXPECT_FALSE(webui_tab_strip->GetVisible());

  ChromeOSBrowserUITest::EnterImmersiveFullscreenMode(browser());
  EXPECT_FALSE(webui_tab_strip->GetVisible());

  AttemptReveal();
  EXPECT_FALSE(webui_tab_strip->GetVisible());

  webui_tab_strip->SetVisibleForTesting(true);

  // The WebUITabStrip should be layed out.
  browser_view()->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_TRUE(webui_tab_strip->GetVisible());
  EXPECT_FALSE(webui_tab_strip->size().IsEmpty());
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/ash/window_pin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_chromeos.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_chromeos_test_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_chromeos.h"
#include "chrome/browser/ui/views/frame/immersive_mode_tester.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_host.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_test_api.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

class ImmersiveModeBrowserViewTest
    : public TopChromeMdParamTest<InProcessBrowserTest> {
 public:
  ImmersiveModeBrowserViewTest() = default;
  ImmersiveModeBrowserViewTest(const ImmersiveModeBrowserViewTest&) = delete;
  ImmersiveModeBrowserViewTest& operator=(const ImmersiveModeBrowserViewTest&) =
      delete;
  ~ImmersiveModeBrowserViewTest() override = default;

  // TopChromeMdParamTest<InProcessBrowserTest>:
  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();

    BrowserView::SetDisableRevealerDelayForTesting(true);

    chromeos::ImmersiveFullscreenControllerTestApi(
        static_cast<ImmersiveModeControllerChromeos*>(
            BrowserView::GetBrowserViewForBrowser(browser())
                ->immersive_mode_controller())
            ->controller())
        .SetupForTest();
  }
};

}  // namespace

using ImmersiveModeBrowserViewTestNoWebUiTabStrip =
    WebUiTabStripOverrideTest<false, ImmersiveModeBrowserViewTest>;

// This test does not make sense for the webUI tabstrip, since the frame is not
// painted in that case.
IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTestNoWebUiTabStrip,
                       ImmersiveFullscreen) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  BrowserNonClientFrameViewChromeOS* frame_view =
      GetFrameViewChromeOS(browser_view);

  ImmersiveModeController* immersive_mode_controller =
      browser_view->immersive_mode_controller();

  // Immersive fullscreen starts disabled.
  ASSERT_FALSE(browser_view->GetWidget()->IsFullscreen());
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());

  // Frame paints by default.
  EXPECT_TRUE(frame_view->GetShouldPaint());
  EXPECT_LT(0, frame_view
                   ->GetBoundsForTabStripRegion(
                       browser_view->tab_strip_region_view()->GetMinimumSize())
                   .bottom());

  // Enter both browser fullscreen and tab fullscreen. Entering browser
  // fullscreen should enable immersive fullscreen.
  ToggleFullscreenModeAndWait(browser());
  EnterFullscreenModeForTabAndWait(browser(), web_contents);
  EXPECT_TRUE(immersive_mode_controller->IsEnabled());
  // Caption button container is hidden.
  EXPECT_FALSE(frame_view->caption_button_container_->GetVisible());

  // An immersive reveal shows the buttons and the top of the frame.
  std::unique_ptr<ImmersiveRevealedLock> revealed_lock =
      immersive_mode_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO);
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_TRUE(frame_view->GetShouldPaint());
  // Caption button container is visible again.
  EXPECT_TRUE(frame_view->caption_button_container_->GetVisible());

  // End the reveal. When in both immersive browser fullscreen and tab
  // fullscreen.
  revealed_lock.reset();
  EXPECT_FALSE(immersive_mode_controller->IsRevealed());
  EXPECT_FALSE(frame_view->GetShouldPaint());
  EXPECT_EQ(0, frame_view
                   ->GetBoundsForTabStripRegion(
                       browser_view->tab_strip_region_view()->GetMinimumSize())
                   .bottom());
  EXPECT_FALSE(frame_view->caption_button_container_->GetVisible());

  // Repeat test but without tab fullscreen.
  ExitFullscreenModeForTabAndWait(browser(), web_contents);

  // Immersive reveal should have same behavior as before.
  revealed_lock = immersive_mode_controller->GetRevealedLock(
      ImmersiveModeController::ANIMATE_REVEAL_NO);
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_TRUE(frame_view->GetShouldPaint());
  EXPECT_LT(0, frame_view
                   ->GetBoundsForTabStripRegion(
                       browser_view->tab_strip_region_view()->GetMinimumSize())
                   .bottom());
  EXPECT_TRUE(frame_view->caption_button_container_->GetVisible());

  // Ending the reveal. Immersive browser should have the same behavior as full
  // screen, i.e., having an origin of (0,0).
  revealed_lock.reset();
  EXPECT_FALSE(frame_view->GetShouldPaint());
  EXPECT_EQ(0, frame_view
                   ->GetBoundsForTabStripRegion(
                       browser_view->tab_strip_region_view()->GetMinimumSize())
                   .bottom());
  EXPECT_FALSE(frame_view->caption_button_container_->GetVisible());

  // Exiting immersive fullscreen should make the caption buttons and the frame
  // visible again.
  {
    FullscreenNotificationObserver waiter(browser());
    browser_view->ExitFullscreen();
    waiter.Wait();
  }
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_TRUE(frame_view->GetShouldPaint());
  EXPECT_LT(0, frame_view
                   ->GetBoundsForTabStripRegion(
                       browser_view->tab_strip_region_view()->GetMinimumSize())
                   .bottom());
  EXPECT_TRUE(frame_view->caption_button_container_->GetVisible());
}

// Tests IDC_SELECT_TAB_0, IDC_SELECT_NEXT_TAB, IDC_SELECT_PREVIOUS_TAB and
// IDC_SELECT_LAST_TAB when the browser is in immersive fullscreen mode.
IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest,
                       TabNavigationAcceleratorsFullscreenBrowser) {
  ImmersiveModeTester tester(browser());
  // Make sure that the focus is on the webcontents rather than on the omnibox,
  // because if the focus is on the omnibox, the tab strip will remain revealed
  // in the immersive fullscreen mode and will interfere with this test waiting
  // for the revealer to be dismissed.
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();

  // Create three more tabs plus the existing one that browser tests start with.
  const GURL about_blank(url::kAboutBlankURL);
  ASSERT_TRUE(AddTabAtIndex(0, about_blank, ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();
  ASSERT_TRUE(AddTabAtIndex(0, about_blank, ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();
  ASSERT_TRUE(AddTabAtIndex(0, about_blank, ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();

  // Toggle fullscreen mode.
  chrome::ToggleFullscreenMode(browser());
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());
  // Wait for the end of the initial reveal which results from adding the new
  // tabs and changing the focused tab.
  tester.VerifyTabIndexAfterReveal(0);

  // Groups the browser command ID and its corresponding active tab index that
  // will result when the command is executed in this test.
  struct TestData {
    int command;
    int expected_index;
  };
  constexpr TestData test_data[] = {{IDC_SELECT_LAST_TAB, 3},
                                    {IDC_SELECT_TAB_0, 0},
                                    {IDC_SELECT_NEXT_TAB, 1},
                                    {IDC_SELECT_PREVIOUS_TAB, 0}};
  for (const auto& datum : test_data)
    tester.RunCommand(datum.command, datum.expected_index);
}

// This test does not make sense for the webUI tabstrip, since the window layout
// is different in that case.
IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTestNoWebUiTabStrip,
                       TestCaptionButtonsReceiveEventsInBrowserImmersiveMode) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  // Make sure that the focus is on the webcontents rather than on the omnibox,
  // because if the focus is on the omnibox, the tab strip will remain revealed
  // in the immersive fullscreen mode and will interfere with this test waiting
  // for the revealer to be dismissed.
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();

  // Toggle fullscreen mode.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());

  EXPECT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsRevealed());

  std::unique_ptr<ImmersiveRevealedLock> revealed_lock =
      browser_view->immersive_mode_controller()->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO);
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsRevealed());

  ImmersiveModeTester tester(browser());

  // Clicking the "restore" caption button should exit the immersive mode.
  aura::Window* window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(window->GetRootWindow());
  gfx::Size button_size = views::GetCaptionButtonLayoutSize(
      views::CaptionButtonLayoutSize::kBrowserCaptionMaximized);
  gfx::Point point_in_restore_button(window->GetBoundsInScreen().top_right());
  point_in_restore_button.Offset(-button_size.width() * 3 / 2,
                                 button_size.height() / 2);

  event_generator.MoveMouseTo(point_in_restore_button);
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsRevealed());
  event_generator.ClickLeftButton();
  tester.WaitForFullscreenToExit();

  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(browser()->window()->IsFullscreen());
}

IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest,
                       TestCaptionButtonsReceiveEventsInAppImmersiveMode) {
  browser()->window()->Close();

  // Open a new app window.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "test_browser_app", true /* trusted_source */, gfx::Rect(0, 0, 300, 300),
      browser()->profile(), true);
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;
  Browser* browser = Browser::Create(params);
  ASSERT_TRUE(browser->is_type_app());
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);

  chromeos::ImmersiveFullscreenControllerTestApi(
      static_cast<ImmersiveModeControllerChromeos*>(
          browser_view->immersive_mode_controller())
          ->controller())
      .SetupForTest();

  // Toggle fullscreen mode.
  chrome::ToggleFullscreenMode(browser);
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(browser_view->GetTabStripVisible());

  EXPECT_TRUE(browser->window()->IsFullscreen());
  EXPECT_FALSE(browser->window()->IsMaximized());
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsRevealed());

  std::unique_ptr<ImmersiveRevealedLock> revealed_lock =
      browser_view->immersive_mode_controller()->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO);
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsRevealed());

  ImmersiveModeTester tester(browser);
  AddBlankTabAndShow(browser);

  // Clicking the "restore" caption button should exit the immersive mode.
  aura::Window* window = browser->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(window->GetRootWindow(), window);
  gfx::Size button_size = views::GetCaptionButtonLayoutSize(
      views::CaptionButtonLayoutSize::kBrowserCaptionMaximized);
  gfx::Point point_in_restore_button(
      window->GetBoundsInRootWindow().top_right());
  point_in_restore_button.Offset(-2 * button_size.width(),
                                 button_size.height() / 2);

  event_generator.MoveMouseTo(point_in_restore_button);
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsRevealed());
  event_generator.ClickLeftButton();
  tester.WaitForFullscreenToExit();

  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(browser->window()->IsFullscreen());
}

// Regression test for crbug.com/796171.  Make sure that going from regular
// fullscreen to locked fullscreen does not cause a crash.
// Also test that the immersive mode is disabled afterwards (and the shelf is
// hidden, and the fullscreen control popup doesn't show up).
IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest,
                       RegularToLockedFullscreenDisablesImmersive) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  // Toggle fullscreen mode.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());

  // Set locked fullscreen state.
  PinWindow(browser()->window()->GetNativeWindow(), /*trusted=*/true);

  // We're fullscreen, immersive is disabled in locked fullscreen, and while
  // we're at it, also make sure that the shelf is hidden.
  EXPECT_TRUE(browser_view->GetWidget()->IsFullscreen());
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(IsShelfVisible());

  // Make sure the fullscreen control popup doesn't show up.
  ui::MouseEvent mouse_move(ui::ET_MOUSE_MOVED, gfx::Point(1, 1), gfx::Point(),
                            base::TimeTicks(), 0, 0);
  ASSERT_TRUE(browser_view->fullscreen_control_host_for_test());
  browser_view->fullscreen_control_host_for_test()->OnMouseEvent(mouse_move);
  EXPECT_FALSE(browser_view->fullscreen_control_host_for_test()->IsVisible());
}

// Regression test for crbug.com/883104.  Make sure that immersive fullscreen is
// disabled in locked fullscreen mode (also the shelf is hidden, and the
// fullscreen control popup doesn't show up).
IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest,
                       LockedFullscreenDisablesImmersive) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  EXPECT_FALSE(browser_view->GetWidget()->IsFullscreen());

  // Set locked fullscreen state.
  PinWindow(browser()->window()->GetNativeWindow(), /*trusted=*/true);

  // We're fullscreen, immersive is disabled in locked fullscreen, and while
  // we're at it, also make sure that the shelf is hidden.
  EXPECT_TRUE(browser_view->GetWidget()->IsFullscreen());
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(IsShelfVisible());

  // Make sure the fullscreen control popup doesn't show up.
  ui::MouseEvent mouse_move(ui::ET_MOUSE_MOVED, gfx::Point(1, 1), gfx::Point(),
                            base::TimeTicks(), 0, 0);
  ASSERT_TRUE(browser_view->fullscreen_control_host_for_test());
  browser_view->fullscreen_control_host_for_test()->OnMouseEvent(mouse_move);
  EXPECT_FALSE(browser_view->fullscreen_control_host_for_test()->IsVisible());
}

// Test the shelf visibility affected by entering and exiting tab fullscreen and
// immersive fullscreen.
IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest, TabAndBrowserFullscreen) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  // The shelf should start out as visible.
  EXPECT_TRUE(IsShelfVisible());

  // 1) Test that entering tab fullscreen from immersive fullscreen hides the
  // shelf.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(IsShelfVisible());

  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  EnterFullscreenModeForTabAndWait(browser(), web_contents);
  ASSERT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(IsShelfVisible());

  // 2) Test that exiting tab fullscreen autohides the shelf.
  ExitFullscreenModeForTabAndWait(browser(), web_contents);
  ASSERT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(IsShelfVisible());

  // 3) Test that exiting tab fullscreen and immersive fullscreen correctly
  // updates the shelf visibility.
  EnterFullscreenModeForTabAndWait(browser(), web_contents);
  ASSERT_TRUE(browser_view->immersive_mode_controller()->IsEnabled());
  chrome::ToggleFullscreenMode(browser());
  ASSERT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_TRUE(IsShelfVisible());
}

#define INSTANTIATE_TEST_SUITE(name) \
  INSTANTIATE_TEST_SUITE_P(All, name, ::testing::Values(false, true))

INSTANTIATE_TEST_SUITE(ImmersiveModeBrowserViewTest);
INSTANTIATE_TEST_SUITE(ImmersiveModeBrowserViewTestNoWebUiTabStrip);

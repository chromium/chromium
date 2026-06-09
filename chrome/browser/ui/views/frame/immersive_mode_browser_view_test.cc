// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <memory>

#include "base/test/run_until.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/ash/test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_chromeos.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_chromeos.h"
#include "chrome/browser/ui/views/frame/immersive_mode_tester.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_host.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_test_api.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

class ImmersiveModeBrowserViewTest
    : public TopChromeMdParamTest<ChromeOSBrowserUITest> {
 public:
  ImmersiveModeBrowserViewTest() = default;
  ImmersiveModeBrowserViewTest(const ImmersiveModeBrowserViewTest&) = delete;
  ImmersiveModeBrowserViewTest& operator=(const ImmersiveModeBrowserViewTest&) =
      delete;
  ~ImmersiveModeBrowserViewTest() override = default;

  // TopChromeMdParamTest<ChromeOSBrowserUITest>:
  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();

    BrowserView::SetDisableRevealerDelayForTesting(true);

    chromeos::ImmersiveFullscreenControllerTestApi(
        static_cast<ImmersiveModeControllerChromeos*>(
            ImmersiveModeController::From(browser()))
            ->controller())
        .SetupForTest();
  }
};

}  // namespace

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

  EnterImmersiveFullscreenMode(browser());

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
  for (const auto& datum : test_data) {
    tester.RunCommand(datum.command, datum.expected_index);
  }
}

IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest,
                       LocatedEventShouldRevealTopChrome) {
  gfx::ScopedAnimationDurationScaleMode scale_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  auto* const immersive_mode_controller =
      ImmersiveModeController::From(browser());

  EnterImmersiveFullscreenMode(browser());

  aura::Window* window = browser()->GetWindow()->GetNativeWindow();

  auto* immersive_fullscreen_controller =
      static_cast<ImmersiveModeControllerChromeos*>(immersive_mode_controller)
          ->controller();

  // In order for the reveal gesture to work, the animation on the frame has to
  // be completely stopped. If this fails, investigate if the
  // kGestureBeginScroll event is consumed by ash::ToplevelWindowEventHandler.
  auto wait_for_unreveal_animation_complete = [immersive_fullscreen_controller,
                                               window]() {
    auto* animation = chromeos::ImmersiveFullscreenControllerTestApi(
                          immersive_fullscreen_controller)
                          .GetAnimation();
    ASSERT_TRUE(base::test::RunUntil(
        [animation]() { return animation->GetCurrentValue() == 0.0; }));

    // Wait for the next begin frame after all animations are completed.
    auto* compositor = window->layer()->GetCompositor();
    ASSERT_TRUE(base::test::RunUntil(
        [compositor]() { return !compositor->IsAnimating(); }));
  };

  wait_for_unreveal_animation_complete();
  //  Animation is disabled, so it is not revealed when entered.
  EXPECT_FALSE(immersive_mode_controller->IsRevealed());

  enum EventType { kMouse, kTouch };
  for (auto event_type : {kMouse, kTouch}) {
    SCOPED_TRACE(event_type == kMouse ? "Mouse" : "Touch");

    ImmersiveModeTester tester(browser());

    ui::test::EventGenerator event_generator(window->GetRootWindow());
    gfx::Point point(std::roundl(window->bounds().width() / 2), 0);

    if (event_type == kMouse) {
      event_generator.MoveMouseTo(point);
    } else {
      event_generator.PressTouch(point);
      event_generator.MoveTouchBy(0, 30);
      event_generator.ReleaseTouch();
    }
    // We need to wait for timer.
    tester.WaitForRevealStarted();
    EXPECT_TRUE(immersive_mode_controller->IsRevealed());

    point.set_y(std::roundl(window->bounds().height() / 2));
    if (event_type == kMouse) {
      // Moving down below the topchrome hides the topchrome.
      event_generator.MoveMouseTo(point);
    } else {
      // Touching the center of the window hides the topchrome.
      event_generator.PressTouch(point);
      event_generator.ReleaseTouch();
    }

    wait_for_unreveal_animation_complete();
    EXPECT_FALSE(immersive_mode_controller->IsRevealed());
  }
}

IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest,
                       TestCaptionButtonsReceiveEventsInBrowserImmersiveMode) {
  auto* const immersive_mode_controller =
      ImmersiveModeController::From(browser());

  // Make sure that the focus is on the webcontents rather than on the omnibox,
  // because if the focus is on the omnibox, the tab strip will remain revealed
  // in the immersive fullscreen mode and will interfere with this test waiting
  // for the revealer to be dismissed.
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();

  EnterImmersiveFullscreenMode(browser());
  EXPECT_FALSE(browser()->GetWindow()->IsMaximized());
  EXPECT_FALSE(immersive_mode_controller->IsRevealed());

  std::unique_ptr<ImmersiveRevealedLock> revealed_lock =
      immersive_mode_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO);
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());

  // Clicking the "restore" caption button should exit the immersive mode.
  aura::Window* window = browser()->GetWindow()->GetNativeWindow();
  ui::test::EventGenerator event_generator(window->GetRootWindow());
  gfx::Size button_size = views::GetCaptionButtonLayoutSize(
      views::CaptionButtonLayoutSize::kBrowserCaptionMaximized);
  gfx::Point point_in_restore_button(window->GetBoundsInScreen().top_right());
  point_in_restore_button.Offset(-button_size.width() * 3 / 2,
                                 button_size.height() / 2);

  event_generator.MoveMouseTo(point_in_restore_button);
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  event_generator.ClickLeftButton();
  ImmersiveModeTester(browser()).WaitForFullscreenToExit();
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_FALSE(browser()->GetWindow()->IsFullscreen());
}

IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest,
                       TestCaptionButtonsReceiveEventsInAppImmersiveMode) {
  // Open a new app window.
  Browser* app_browser =
      CreateBrowserForApp("test_browser_app", browser()->profile());
  auto* const immersive_mode_controller =
      ImmersiveModeController::From(app_browser);
  BrowserView* const app_view =
      BrowserView::GetBrowserViewForBrowser(app_browser);
  chromeos::ImmersiveFullscreenControllerTestApi(
      static_cast<ImmersiveModeControllerChromeos*>(immersive_mode_controller)
          ->controller())
      .SetupForTest();

  EnterImmersiveFullscreenMode(app_browser);
  EXPECT_TRUE(app_browser->GetWindow()->IsFullscreen());
  EXPECT_FALSE(app_browser->GetWindow()->IsMaximized());
  EXPECT_FALSE(app_view->GetTabStripVisible());
  EXPECT_FALSE(immersive_mode_controller->IsRevealed());

  std::unique_ptr<ImmersiveRevealedLock> revealed_lock =
      immersive_mode_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO);
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());

  AddBlankTabAndShow(app_browser);

  // Clicking the "restore" caption button should exit the immersive mode.
  aura::Window* app_window = app_browser->GetWindow()->GetNativeWindow();
  ui::test::EventGenerator event_generator(app_window->GetRootWindow(),
                                           app_window);
  gfx::Size button_size = views::GetCaptionButtonLayoutSize(
      views::CaptionButtonLayoutSize::kBrowserCaptionMaximized);
  gfx::Point point_in_restore_button(
      app_window->GetBoundsInRootWindow().top_right());
  point_in_restore_button.Offset(-2 * button_size.width(),
                                 button_size.height() / 2);

  event_generator.MoveMouseTo(point_in_restore_button);
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  event_generator.ClickLeftButton();
  ImmersiveModeTester(app_browser).WaitForFullscreenToExit();
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_FALSE(app_browser->GetWindow()->IsFullscreen());
}

// Regression test for crbug.com/40555351.  Make sure that going from regular
// fullscreen to locked fullscreen does not cause a crash.
// Also test that the immersive mode is disabled afterwards (and the shelf is
// hidden, and the fullscreen control popup doesn't show up).
IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest,
                       RegularToLockedFullscreenDisablesImmersive) {
  EnterImmersiveFullscreenMode(browser());

  // Set locked fullscreen state.
  PinWindow(browser()->GetWindow()->GetNativeWindow(), /*trusted=*/true);

  // We're fullscreen, immersive is disabled in locked fullscreen, and while
  // we're at it, also make sure that the shelf is hidden.
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  EXPECT_TRUE(browser_view->GetWidget()->IsFullscreen());
  EXPECT_FALSE(ImmersiveModeController::From(browser())->IsEnabled());
  EXPECT_FALSE(IsShelfVisible());

  // Make sure the fullscreen control popup doesn't show up.
  ui::MouseEvent mouse_move(ui::EventType::kMouseMoved, gfx::Point(1, 1),
                            gfx::Point(), base::TimeTicks(), 0, 0);
  auto* const fullscreen_control_host =
      browser()->GetFeatures().fullscreen_control_host();
  ASSERT_NE(fullscreen_control_host, nullptr);
  fullscreen_control_host->OnMouseEvent(mouse_move);
  EXPECT_FALSE(fullscreen_control_host->IsVisible());
}

// Regression test for crbug.com/41413209.  Make sure that immersive fullscreen
// is disabled in locked fullscreen mode (also the shelf is hidden, and the
// fullscreen control popup doesn't show up).
IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest,
                       LockedFullscreenDisablesImmersive) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  EXPECT_FALSE(browser_view->GetWidget()->IsFullscreen());

  // Set locked fullscreen state.
  PinWindow(browser()->GetWindow()->GetNativeWindow(), /*trusted=*/true);

  // We're fullscreen, immersive is disabled in locked fullscreen, and while
  // we're at it, also make sure that the shelf is hidden.
  EXPECT_TRUE(browser_view->GetWidget()->IsFullscreen());
  EXPECT_FALSE(ImmersiveModeController::From(browser())->IsEnabled());
  EXPECT_FALSE(IsShelfVisible());

  // Make sure the fullscreen control popup doesn't show up.
  ui::MouseEvent mouse_move(ui::EventType::kMouseMoved, gfx::Point(1, 1),
                            gfx::Point(), base::TimeTicks(), 0, 0);
  auto* const fullscreen_control_host =
      browser()->GetFeatures().fullscreen_control_host();
  ASSERT_NE(fullscreen_control_host, nullptr);
  fullscreen_control_host->OnMouseEvent(mouse_move);
  EXPECT_FALSE(fullscreen_control_host->IsVisible());
}

// Test the shelf visibility affected by entering and exiting tab fullscreen and
// immersive fullscreen.
IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest, TabAndBrowserFullscreen) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  auto* const immersive_mode_controller =
      ImmersiveModeController::From(browser());

  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  // The shelf should start out as visible.
  EXPECT_TRUE(IsShelfVisible());

  // 1) Test that entering tab fullscreen from immersive fullscreen hides the
  // shelf.
  EnterImmersiveFullscreenMode(browser());
  EXPECT_FALSE(IsShelfVisible());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  EnterTabFullscreenMode(browser(), web_contents);
  ASSERT_TRUE(immersive_mode_controller->IsEnabled());
  EXPECT_FALSE(IsShelfVisible());

  // 2) Test that exiting tab fullscreen autohides the shelf.
  ExitTabFullscreenMode(browser(), web_contents);
  ASSERT_TRUE(immersive_mode_controller->IsEnabled());
  EXPECT_FALSE(IsShelfVisible());

  // 3) Test that exiting tab fullscreen and immersive fullscreen correctly
  // updates the shelf visibility.
  EnterTabFullscreenMode(browser(), web_contents);
  ASSERT_TRUE(immersive_mode_controller->IsEnabled());
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  ASSERT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_TRUE(IsShelfVisible());
}

IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest,
                       BubbleAnchoredToTabStripKeepsRevealed) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* const immersive_mode_controller =
      ImmersiveModeController::From(browser());

  EXPECT_TRUE(browser_view->GetSupportsTabStrip());

  EXPECT_FALSE(browser()->GetWindow()->IsFullscreen());
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_FALSE(immersive_mode_controller->IsRevealed());

  aura::Window* window = browser()->GetWindow()->GetNativeWindow();

  ui::test::EventGenerator event_generator(window->GetRootWindow(), window);
  event_generator.PressAndReleaseKeyAndModifierKeys(
      ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR);

  auto* tab_search_host = browser_view->GetTabSearchBubbleHost();
  ASSERT_TRUE(tab_search_host);
  auto* bubble_manager = tab_search_host->webui_bubble_manager_for_testing();
  ASSERT_TRUE(bubble_manager);

  EXPECT_TRUE(bubble_manager->GetBubbleWidget());
  views::test::WidgetVisibleWaiter(bubble_manager->GetBubbleWidget()).Wait();

  views::View* anchor_view =
      bubble_manager->bubble_view_for_testing()->GetAnchorView();
  EXPECT_TRUE(browser_view->tab_strip_view()->Contains(anchor_view));

  display::Screen* screen = display::Screen::Get();
  display::Display display = screen->GetDisplayNearestWindow(window);
  gfx::Rect bubble_rect_normal =
      bubble_manager->GetBubbleWidget()->GetWindowBoundsInScreen();
  gfx::Rect anchor_rect_normal = anchor_view->GetBoundsInScreen();

  // The bubble's top edge should be close to the bottom of the anchor view
  // (adjusting for potential shadow or inset). We can check if it's placed
  // generally below the anchor view's top.
  EXPECT_GE(bubble_rect_normal.y(), anchor_rect_normal.y());
  EXPECT_TRUE(display.work_area().Contains(bubble_rect_normal));

  EnterImmersiveFullscreenMode(browser());

  EXPECT_TRUE(chromeos::ImmersiveFullscreenControllerTestApi(
                  static_cast<ImmersiveModeControllerChromeos*>(
                      immersive_mode_controller)
                      ->controller())
                  .IsRevealLocked());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return immersive_mode_controller->IsRevealed(); }));

  EXPECT_EQ(anchor_view,
            bubble_manager->bubble_view_for_testing()->GetAnchorView());

  gfx::Rect bubble_rect_immersive =
      bubble_manager->GetBubbleWidget()->GetWindowBoundsInScreen();
  gfx::Rect anchor_rect_immersive = anchor_view->GetBoundsInScreen();

  EXPECT_GE(bubble_rect_immersive.y(), anchor_rect_immersive.y());
  EXPECT_TRUE(display.bounds().Contains(bubble_rect_immersive));

  event_generator.PressAndReleaseKey(ui::VKEY_ESCAPE);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !bubble_manager->GetBubbleWidget(); }));
}

class ImmersiveModeBrowserViewVerticalTabsTest
    : public ImmersiveModeBrowserViewTest {
 public:
  ImmersiveModeBrowserViewVerticalTabsTest() {
    scoped_feature_list_.InitAndEnableFeature(tabs::kVerticalTabs);
  }

  void SetUpOnMainThread() override {
    ImmersiveModeBrowserViewTest::SetUpOnMainThread();
    tabs::VerticalTabStripStateController::From(browser())
        ->SetVerticalTabsEnabled(true);
    RunScheduledLayouts();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewVerticalTabsTest,
                       BubbleAnchoredToTabStripDoesNotReveal) {
  auto verify_no_reveal = [&](Browser* test_browser,
                              std::string_view trace_name) {
    SCOPED_TRACE(trace_name);
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(test_browser);
    auto* const immersive_mode_controller =
        ImmersiveModeController::From(test_browser);

    EXPECT_TRUE(browser_view->GetSupportsTabStrip());

    EXPECT_FALSE(test_browser->GetWindow()->IsFullscreen());
    EXPECT_FALSE(immersive_mode_controller->IsEnabled());
    EXPECT_FALSE(immersive_mode_controller->IsRevealed());

    aura::Window* window = test_browser->GetWindow()->GetNativeWindow();

    ui::test::EventGenerator event_generator(window->GetRootWindow(), window);
    event_generator.PressAndReleaseKeyAndModifierKeys(
        ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR);

    auto* tab_search_host = browser_view->GetTabSearchBubbleHost();
    ASSERT_TRUE(tab_search_host);
    auto* bubble_manager = tab_search_host->webui_bubble_manager_for_testing();
    ASSERT_TRUE(bubble_manager);

    EXPECT_TRUE(bubble_manager->GetBubbleWidget());
    views::test::WidgetVisibleWaiter(bubble_manager->GetBubbleWidget()).Wait();

    EnterImmersiveFullscreenMode(test_browser);

    EXPECT_FALSE(chromeos::ImmersiveFullscreenControllerTestApi(
                     static_cast<ImmersiveModeControllerChromeos*>(
                         immersive_mode_controller)
                         ->controller())
                     .IsRevealLocked());

    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !immersive_mode_controller->IsRevealed(); }));

    event_generator.PressAndReleaseKey(ui::VKEY_ESCAPE);

    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !bubble_manager->GetBubbleWidget(); }));
  };

  // Test the initial browser
  verify_no_reveal(browser(), "1st browser");

  // Create a new browser with VT on, and test it
  Browser* new_browser = CreateBrowser(browser()->profile());
  verify_no_reveal(new_browser, "2nd browser");
}

#define INSTANTIATE_TEST_SUITE(name) \
  INSTANTIATE_TEST_SUITE_P(All, name, ::testing::Values(false, true))

INSTANTIATE_TEST_SUITE(ImmersiveModeBrowserViewTest);
INSTANTIATE_TEST_SUITE(ImmersiveModeBrowserViewVerticalTabsTest);

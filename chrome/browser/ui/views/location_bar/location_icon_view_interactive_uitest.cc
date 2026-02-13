// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

namespace {

class LocationIconViewTest : public InProcessBrowserTest {
 public:
  LocationIconViewTest() = default;

  LocationIconViewTest(const LocationIconViewTest&) = delete;
  LocationIconViewTest& operator=(const LocationIconViewTest&) = delete;

  ~LocationIconViewTest() override = default;

 protected:
  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  LocationBarView* GetLocationBarView() {
    return GetBrowserView()->GetLocationBarView();
  }

  LocationIconView* GetLocationIconView() {
    return GetLocationBarView()->location_icon_view();
  }

#if BUILDFLAG(IS_MAC)
  void EnterFullscreenMode() {
    bool initial_fullscreen = browser()->window()->IsFullscreen();
    EXPECT_FALSE(initial_fullscreen);

    auto* fullscreen_controller =
        browser()->GetExclusiveAccessManager()->fullscreen_controller();
    fullscreen_controller->ToggleBrowserFullscreenMode(/*user_initiated=*/true);

    // Wait for fullscreen transition.
    ASSERT_TRUE(base::test::RunUntil([&]() -> bool {
      return browser()->window()->IsFullscreen();
    })) << "Failed to enter fullscreen mode";
  }

  void FocusOmnibox() {
    LocationBarView* location_bar = GetLocationBarView();
    ASSERT_TRUE(location_bar) << "Failed to get LocationBarView";

    location_bar->FocusLocation(/*is_user_initiated=*/true,
                                /*clear_focus_if_failed=*/false);

    // Wait for focus to be set.
    ASSERT_TRUE(base::test::RunUntil([&]() -> bool {
      return location_bar->HasFocus();
    })) << "Omnibox failed to receive focus";
  }

  void ExitFullscreenMode() {
    bool initial_fullscreen = browser()->window()->IsFullscreen();
    EXPECT_TRUE(initial_fullscreen);

    auto* fullscreen_controller =
        browser()->GetExclusiveAccessManager()->fullscreen_controller();
    fullscreen_controller->ToggleBrowserFullscreenMode(/*user_initiated=*/true);

    // Wait for fullscreen exit.
    ASSERT_TRUE(base::test::RunUntil([&]() -> bool {
      return !browser()->window()->IsFullscreen();
    })) << "Failed to exit fullscreen mode";
  }
#endif  // BUILDFLAG(IS_MAC)
};

// Verify that clicking the location icon a second time hides the bubble.
// TODO(crbug.com/40251927) flaky on mac11-arm64-rel, disabled via filter
// TODO(crbug.com/41481796) Fails consistently on Linux, disabled via filter.
IN_PROC_BROWSER_TEST_F(LocationIconViewTest, HideOnSecondClick) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* location_icon_view =
      browser_view->toolbar()->location_bar_view()->location_icon_view();
  ASSERT_TRUE(location_icon_view);

  // Verify that clicking once shows the location icon bubble.
  scoped_refptr<content::MessageLoopRunner> runner1 =
      new content::MessageLoopRunner;
  ui_test_utils::MoveMouseToCenterAndClick(
      location_icon_view, ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP, runner1->QuitClosure());
  runner1->Run();

  EXPECT_EQ(PageInfoBubbleView::BUBBLE_PAGE_INFO,
            PageInfoBubbleView::GetShownBubbleType());

  // Verify that clicking again doesn't reshow it.
  scoped_refptr<content::MessageLoopRunner> runner2 =
      new content::MessageLoopRunner;
  ui_test_utils::MoveMouseToCenterAndClick(
      location_icon_view, ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP, runner2->QuitClosure());
  runner2->Run();

  EXPECT_EQ(PageInfoBubbleView::BUBBLE_NONE,
            PageInfoBubbleView::GetShownBubbleType());
}

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(LocationIconViewTest,
                       ImmersiveFullscreenExitWithOmniboxFocus) {
  BrowserView* browser_view = GetBrowserView();
  ASSERT_TRUE(browser_view) << "Failed to get BrowserView";

  EnterFullscreenMode();

  auto* immersive_mode_controller = ImmersiveModeController::From(browser());
  if (!immersive_mode_controller || !immersive_mode_controller->IsEnabled()) {
    GTEST_SKIP() << "Immersive mode not supported on this configuration";
  }

  FocusOmnibox();

  // This triggers OnImmersiveFullscreenExited() ->
  // ReparentTopContainerForEndOfImmersive() which reparents the toolbar
  // hierarchy while the omnibox (and location icon) may be updating.
  //
  // The crash scenario:
  // 1. Omnibox has focus.
  // 2. ExitFullscreenMode() triggers view reparenting.
  // 3. During reparenting, LocationIconView is temporarily detached from
  // widget.
  // 4. Theme change or focus change triggers LocationIconView::Update().
  // 5. Update() calls UpdateBackground().
  // 6. UpdateBackground() calls GetWidget() which returns nullptr.
  // 7. CRASH when trying to call GetWidget()->GetColorProvider().
  ExitFullscreenMode();

  // Expect no crash and fullscreen to be exited.
  EXPECT_FALSE(browser()->window()->IsFullscreen());
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace

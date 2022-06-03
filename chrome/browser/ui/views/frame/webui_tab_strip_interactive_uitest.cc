// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/controls/webview/webview.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_test_api.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class WebUITabStripInteractiveTest : public InProcessBrowserTest {
 public:
  WebUITabStripInteractiveTest() {
    feature_override_.InitAndEnableFeature(features::kWebUITabStrip);
  }

  ~WebUITabStripInteractiveTest() override = default;

 private:
  base::test::ScopedFeatureList feature_override_;
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_{true};
};

// Regression test for crbug.com/1027375.
IN_PROC_BROWSER_TEST_F(WebUITabStripInteractiveTest,
                       CanTypeInOmniboxAfterTabStripClose) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  WebUITabStripContainerView* const container = browser_view->webui_tab_strip();
  ASSERT_NE(nullptr, container);

  ui_test_utils::FocusView(browser(), VIEW_ID_OMNIBOX);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  OmniboxViewViews* const omnibox =
      browser_view->toolbar()->location_bar()->omnibox_view();
  omnibox->SetUserText(u"");

  container->SetVisibleForTesting(true);
  browser_view->Layout();

  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Make sure the tab strip's contents are fully loaded.
  views::WebView* const container_web_view = container->web_view_for_testing();
  ASSERT_TRUE(WaitForLoadStop(container_web_view->GetWebContents()));

  // Click in tab strip then in Omnibox.
  base::RunLoop click_loop_1;
  ui_test_utils::MoveMouseToCenterAndPress(
      container_web_view, ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP, click_loop_1.QuitClosure());
  click_loop_1.Run();

  base::RunLoop click_loop_2;
  ui_test_utils::MoveMouseToCenterAndPress(omnibox, ui_controls::LEFT,
                                           ui_controls::DOWN | ui_controls::UP,
                                           click_loop_2.QuitClosure());
  click_loop_2.Run();

  // The omnibox should still be focused and should accept keyboard input.
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_A, false,
                                              false, false, false));
  EXPECT_EQ(u"a", omnibox->GetText());
}

IN_PROC_BROWSER_TEST_F(WebUITabStripInteractiveTest,
                       EventInTabContentClosesContainer) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());

  WebUITabStripContainerView* const container = browser_view->webui_tab_strip();
  ASSERT_NE(nullptr, container);

  // Open the tab strip
  container->SetVisibleForTesting(true);
  browser_view->Layout();

  base::RunLoop click_loop;
  ui_test_utils::MoveMouseToCenterAndPress(
      browser_view->contents_web_view(), ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP, click_loop.QuitClosure());
  click_loop.Run();

  // Make sure it's closed (after the close animation).
  container->FinishAnimationForTesting();
  EXPECT_FALSE(container->GetVisible());
}

IN_PROC_BROWSER_TEST_F(WebUITabStripInteractiveTest,
                       EventInContainerDoesNotClose) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());

  WebUITabStripContainerView* const container = browser_view->webui_tab_strip();
  ASSERT_NE(nullptr, container);

  // Open the tab strip
  container->SetVisibleForTesting(true);
  browser_view->Layout();

  base::RunLoop click_loop;
  ui_test_utils::MoveMouseToCenterAndPress(container, ui_controls::LEFT,
                                           ui_controls::DOWN | ui_controls::UP,
                                           click_loop.QuitClosure());
  click_loop.Run();

  // Make sure it stays open. The FinishAnimationForTesting() call
  // should be a no-op.
  container->FinishAnimationForTesting();
  EXPECT_TRUE(container->GetVisible());
  EXPECT_FALSE(container->bounds().IsEmpty());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Regression test for crbug.com/1112028
IN_PROC_BROWSER_TEST_F(WebUITabStripInteractiveTest, CanUseInImmersiveMode) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());

  chromeos::ImmersiveFullscreenControllerTestApi immersive_test_api(
      chromeos::ImmersiveFullscreenController::Get(browser_view->GetWidget()));
  immersive_test_api.SetupForTest();

  ImmersiveModeController* const immersive_mode_controller =
      browser_view->immersive_mode_controller();
  immersive_mode_controller->SetEnabled(true);

  WebUITabStripContainerView* const container = browser_view->webui_tab_strip();
  ASSERT_NE(nullptr, container);

  // IPH may cause a reveal. Stop it.
  browser_view->feature_promo_controller()->BlockPromosForTesting();

  EXPECT_FALSE(immersive_mode_controller->IsRevealed());

  // Try opening the tab strip.
  container->SetVisibleForTesting(true);
  browser_view->Layout();
  EXPECT_TRUE(container->GetVisible());
  EXPECT_FALSE(container->bounds().IsEmpty());
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());

  // Tapping in the tab strip shouldn't hide the toolbar.
  base::RunLoop click_loop_1;
  ui_test_utils::MoveMouseToCenterAndPress(container, ui_controls::LEFT,
                                           ui_controls::DOWN | ui_controls::UP,
                                           click_loop_1.QuitClosure());
  click_loop_1.Run();

  // If the behavior is correct, this call will be a no-op.
  container->FinishAnimationForTesting();
  EXPECT_TRUE(container->GetVisible());
  EXPECT_FALSE(container->bounds().IsEmpty());
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());

  // Interacting with the toolbar should also not close the container.
  base::RunLoop click_loop_2;
  ui_test_utils::MoveMouseToCenterAndPress(
      browser_view->toolbar()->reload_button(), ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP, click_loop_2.QuitClosure());
  click_loop_2.Run();

  container->FinishAnimationForTesting();
  EXPECT_TRUE(container->GetVisible());
  EXPECT_FALSE(container->bounds().IsEmpty());
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

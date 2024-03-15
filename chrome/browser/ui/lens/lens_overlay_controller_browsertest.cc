// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class runs functional tests for lens overlay. These tests spin up a full
// web browser, but allow for inspection and modification of internal state of
// LensOverlayController and other business-logic classes.

#include "base/test/run_until.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/side_panel/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/lens/lens_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"

namespace {

using State = LensOverlayController::State;

class LensOverlayControllerBrowserTest : public InProcessBrowserTest {
 private:
  base::test::ScopedFeatureList feature_list_{lens::features::kLensOverlay};
};

// TODO(https://crbug.com/329708692): Flaky on Linux and Lacros.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_CaptureScreenshot DISABLED_CaptureScreenshot
#else
#define MAYBE_CaptureScreenshot CaptureScreenshot
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       MAYBE_CaptureScreenshot) {
  // State should start in off.
  auto* controller =
      browser()->tab_strip_model()->GetActiveTab()->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify screenshot was captured and stored.
  auto screenshot_bitmap = controller->current_screenshot();
  ASSERT_FALSE(screenshot_bitmap.empty());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, CreateAndLoadWebUI) {
  // State should start in off.
  auto* controller =
      browser()->tab_strip_model()->GetActiveTab()->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Assert that the web view was created and loaded WebUI.
  GURL webui_url(chrome::kChromeUILensUntrustedURL);
  raw_ptr<views::WebView> overlay_web_view = views::AsViewClass<views::WebView>(
      controller->GetOverlayWidgetForTesting()
          ->GetContentsView()
          ->children()[0]);
  ASSERT_TRUE(content::WaitForLoadStop(overlay_web_view->GetWebContents()));
  ASSERT_EQ(overlay_web_view->GetWebContents()->GetLastCommittedURL(),
            webui_url);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, ShowSidePanel) {
  // State should start in off.
  auto* controller =
      browser()->tab_strip_model()->GetActiveTab()->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Now show the side panel.
  controller->side_panel_coordinator()->RegisterEntryAndShow();

  auto* coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);
}

}  // namespace

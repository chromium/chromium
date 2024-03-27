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
#include "chrome/browser/ui/tabs/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/side_panel/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/lens/lens_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"

namespace {

using State = LensOverlayController::State;

// Stubs out network requests.
class LensOverlayControllerFake : public LensOverlayController {
 public:
  explicit LensOverlayControllerFake(tabs::TabModel* tab_model)
      : LensOverlayController(tab_model) {}
};

class TabFeaturesFake : public tabs::TabFeatures {
 public:
  explicit TabFeaturesFake(tabs::TabModel* tab) : tabs::TabFeatures(tab) {}

 protected:
  std::unique_ptr<LensOverlayController> CreateLensController(
      tabs::TabModel* tab) override {
    return std::make_unique<LensOverlayControllerFake>(tab);
  }
};

class LensOverlayControllerBrowserTest : public InProcessBrowserTest {
 protected:
  LensOverlayControllerBrowserTest() {
    tabs::TabFeatures::ReplaceTabFeaturesForTesting(base::BindRepeating(
        &LensOverlayControllerBrowserTest::CreateTabFeatures,
        base::Unretained(this)));
  }

  ~LensOverlayControllerBrowserTest() override {
    tabs::TabFeatures::ReplaceTabFeaturesForTesting(base::NullCallback());
  }

  std::unique_ptr<tabs::TabFeatures> CreateTabFeatures(tabs::TabModel* tab) {
    return std::make_unique<TabFeaturesFake>(tab);
  }

  content::WebContents* GetOverlayWebContents() {
    auto* controller = browser()
                           ->tab_strip_model()
                           ->GetActiveTab()
                           ->tab_features()
                           ->lens_overlay_controller();
    raw_ptr<views::WebView> overlay_web_view =
        views::AsViewClass<views::WebView>(
            controller->GetOverlayWidgetForTesting()
                ->GetContentsView()
                ->children()[0]);
    return overlay_web_view->GetWebContents();
  }

 private:
  base::test::ScopedFeatureList feature_list_{lens::features::kLensOverlay};
};

// TODO(https://crbug.com/329708692): Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       DISABLED_CaptureScreenshot) {
  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
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
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Assert that the web view was created and loaded WebUI.
  GURL webui_url(chrome::kChromeUILensUntrustedURL);
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  ASSERT_EQ(GetOverlayWebContents()->GetLastCommittedURL(), webui_url);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, ShowSidePanel) {
  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
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

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/reload_button_web_view.h"

#include "base/command_line.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

class ReloadButtonWebViewPixelBrowserTest : public InProcessBrowserTest {
 public:
  ReloadButtonWebViewPixelBrowserTest() {
    // All features for Webium Production should be included here.
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2,
         features::kInitialWebUISyncNavStartToCommit},
        {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Force the color mode to light to avoid flakiness.
    ThemeServiceFactory::GetForProfile(browser()->profile())
        ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kLight);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReloadButtonWebViewPixelBrowserTest, Basic) {
  ui::TrackedElement* element = nullptr;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    element =
        BrowserElements::From(browser())->GetElement(kReloadButtonElementId);
    return element != nullptr;
  }));
  ASSERT_TRUE(element);
  views::TrackedElementViews* reload_view_element =
      element->AsA<views::TrackedElementViews>();
  ASSERT_TRUE(reload_view_element);
  ReloadButtonWebView* reload_view =
      views::AsViewClass<ReloadButtonWebView>(reload_view_element->view());
  ASSERT_TRUE(reload_view);
  views::WebView* web_view = reload_view->GetWebViewForTesting();
  ASSERT_TRUE(web_view);

  // Wait for the WebView to finish composition.
  content::WaitForCopyableViewInWebContents(web_view->GetWebContents());
  // Assert that WebContents is not loading, as it affects the state of the
  // reload button.
  ASSERT_FALSE(web_view->GetWebContents()->IsLoading());
  // The WebView should be using the light color mode.
  ASSERT_EQ(web_view->GetWidget()->GetColorMode(),
            ui::ColorProviderKey::ColorMode::kLight);

  // Pixel test
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVerifyPixels)) {
    views::ViewSkiaGoldPixelDiff pixel_diff(
        "ReloadButtonWebViewPixelBrowserTest");
    EXPECT_TRUE(pixel_diff.CompareViewScreenshot("Basic", reload_view));
  }
}

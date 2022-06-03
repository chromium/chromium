// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/pixel/browser_skia_gold_pixel_diff.h"
#include "content/public/test/browser_test.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

class SkiaGoldDemoPixelTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    // Call this before SetUp() to cause the test to generate pixel output.
    EnablePixelOutput();

    InProcessBrowserTest::SetUp();
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();

    // Initialize the class here. Follow the best practice to use
    // the class name as the screenshot prefix.
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
        browser()->window()->GetNativeWindow());
    ASSERT_NO_FATAL_FAILURE(pixel_diff_.Init(widget, "SkiaGoldDemoPixelTest"));
  }

  const BrowserSkiaGoldPixelDiff& GetPixelDiff() const { return pixel_diff_; }

 private:
  BrowserSkiaGoldPixelDiff pixel_diff_;
};

// This is a demo test to ensure the omnibox looks as expected.
// The test will first open the bookmarks manager, then take a screenshot of
// the omnibox. CompareScreenshot() compares with the golden image,
// which was previously human-approved, is stored server-side, and is managed
// by Skia Gold. If any pixels differ, the test will fail and output a link
// for the author to triage the new image.
IN_PROC_BROWSER_TEST_F(SkiaGoldDemoPixelTest, TestOmnibox) {
  // Always disable animation for stability.
  ui::ScopedAnimationDurationScaleMode disable_animation(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  GURL url("chrome://bookmarks");
  AddTabAtIndex(0, url, ui::PageTransition::PAGE_TRANSITION_FIRST);
  auto* const browser_view = static_cast<BrowserView*>(browser()->window());
  bool ret = GetPixelDiff().CompareScreenshot("omnibox",
      browser_view->GetLocationBarView());
  EXPECT_TRUE(ret);
}

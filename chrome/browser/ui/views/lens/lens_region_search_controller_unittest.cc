// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace lens {

class LensRegionSearchControllerTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures({features::kLensStandalone}, {});
    TestWithBrowserView::SetUp();

    // Create an active web contents.
    AddTab(browser_view()->browser(), GURL("about:blank"));
    controller_ = std::make_unique<LensRegionSearchController>();
    controller_->SetWebContentsForTesting(
        browser_view()->GetActiveWebContents());
    controller_->SetEntryPointForTesting(
        lens::AmbientSearchEntryPoint::
            CONTEXT_MENU_SEARCH_REGION_WITH_GOOGLE_LENS);
  }

  void TearDown() override {
    controller_.reset();
    TestWithBrowserView::TearDown();
  }

 protected:
  std::unique_ptr<LensRegionSearchController> controller_;
};

TEST_F(LensRegionSearchControllerTest, LensRegionSearchEmptyImage) {
  base::HistogramTester tester;
  image_editor::ScreenshotCaptureResult result;

  result.screen_bounds = gfx::Rect();
  result.image = gfx::Image();

  controller_->OnCaptureCompleted(result);
  tester.ExpectUniqueSample(
      lens::kLensRegionSearchCaptureResultHistogramName,
      lens::LensRegionSearchCaptureResult::ERROR_CAPTURING_REGION, 1);
}

TEST_F(LensRegionSearchControllerTest, LensRegionSearchEmptyCaptureResults) {
  base::HistogramTester tester;
  image_editor::ScreenshotCaptureResult result;
  controller_->OnCaptureCompleted(result);
  tester.ExpectUniqueSample(
      lens::kLensRegionSearchCaptureResultHistogramName,
      lens::LensRegionSearchCaptureResult::ERROR_CAPTURING_REGION, 1);
}

TEST_F(LensRegionSearchControllerTest, UndefinedAspectRatioTest) {
  int height = 0;
  int width = 100;
  EXPECT_EQ(LensRegionSearchController::GetAspectRatioFromSize(height, width),
            LensRegionSearchAspectRatio::UNDEFINED);
}

TEST_F(LensRegionSearchControllerTest, SquareAspectRatioTest) {
  int height = 100;
  int width = 100;
  int screen_height = 1000;
  int screen_width = 1000;

  base::HistogramTester tester;
  image_editor::ScreenshotCaptureResult result;
  result.image = gfx::test::CreateImage(width, height);
  result.screen_bounds = gfx::Rect(screen_width, screen_height);

  controller_->OnCaptureCompleted(result);
  tester.ExpectUniqueSample(lens::kLensRegionSearchCaptureResultHistogramName,
                            lens::LensRegionSearchCaptureResult::SUCCESS, 1);
  tester.ExpectUniqueSample(kLensRegionSearchRegionAspectRatioHistogramName,
                            LensRegionSearchAspectRatio::SQUARE, 1);
  tester.ExpectTotalCount(
      kLensRegionSearchRegionViewportProportionHistogramName, 1);
}

TEST_F(LensRegionSearchControllerTest, WideAspectRatioTest) {
  int height = 100;
  int width = 170;
  int screen_height = 1000;
  int screen_width = 1000;

  base::HistogramTester tester;
  image_editor::ScreenshotCaptureResult result;
  result.image = gfx::test::CreateImage(width, height);
  result.screen_bounds = gfx::Rect(screen_width, screen_height);

  controller_->OnCaptureCompleted(result);
  tester.ExpectUniqueSample(lens::kLensRegionSearchCaptureResultHistogramName,
                            lens::LensRegionSearchCaptureResult::SUCCESS, 1);
  tester.ExpectUniqueSample(kLensRegionSearchRegionAspectRatioHistogramName,
                            LensRegionSearchAspectRatio::WIDE, 1);
  tester.ExpectTotalCount(
      kLensRegionSearchRegionViewportProportionHistogramName, 1);
}

TEST_F(LensRegionSearchControllerTest, VeryWideAspectRatioTest) {
  int height = 100;
  int width = 10000;
  int screen_height = 10000;
  int screen_width = 10000;

  base::HistogramTester tester;
  image_editor::ScreenshotCaptureResult result;
  result.image = gfx::test::CreateImage(width, height);
  result.screen_bounds = gfx::Rect(screen_width, screen_height);

  controller_->OnCaptureCompleted(result);
  tester.ExpectUniqueSample(lens::kLensRegionSearchCaptureResultHistogramName,
                            lens::LensRegionSearchCaptureResult::SUCCESS, 1);
  tester.ExpectUniqueSample(kLensRegionSearchRegionAspectRatioHistogramName,
                            LensRegionSearchAspectRatio::VERY_WIDE, 1);
  tester.ExpectTotalCount(
      kLensRegionSearchRegionViewportProportionHistogramName, 1);
}

TEST_F(LensRegionSearchControllerTest, TallAspectRatioTest) {
  int height = 170;
  int width = 100;
  int screen_height = 1000;
  int screen_width = 1000;

  base::HistogramTester tester;
  image_editor::ScreenshotCaptureResult result;
  result.image = gfx::test::CreateImage(width, height);
  result.screen_bounds = gfx::Rect(screen_width, screen_height);

  controller_->OnCaptureCompleted(result);
  tester.ExpectUniqueSample(lens::kLensRegionSearchCaptureResultHistogramName,
                            lens::LensRegionSearchCaptureResult::SUCCESS, 1);
  tester.ExpectUniqueSample(kLensRegionSearchRegionAspectRatioHistogramName,
                            LensRegionSearchAspectRatio::TALL, 1);
  tester.ExpectTotalCount(
      kLensRegionSearchRegionViewportProportionHistogramName, 1);
}

TEST_F(LensRegionSearchControllerTest, VeryTallAspectRatioTest) {
  int height = 10000;
  int width = 100;
  int screen_height = 10000;
  int screen_width = 10000;

  base::HistogramTester tester;
  image_editor::ScreenshotCaptureResult result;
  result.image = gfx::test::CreateImage(width, height);
  result.screen_bounds = gfx::Rect(screen_width, screen_height);

  controller_->OnCaptureCompleted(result);
  tester.ExpectUniqueSample(lens::kLensRegionSearchCaptureResultHistogramName,
                            lens::LensRegionSearchCaptureResult::SUCCESS, 1);
  tester.ExpectUniqueSample(kLensRegionSearchRegionAspectRatioHistogramName,
                            LensRegionSearchAspectRatio::VERY_TALL, 1);
  tester.ExpectTotalCount(
      kLensRegionSearchRegionViewportProportionHistogramName, 1);
}

TEST_F(LensRegionSearchControllerTest, AccurateViewportProportionTest) {
  int screen_height = 1000;
  int screen_width = 1000;
  int image_height = 100;
  int image_width = 100;
  EXPECT_EQ(LensRegionSearchController::CalculateViewportProportionFromAreas(
                screen_height, screen_width, image_width, image_height),
            1);
}

TEST_F(LensRegionSearchControllerTest, UndefinedViewportProportionTest) {
  int screen_height = 0;
  int screen_width = 0;
  int image_height = 100;
  int image_width = 100;
  EXPECT_EQ(LensRegionSearchController::CalculateViewportProportionFromAreas(
                screen_height, screen_width, image_width, image_height),
            -1);
}

}  // namespace lens

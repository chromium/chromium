// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/fenced_frames_page_load_metrics_observer.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

namespace {

const char kSetUpUrl[] = "https://a.test/setup";
const char kTestUrl[] = "https://a.test/test";
const char kFencedFramesUrl[] = "https://a.test/fenced_frames";

class FencedFramesPageLoadMetricsObserverTest
    : public PageLoadMetricsObserverContentTestHarness {
 public:
  FencedFramesPageLoadMetricsObserverTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }

 protected:
  void RegisterObservers(PageLoadTracker* tracker) override {
    auto observer = std::make_unique<FencedFramesPageLoadMetricsObserver>();
    observer_ = observer.get();
    tracker->AddObserver(std::move(observer));

    // Add UmaPageLoadMetricsObserver to ensure existing observer's behavior
    // being not changed.
    tracker->AddObserver(std::make_unique<UmaPageLoadMetricsObserver>());
  }

  void PopulateTimingForHistograms(content::RenderFrameHost* rfh) {
    mojom::PageLoadTiming timing;
    InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::Now();
    timing.parse_timing->parse_stop = base::Milliseconds(50);
    timing.paint_timing->first_image_paint = base::Milliseconds(80);
    timing.paint_timing->first_contentful_paint = base::Milliseconds(100);

    auto largest_contentful_paint = CreateLargestContentfulPaintTiming();
    largest_contentful_paint->largest_image_paint = base::Milliseconds(100);
    largest_contentful_paint->largest_image_paint_size = 100;
    timing.paint_timing->largest_contentful_paint =
        std::move(largest_contentful_paint);

    timing.interactive_timing->first_input_delay = base::Milliseconds(10);
    timing.interactive_timing->first_input_timestamp = base::Milliseconds(4780);

    PopulateRequiredTimingFields(&timing);
    tester()->SimulateTimingUpdate(timing, rfh);
  }

 private:
  void SetUp() override {
    PageLoadMetricsObserverContentTestHarness::SetUp();

    // Navigating here so |RegisterObservers| will get called before each test.
    NavigateAndCommit(GURL(kSetUpUrl));
  }

  raw_ptr<FencedFramesPageLoadMetricsObserver, DanglingUntriaged> observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FencedFramesPageLoadMetricsObserverTest, Foreground) {
  NavigateAndCommit(GURL(kTestUrl));

  content::RenderFrameHostWrapper fenced_frame_root(
      content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendFencedFrame());
  ASSERT_TRUE(fenced_frame_root->IsFencedFrameRoot());

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL(kFencedFramesUrl), fenced_frame_root.get());
  ASSERT_NE(nullptr, simulator);
  simulator->Commit();

  PopulateTimingForHistograms(fenced_frame_root.get());
  tester()->NavigateToUntrackedUrl();

  web_contents()->GetController().GetBackForwardCache().Flush();
  EXPECT_TRUE(fenced_frame_root.WaitUntilRenderFrameDeleted());

  // Verify if FencedFrames prefixed metrics are recorded as expected.
  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kHistogramFencedFramesNavigationToFirstPaint, 1);
  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kHistogramFencedFramesNavigationToFirstImagePaint, 1);
  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kHistogramFencedFramesNavigationToFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kHistogramFencedFramesNavigationToLargestContentfulPaint2, 1);
  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kHistogramFencedFramesFirstInputDelay4, 1);
  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kHistogramFencedFramesCumulativeShiftScore, 1);
  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kHistogramFencedFramesCumulativeShiftScoreMainFrame, 1);

  // Verify if other observers, i.e. UmaPageLoadMetricsObserver doesn't
  // record metrics for inner Page of the FencedFrames. It counts 2 for
  // NavigateAndCommit(GURL(kTestUrl)) and NavigateToUntrackedUrl(), and
  // doesn't for similator->Commit() on kFencedFramesUrl.
  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kHistogramPageLoadCpuTotalUsage, 2);
}

TEST_F(FencedFramesPageLoadMetricsObserverTest, Background) {
  NavigateAndCommit(GURL(kTestUrl));

  // Simulate a background tab.
  web_contents()->WasHidden();

  content::RenderFrameHostWrapper fenced_frame_root(
      content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendFencedFrame());
  ASSERT_TRUE(fenced_frame_root->IsFencedFrameRoot());

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL(kFencedFramesUrl), fenced_frame_root.get());
  ASSERT_NE(nullptr, simulator);
  simulator->Commit();

  PopulateTimingForHistograms(fenced_frame_root.get());
  tester()->NavigateToUntrackedUrl();

  web_contents()->GetController().GetBackForwardCache().Flush();
  EXPECT_TRUE(fenced_frame_root.WaitUntilRenderFrameDeleted());

  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kHistogramFencedFramesNavigationToFirstPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kHistogramFencedFramesNavigationToFirstImagePaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kHistogramFencedFramesNavigationToFirstContentfulPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kHistogramFencedFramesNavigationToLargestContentfulPaint2, 0);
  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kHistogramFencedFramesFirstInputDelay4, 0);
  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kHistogramFencedFramesCumulativeShiftScore, 1);
  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kHistogramFencedFramesCumulativeShiftScoreMainFrame, 1);
}

}  // namespace

}  // namespace page_load_metrics

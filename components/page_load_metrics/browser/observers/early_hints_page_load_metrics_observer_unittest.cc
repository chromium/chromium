// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/early_hints_page_load_metrics_observer.h"

#include <memory>

#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"

namespace {

const char kTestUrl[] = "https://a.test";

class EarlyHintsPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverContentTestHarness {
 protected:
  // page_load_metrics::PageLoadMetricsObserverContentTestHarness overrides:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<EarlyHintsPageLoadMetricsObserver>());
  }

  void NavigateAndCommitWithEarlyHintsPreload(GURL url) {
    std::unique_ptr<content::NavigationSimulator> navigation =
        content::NavigationSimulator::CreateBrowserInitiated(url,
                                                             web_contents());
    navigation->SetEarlyHintsPreloadLinkHeaderReceived(true);
    navigation->SetTransition(ui::PAGE_TRANSITION_LINK);
    navigation->Commit();
  }

  void PopulateTimingForHistograms() {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::Now();
    timing.parse_timing->parse_stop = base::Milliseconds(50);
    timing.paint_timing->first_contentful_paint = base::Milliseconds(100);

    auto largest_contentful_paint =
        page_load_metrics::CreateLargestContentfulPaintTiming();
    largest_contentful_paint->largest_image_paint = base::Milliseconds(100);
    largest_contentful_paint->largest_image_paint_size = 100;
    timing.paint_timing->largest_contentful_paint =
        std::move(largest_contentful_paint);

    timing.interactive_timing->first_input_delay = base::Milliseconds(10);
    timing.interactive_timing->first_input_timestamp = base::Milliseconds(4780);

    PopulateRequiredTimingFields(&timing);
    tester()->SimulateTimingUpdate(timing);
  }
};

TEST_F(EarlyHintsPageLoadMetricsObserverTest, PageType) {
  NavigateAndCommit(GURL(kTestUrl));
  PopulateTimingForHistograms();
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(
      page_load_metrics::internal::kPageLoadTrackerPageType,
      page_load_metrics::internal::PageLoadTrackerPageType::kPrimaryPage, 1);
}

TEST_F(EarlyHintsPageLoadMetricsObserverTest, WithoutPreload) {
  NavigateAndCommit(GURL(kTestUrl));
  PopulateTimingForHistograms();
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramEarlyHintsPreloadFirstContentfulPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramEarlyHintsPreloadLargestContentfulPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramEarlyHintsPreloadFirstInputDelay, 0);
}

TEST_F(EarlyHintsPageLoadMetricsObserverTest, WithPreload) {
  NavigateAndCommitWithEarlyHintsPreload(GURL(kTestUrl));
  PopulateTimingForHistograms();
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramEarlyHintsPreloadFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramEarlyHintsPreloadLargestContentfulPaint, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramEarlyHintsPreloadFirstInputDelay, 1);
}

}  // namespace

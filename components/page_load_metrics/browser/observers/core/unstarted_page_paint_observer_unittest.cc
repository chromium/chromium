// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/core/unstarted_page_paint_observer.h"

#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/navigation_simulator_impl.h"

namespace {

const char kTestUrl[] = "https://a.test/";

}  // namespace

class UnstartedPagePaintObserverTest
    : public page_load_metrics::PageLoadMetricsObserverContentTestHarness {
 protected:
  void InitFirstContentfulPaintTiming(
      page_load_metrics::mojom::PageLoadTiming& timing) {
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::Now();
    timing.response_start = base::Milliseconds(1);
    timing.parse_timing->parse_start = base::Milliseconds(10);
    timing.paint_timing->first_paint = base::Milliseconds(30);
    timing.paint_timing->first_contentful_paint = base::Milliseconds(300);
  }

 private:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<UnstartedPagePaintObserver>());
  }
};

TEST_F(UnstartedPagePaintObserverTest,
       PrimaryPageUnstartedPagePaintFiresHistogramToTrue) {
  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Forward time to force navigation to expire.
  task_environment()->FastForwardBy(
      base::Seconds(internal::kUnstartedPagePaintTimeoutSeconds));

  // Validate histogram set to true is fired.
  tester()->histogram_tester().ExpectBucketCount(
      ::internal::kPageLoadUnstartedPagePaint, true, 1);
}

TEST_F(UnstartedPagePaintObserverTest,
       PrimaryPageAfterFirstContentfulPaintFiresHistogramToFalse) {
  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Simulate some time than expiration timer.
  task_environment()->FastForwardBy(
      base::Seconds(internal::kUnstartedPagePaintTimeoutSeconds / 2));

  // Simulate timing report with first contentful paint.
  page_load_metrics::mojom::PageLoadTiming timing;
  InitFirstContentfulPaintTiming(timing);
  tester()->SimulateTimingUpdate(timing);

  // Forward time to a time higher than expiration timer.
  task_environment()->FastForwardBy(
      base::Seconds(internal::kUnstartedPagePaintTimeoutSeconds));

  // Validate histogram set to false is fired after first contentful paint
  // reported.
  tester()->histogram_tester().ExpectBucketCount(
      ::internal::kPageLoadUnstartedPagePaint, false, 1);
}

TEST_F(UnstartedPagePaintObserverTest,
       PrimaryPageToBackForwardCacheDoesNotFireHistogram) {
  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Simulate some time, less than expiration threshold.
  task_environment()->FastForwardBy(
      base::Seconds(internal::kUnstartedPagePaintTimeoutSeconds / 2));

  // Navigate to a different page to send first one to Back Forward Cache
  const char kOtherTestUrl[] = "https://other.test/";
  NavigateAndCommit(GURL(kOtherTestUrl));

  // Forward time to force navigation to expire for first navigation.
  task_environment()->FastForwardBy(
      base::Seconds(internal::kUnstartedPagePaintTimeoutSeconds / 2 + 1));

  // Validate no histogram fired
  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kPageLoadUnstartedPagePaint, 0);
}

TEST_F(
    UnstartedPagePaintObserverTest,
    PrimaryPageRestoredFromBackForwardCacheAndUnstartedPagePaintFiresHistogram) {
  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Simulate some time, less than expiration threshold.
  task_environment()->FastForwardBy(
      base::Seconds(internal::kUnstartedPagePaintTimeoutSeconds / 2));

  // Navigate to a different page to send first one to Back Forward Cache
  const char kOtherTestUrl[] = "https://other.test/";
  NavigateAndCommit(GURL(kOtherTestUrl));

  // Forward time to force navigation to expire for first navigation.
  task_environment()->FastForwardBy(
      base::Seconds(internal::kUnstartedPagePaintTimeoutSeconds / 2 + 1));

  // Validate no histogram fired
  tester()->histogram_tester().ExpectBucketCount(
      ::internal::kPageLoadUnstartedPagePaint, true, 0);

  // Navigate back to first page from Back Forward Cache.
  auto back_navigation1 =
      content::NavigationSimulatorImpl::CreateHistoryNavigation(
          -1, web_contents(), false /* is_renderer_initiated */);
  back_navigation1->ReadyToCommit();

  // Forward time to force navigation to expire.
  task_environment()->FastForwardBy(
      base::Seconds(internal::kUnstartedPagePaintTimeoutSeconds));

  // Validate histogram fired
  tester()->histogram_tester().ExpectBucketCount(
      ::internal::kPageLoadUnstartedPagePaint, true, 1);
}

TEST_F(
    UnstartedPagePaintObserverTest,
    PrimaryPageFirstContentfulPaintBeforeSentToBackForwardCacheDoesNotFireHistogramAfterRestoringFromBackForwardCache) {
  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Simulate some time, less than expiration threshold.
  task_environment()->FastForwardBy(
      base::Seconds(internal::kUnstartedPagePaintTimeoutSeconds / 2));

  // Simulate timing report with first contentful paint.
  page_load_metrics::mojom::PageLoadTiming timing;
  InitFirstContentfulPaintTiming(timing);
  tester()->SimulateTimingUpdate(timing);

  // Validate histogram to false was fired.
  tester()->histogram_tester().ExpectBucketCount(
      ::internal::kPageLoadUnstartedPagePaint, false, 1);

  // Navigate to a different page to send first one to Back Forward Cache
  const char kOtherTestUrl[] = "https://other.test/";
  NavigateAndCommit(GURL(kOtherTestUrl));

  // Navigate back to first page from Back Forward Cache.
  auto back_navigation1 =
      content::NavigationSimulatorImpl::CreateHistoryNavigation(
          -1, web_contents(), false /* is_renderer_initiated */);
  back_navigation1->ReadyToCommit();

  // Forward time to force navigation to expire.
  task_environment()->FastForwardBy(
      base::Seconds(internal::kUnstartedPagePaintTimeoutSeconds));

  // Validate no histogram to true was fired.
  tester()->histogram_tester().ExpectBucketCount(
      ::internal::kPageLoadUnstartedPagePaint, true, 0);
}

TEST_F(UnstartedPagePaintObserverTest, PrerenderPageDoesNotFireHistogram) {
  content::test::ScopedPrerenderWebContentsDelegate web_contents_delegate(
      *web_contents());

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Simulate timing report with first contentful paint.
  page_load_metrics::mojom::PageLoadTiming timing;
  InitFirstContentfulPaintTiming(timing);
  tester()->SimulateTimingUpdate(timing);

  // Validate histogram to false was fired.
  tester()->histogram_tester().ExpectBucketCount(
      ::internal::kPageLoadUnstartedPagePaint, false, 1);

  // Add a prerender page.
  const char kPrerenderingUrl[] = "https://a.test/prerender";
  content::WebContentsTester::For(web_contents())
      ->AddPrerenderAndCommitNavigation(GURL(kPrerenderingUrl));

  // Forward time to force navigation to expire.
  task_environment()->FastForwardBy(
      base::Seconds(internal::kUnstartedPagePaintTimeoutSeconds));

  // Validate no additional histograms fired
  tester()->histogram_tester().ExpectTotalCount(
      ::internal::kPageLoadUnstartedPagePaint, 1);
}

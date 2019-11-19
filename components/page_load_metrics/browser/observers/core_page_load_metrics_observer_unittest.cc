// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/core_page_load_metrics_observer.h"

#include <memory>

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/web_mouse_event.h"

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;
using LargestContentType =
    page_load_metrics::PageLoadMetricsObserver::LargestContentType;

namespace {

const char kDefaultTestUrl[] = "https://google.com";
const char kDefaultTestUrlAnchor[] = "https://google.com#samepage";
const char kDefaultTestUrl2[] = "https://whatever.com";

}  // namespace

class CorePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverContentTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<CorePageLoadMetricsObserver>());
  }

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverContentTestHarness::SetUp();
    page_load_metrics::LargestContentfulPaintHandler::SetTestMode(true);
  }
};

TEST_F(CorePageLoadMetricsObserverTest, NoMetrics) {
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramDomContentLoaded, 0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout,
                                                0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstImagePaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest,
       SameDocumentNoTriggerUntilTrueNavCommit) {
  base::TimeDelta first_layout = base::TimeDelta::FromMilliseconds(1);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.document_timing->first_layout = first_layout;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);

  NavigateAndCommit(GURL(kDefaultTestUrlAnchor));

  NavigateAndCommit(GURL(kDefaultTestUrl2));
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramDomContentLoaded, 0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout,
                                                1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFirstLayout, first_layout.InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstImagePaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, SingleMetricAfterCommit) {
  base::TimeDelta first_layout = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta parse_start = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta parse_stop = base::TimeDelta::FromMilliseconds(5);
  base::TimeDelta parse_script_load_duration =
      base::TimeDelta::FromMilliseconds(3);
  base::TimeDelta parse_script_exec_duration =
      base::TimeDelta::FromMilliseconds(1);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.document_timing->first_layout = first_layout;
  timing.parse_timing->parse_start = parse_start;
  timing.parse_timing->parse_stop = parse_stop;
  timing.parse_timing->parse_blocked_on_script_load_duration =
      parse_script_load_duration;
  timing.parse_timing->parse_blocked_on_script_execution_duration =
      parse_script_exec_duration;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramDomContentLoaded, 0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout,
                                                1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFirstLayout, first_layout.InMilliseconds(), 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramParseDuration,
      (parse_stop - parse_start).InMilliseconds(), 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramParseBlockedOnScriptLoad,
      parse_script_load_duration.InMilliseconds(), 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramParseBlockedOnScriptExecution,
      parse_script_exec_duration.InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstImagePaint, 0);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDuration, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, MultipleMetricsAfterCommits) {
  base::TimeDelta response = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta first_layout_1 = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta first_layout_2 = base::TimeDelta::FromMilliseconds(20);
  base::TimeDelta first_image_paint = base::TimeDelta::FromMilliseconds(30);
  base::TimeDelta first_contentful_paint = first_image_paint;
  base::TimeDelta dom_content = base::TimeDelta::FromMilliseconds(40);
  base::TimeDelta load = base::TimeDelta::FromMilliseconds(100);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.response_start = response;
  timing.document_timing->first_layout = first_layout_1;
  timing.paint_timing->first_image_paint = first_image_paint;
  timing.paint_timing->first_contentful_paint = first_contentful_paint;
  timing.document_timing->dom_content_loaded_event_start = dom_content;
  timing.document_timing->load_event_start = load;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFirstContentfulPaint,
      first_contentful_paint.InMilliseconds(), 1);

  NavigateAndCommit(GURL(kDefaultTestUrl2));

  page_load_metrics::mojom::PageLoadTiming timing2;
  page_load_metrics::InitPageLoadTimingForTest(&timing2);
  timing2.navigation_start = base::Time::FromDoubleT(200);
  timing2.document_timing->first_layout = first_layout_2;
  PopulateRequiredTimingFields(&timing2);

  tester()->SimulateTimingUpdate(timing2);

  NavigateAndCommit(GURL(kDefaultTestUrl));

  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout,
                                                2);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFirstLayout, first_layout_1.InMilliseconds(), 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFirstLayout, first_layout_2.InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFirstContentfulPaint,
      first_contentful_paint.InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstImagePaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFirstImagePaint, first_image_paint.InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramDomContentLoaded, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramDomContentLoaded, dom_content.InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 1);
  tester()->histogram_tester().ExpectBucketCount(internal::kHistogramLoad,
                                                 load.InMilliseconds(), 1);
}

TEST_F(CorePageLoadMetricsObserverTest, BackgroundDifferentHistogram) {
  base::TimeDelta first_layout = base::TimeDelta::FromSeconds(2);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.document_timing->first_layout = first_layout;
  PopulateRequiredTimingFields(&timing);

  // Simulate "Open link in new tab."
  web_contents()->WasHidden();
  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);

  // Simulate switching to the tab and making another navigation.
  web_contents()->WasShown();

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramDomContentLoaded, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramLoad, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramFirstLayout, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kBackgroundHistogramFirstLayout, first_layout.InMilliseconds(),
      1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramFirstImagePaint, 0);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramDomContentLoaded, 0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout,
                                                0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstImagePaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, OnlyBackgroundLaterEvents) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.document_timing->dom_content_loaded_event_start =
      base::TimeDelta::FromMicroseconds(1);
  PopulateRequiredTimingFields(&timing);

  // Make sure first_image_paint hasn't been set (wasn't set by
  // PopulateRequiredTimingFields), since we want to defer setting it until
  // after backgrounding.
  ASSERT_FALSE(timing.paint_timing->first_image_paint);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);

  // Background the tab, then foreground it.
  web_contents()->WasHidden();
  web_contents()->WasShown();
  timing.paint_timing->first_image_paint = base::TimeDelta::FromSeconds(4);
  PopulateRequiredTimingFields(&timing);
  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  // If the system clock is low resolution, PageLoadTracker's
  // first_background_time_ may be same as other times such as
  // dom_content_loaded_event_start.
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start,
          tester()->GetDelegateForCommittedLoad())) {
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramDomContentLoaded, 1);
    tester()->histogram_tester().ExpectBucketCount(
        internal::kHistogramDomContentLoaded,
        timing.document_timing->dom_content_loaded_event_start.value()
            .InMilliseconds(),
        1);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kBackgroundHistogramDomContentLoaded, 0);
  } else {
    tester()->histogram_tester().ExpectTotalCount(
        internal::kBackgroundHistogramDomContentLoaded, 1);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramDomContentLoaded, 0);
  }

  tester()->histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramLoad, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramFirstImagePaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kBackgroundHistogramFirstImagePaint,
      timing.paint_timing->first_image_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstImagePaint, 0);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDuration, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, DontBackgroundQuickerLoad) {
  // Set this event at 1 microsecond so it occurs before we foreground later in
  // the test.
  base::TimeDelta first_layout = base::TimeDelta::FromMicroseconds(1);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.document_timing->first_layout = first_layout;
  PopulateRequiredTimingFields(&timing);

  web_contents()->WasHidden();

  // Open in new tab
  tester()->StartNavigation(GURL(kDefaultTestUrl));

  // Switch to the tab
  web_contents()->WasShown();

  // Start another provisional load
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  tester()->SimulateTimingUpdate(timing);

  // Navigate again to see if the timing updated for the foregrounded load.
  NavigateAndCommit(GURL(kDefaultTestUrl));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramDomContentLoaded, 0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout,
                                                1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFirstLayout, first_layout.InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstImagePaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, FailedProvisionalLoad) {
  GURL url(kDefaultTestUrl);
  // The following tests a navigation that fails and should commit an error
  // page, but finishes before the error page commit.
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  navigation->Fail(net::ERR_TIMED_OUT);
  navigation->AbortCommit();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramDomContentLoaded, 0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout,
                                                0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstImagePaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFailedProvisionalLoad, 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDuration, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDurationNoCommit, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, FailedBackgroundProvisionalLoad) {
  // Test that failed provisional event does not get logged in the
  // histogram if it happened in the background
  GURL url(kDefaultTestUrl);
  web_contents()->WasHidden();
  content::NavigationSimulator::NavigateAndFailFromDocument(
      url, net::ERR_TIMED_OUT, main_rfh());

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFailedProvisionalLoad, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, Reload) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(5);
  timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(10);
  PopulateRequiredTimingFields(&timing);

  GURL url(kDefaultTestUrl);
  tester()->NavigateWithPageTransitionAndCommit(url,
                                                ui::PAGE_TRANSITION_RELOAD);
  tester()->SimulateTimingUpdate(timing);

  auto resources =
      GetSampleResourceDataUpdateForTesting(10 * 1024 /* resource_size */);
  tester()->SimulateResourceDataUseUpdate(resources);
  int64_t network_bytes = 0;
  int64_t cache_bytes = 0;
  for (const auto& resource : resources) {
    if (resource->is_complete) {
      if (resource->cache_type ==
          page_load_metrics::mojom::CacheType::kNotCached)
        network_bytes += resource->encoded_body_length;
      else
        cache_bytes += resource->encoded_body_length;
    }
  }

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintReload, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeFirstContentfulPaintReload,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintForwardBack, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintNewNavigation, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartReload, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeParseStartReload,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartForwardBack, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartNewNavigation, 0);

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeNetworkBytesReload,
      static_cast<int>((network_bytes) / 1024), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeNetworkBytesForwardBack, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeNetworkBytesNewNavigation, 0);

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeCacheBytesReload,
      static_cast<int>((cache_bytes) / 1024), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeCacheBytesForwardBack, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeCacheBytesNewNavigation, 0);

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeTotalBytesReload,
      static_cast<int>((network_bytes + cache_bytes) / 1024), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeTotalBytesForwardBack, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeTotalBytesNewNavigation, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, ForwardBack) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(5);
  timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(10);
  PopulateRequiredTimingFields(&timing);

  GURL url(kDefaultTestUrl);
  // Back navigations to a page that was reloaded report a main transition type
  // of PAGE_TRANSITION_RELOAD with a PAGE_TRANSITION_FORWARD_BACK
  // modifier. This test verifies that when we encounter such a page, we log it
  // as a forward/back navigation.
  tester()->NavigateWithPageTransitionAndCommit(
      url, ui::PageTransitionFromInt(ui::PAGE_TRANSITION_RELOAD |
                                     ui::PAGE_TRANSITION_FORWARD_BACK));
  tester()->SimulateTimingUpdate(timing);

  auto resources =
      GetSampleResourceDataUpdateForTesting(10 * 1024 /* resource_size */);
  tester()->SimulateResourceDataUseUpdate(resources);
  int64_t network_bytes = 0;
  int64_t cache_bytes = 0;
  for (const auto& resource : resources) {
    if (resource->is_complete) {
      if (resource->cache_type ==
          page_load_metrics::mojom::CacheType::kNotCached)
        network_bytes += resource->encoded_body_length;
      else
        cache_bytes += resource->encoded_body_length;
    }
  }

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintReload, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintForwardBack, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeFirstContentfulPaintForwardBack,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintNewNavigation, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartReload, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartForwardBack, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeParseStartForwardBack,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartNewNavigation, 0);

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeNetworkBytesForwardBack,
      static_cast<int>((network_bytes) / 1024), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeNetworkBytesNewNavigation, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeNetworkBytesReload, 0);

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeCacheBytesForwardBack,
      static_cast<int>((cache_bytes) / 1024), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeCacheBytesNewNavigation, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeCacheBytesReload, 0);

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeTotalBytesForwardBack,
      static_cast<int>((network_bytes + cache_bytes) / 1024), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeTotalBytesNewNavigation, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeTotalBytesReload, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, NewNavigation) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(5);
  timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(10);
  PopulateRequiredTimingFields(&timing);

  GURL url(kDefaultTestUrl);
  tester()->NavigateWithPageTransitionAndCommit(url, ui::PAGE_TRANSITION_LINK);
  tester()->SimulateTimingUpdate(timing);

  auto resources =
      GetSampleResourceDataUpdateForTesting(10 * 1024 /* resource_size */);
  tester()->SimulateResourceDataUseUpdate(resources);
  int64_t network_bytes = 0;
  int64_t cache_bytes = 0;
  for (const auto& resource : resources) {
    if (resource->is_complete) {
      if (resource->cache_type ==
          page_load_metrics::mojom::CacheType::kNotCached)
        network_bytes += resource->encoded_body_length;
      else
        cache_bytes += resource->encoded_body_length;
    }
  }

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintReload, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintForwardBack, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintNewNavigation, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeFirstContentfulPaintNewNavigation,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartReload, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartForwardBack, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartNewNavigation, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeParseStartNewNavigation,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeNetworkBytesNewNavigation,
      static_cast<int>((network_bytes) / 1024), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeNetworkBytesForwardBack, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeNetworkBytesReload, 0);

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeCacheBytesNewNavigation,
      static_cast<int>((cache_bytes) / 1024), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeCacheBytesForwardBack, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeCacheBytesReload, 0);

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeTotalBytesNewNavigation,
      static_cast<int>((network_bytes + cache_bytes) / 1024), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeTotalBytesForwardBack, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeTotalBytesReload, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, BytesAndResourcesCounted) {
  NavigateAndCommit(GURL(kDefaultTestUrl));
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramPageLoadTotalBytes, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramPageLoadNetworkBytes, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramPageLoadCacheBytes, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramPageLoadNetworkBytesIncludingHeaders, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramTotalCompletedResources, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNetworkCompletedResources, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCacheCompletedResources, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, FirstMeaningfulPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(5);
  timing.paint_timing->first_meaningful_paint =
      base::TimeDelta::FromMilliseconds(10);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstMeaningfulPaint, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramParseStartToFirstMeaningfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFirstMeaningfulPaintStatus,
      internal::FIRST_MEANINGFUL_PAINT_RECORDED, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, LargestImagePaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_image_paint_size = 10u;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLargestImagePaint),
              testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(CorePageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_OnlySubframeProvided) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(100);
  // Intentionally not set candidates for the main frame.
  PopulateRequiredTimingFields(&timing);

  // Create a subframe timing with a largest_image_paint.
  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(200);
  subframe_timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  subframe_timing.paint_timing->largest_image_paint_size = 100u;
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kDefaultTestUrl));
  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLargestContentfulPaint),
              testing::ElementsAre(base::Bucket(4780, 1)));
  EXPECT_THAT(
      tester()->histogram_tester().GetAllSamples(
          internal::kHistogramLargestContentfulPaintContentType),
      testing::ElementsAre(base::Bucket(
          static_cast<base::HistogramBase::Sample>(LargestContentType::kImage),
          1)));
  EXPECT_TRUE(
      tester()
          ->histogram_tester()
          .GetAllSamples(internal::kHistogramLargestContentfulPaintMainFrame)
          .empty());
  EXPECT_TRUE(
      tester()
          ->histogram_tester()
          .GetAllSamples(
              internal::kHistogramLargestContentfulPaintMainFrameContentType)
          .empty());
}

TEST_F(CorePageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_OnlyMainFrameProvided) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_image_paint_size = 50u;
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  // Intentionally not set candidates for the subframes.
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kDefaultTestUrl));
  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLargestContentfulPaint),
              testing::ElementsAre(base::Bucket(4780, 1)));
  EXPECT_THAT(
      tester()->histogram_tester().GetAllSamples(
          internal::kHistogramLargestContentfulPaintContentType),
      testing::ElementsAre(base::Bucket(
          static_cast<base::HistogramBase::Sample>(LargestContentType::kImage),
          1)));
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLargestContentfulPaintMainFrame),
              testing::ElementsAre(base::Bucket(4780, 1)));
  EXPECT_THAT(
      tester()->histogram_tester().GetAllSamples(
          internal::kHistogramLargestContentfulPaintMainFrameContentType),
      testing::ElementsAre(base::Bucket(
          static_cast<base::HistogramBase::Sample>(LargestContentType::kImage),
          1)));
}

// This is to test whether LargestContentfulPaintAllFrames could merge
// candidates from different frames correctly. The merging will substitutes the
// existing candidate if a larger candidate from subframe is provided.
TEST_F(CorePageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_MergeFromFramesBySize_SubframeLarger) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  // Create a main frame timing with a largest_image_paint that happens late.
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(9382);
  timing.paint_timing->largest_image_paint_size = 50u;
  PopulateRequiredTimingFields(&timing);

  // Create a candidate in subframe with a larger size.
  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  subframe_timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  subframe_timing.paint_timing->largest_image_paint_size = 100u;
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kDefaultTestUrl));
  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLargestContentfulPaint),
              testing::ElementsAre(base::Bucket(4780, 1)));
  EXPECT_THAT(
      tester()->histogram_tester().GetAllSamples(
          internal::kHistogramLargestContentfulPaintContentType),
      testing::ElementsAre(base::Bucket(
          static_cast<base::HistogramBase::Sample>(LargestContentType::kImage),
          1)));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLargestContentfulPaintMainFrame),
              testing::ElementsAre(base::Bucket(9382, 1)));
  EXPECT_THAT(
      tester()->histogram_tester().GetAllSamples(
          internal::kHistogramLargestContentfulPaintMainFrameContentType),
      testing::ElementsAre(base::Bucket(
          static_cast<base::HistogramBase::Sample>(LargestContentType::kImage),
          1)));
}

// This is to test whether LargestContentfulPaintAllFrames could merge
// candidates from different frames correctly. The merging will substitutes the
// existing candidate if a larger candidate from main frame is provided.
TEST_F(CorePageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_MergeFromFramesBySize_MainFrameLarger) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_text_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_text_paint_size = 100u;
  PopulateRequiredTimingFields(&timing);

  // Create a candidate in subframe with a smaller size.
  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  subframe_timing.paint_timing->largest_text_paint =
      base::TimeDelta::FromMilliseconds(300);
  subframe_timing.paint_timing->largest_text_paint_size = 50u;
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kDefaultTestUrl));
  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLargestContentfulPaint),
              testing::ElementsAre(base::Bucket(4780, 1)));
  EXPECT_THAT(
      tester()->histogram_tester().GetAllSamples(
          internal::kHistogramLargestContentfulPaintContentType),
      testing::ElementsAre(base::Bucket(
          static_cast<base::HistogramBase::Sample>(LargestContentType::kText),
          1)));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLargestContentfulPaintMainFrame),
              testing::ElementsAre(base::Bucket(4780, 1)));
  EXPECT_THAT(
      tester()->histogram_tester().GetAllSamples(
          internal::kHistogramLargestContentfulPaintMainFrameContentType),
      testing::ElementsAre(base::Bucket(
          static_cast<base::HistogramBase::Sample>(LargestContentType::kText),
          1)));
}

// This tests a trade-off we have made - aggregating all subframe candidates,
// which makes LCP unable to substitute the subframe candidate with a smaller
// candidate. This test provides two subframe candidates, the later larger than
// the first one.
TEST_F(CorePageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_SubframesCandidateOnlyGetLarger_Larger) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  subframe_timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  subframe_timing.paint_timing->largest_image_paint_size = 50u;
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kDefaultTestUrl));
  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  subframe_timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(300);
  subframe_timing.paint_timing->largest_image_paint_size = 10u;
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  // Ensure that the largest_image_paint timing for the main frame is recorded.
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLargestContentfulPaint),
              testing::ElementsAre(base::Bucket(4780, 1)));
  EXPECT_THAT(
      tester()->histogram_tester().GetAllSamples(
          internal::kHistogramLargestContentfulPaintContentType),
      testing::ElementsAre(base::Bucket(
          static_cast<base::HistogramBase::Sample>(LargestContentType::kImage),
          1)));
}

// This tests a trade-off we have made - aggregating all subframe candidates,
// which makes LCP unable to substitute the subframe candidate with a smaller
// candidate. This test provides two subframe candidates, the later smaller than
// the first one.
TEST_F(
    CorePageLoadMetricsObserverTest,
    LargestContentfulPaintAllFrames_SubframesCandidateOnlyGetLarger_Smaller) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  subframe_timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  subframe_timing.paint_timing->largest_image_paint_size = 10u;
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kDefaultTestUrl));
  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  subframe_timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(990);
  subframe_timing.paint_timing->largest_image_paint_size = 50u;
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  // Ensure that the largest_image_paint timing for the main frame is recorded.
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLargestContentfulPaint),
              testing::ElementsAre(base::Bucket(990, 1)));
  EXPECT_THAT(
      tester()->histogram_tester().GetAllSamples(
          internal::kHistogramLargestContentfulPaintContentType),
      testing::ElementsAre(base::Bucket(
          static_cast<base::HistogramBase::Sample>(LargestContentType::kImage),
          1)));
}

TEST_F(CorePageLoadMetricsObserverTest,
       LargestImagePaint_DiscardBackgroundResult) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  web_contents()->WasHidden();
  // This event happens after first background, so it will be discarded.
  timing.paint_timing->largest_image_paint = base::Time::Now() - base::Time();
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLargestImagePaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, LargestImagePaint_ReportLastCandidate) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  timing.navigation_start = base::Time::FromDoubleT(1);

  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(1000);
  timing.paint_timing->largest_image_paint_size = 10u;
  PopulateRequiredTimingFields(&timing);
  tester()->SimulateTimingUpdate(timing);

  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_image_paint_size = 5u;
  PopulateRequiredTimingFields(&timing);
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLargestImagePaint),
              testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(CorePageLoadMetricsObserverTest, ReportLastNullCandidate) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  timing.navigation_start = base::Time::FromDoubleT(1);

  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(1000);
  timing.paint_timing->largest_image_paint_size = 10u;

  PopulateRequiredTimingFields(&timing);
  tester()->SimulateTimingUpdate(timing);

  timing.paint_timing->largest_image_paint = base::Optional<base::TimeDelta>();
  timing.paint_timing->largest_image_paint_size = 0;
  PopulateRequiredTimingFields(&timing);
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLargestImagePaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, LargestTextPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_text_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_text_paint_size = 10u;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLargestTextPaint),
              testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(CorePageLoadMetricsObserverTest, LargestContentfulPaint_NoTextOrImage) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // When the size is 0, the timing is regarded as not set and should be
  // excluded from recording to UMA.
  timing.paint_timing->largest_text_paint_size = 0u;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLargestContentfulPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLargestContentfulPaintContentType, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLargestContentfulPaintMainFrame, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLargestContentfulPaintMainFrameContentType, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, LargestContentfulPaint_OnlyText) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_text_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_text_paint_size = 100;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLargestContentfulPaint),
              testing::ElementsAre(base::Bucket(4780, 1)));
  EXPECT_THAT(
      tester()->histogram_tester().GetAllSamples(
          internal::kHistogramLargestContentfulPaintContentType),
      testing::ElementsAre(base::Bucket(
          static_cast<base::HistogramBase::Sample>(LargestContentType::kText),
          1)));
}

TEST_F(CorePageLoadMetricsObserverTest, LargestContentfulPaint_OnlyImage) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_image_paint_size = 100;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLargestContentfulPaint),
              testing::ElementsAre(base::Bucket(4780, 1)));
  EXPECT_THAT(
      tester()->histogram_tester().GetAllSamples(
          internal::kHistogramLargestContentfulPaintContentType),
      testing::ElementsAre(base::Bucket(
          static_cast<base::HistogramBase::Sample>(LargestContentType::kImage),
          1)));
}

TEST_F(CorePageLoadMetricsObserverTest,
       LargestContentfulPaint_ImageLargerThanText) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_image_paint_size = 100;
  timing.paint_timing->largest_text_paint =
      base::TimeDelta::FromMilliseconds(1000);
  timing.paint_timing->largest_text_paint_size = 10;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLargestContentfulPaint),
              testing::ElementsAre(base::Bucket(4780, 1)));
  EXPECT_THAT(
      tester()->histogram_tester().GetAllSamples(
          internal::kHistogramLargestContentfulPaintContentType),
      testing::ElementsAre(base::Bucket(
          static_cast<base::HistogramBase::Sample>(LargestContentType::kImage),
          1)));
}

TEST_F(CorePageLoadMetricsObserverTest,
       LargestContentfulPaint_TextLargerThanImage) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_image_paint_size = 10;
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_text_paint =
      base::TimeDelta::FromMilliseconds(990);
  timing.paint_timing->largest_text_paint_size = 100;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLargestContentfulPaint),
              testing::ElementsAre(base::Bucket(990, 1)));
  EXPECT_THAT(
      tester()->histogram_tester().GetAllSamples(
          internal::kHistogramLargestContentfulPaintContentType),
      testing::ElementsAre(base::Bucket(
          static_cast<base::HistogramBase::Sample>(LargestContentType::kText),
          1)));
}

TEST_F(CorePageLoadMetricsObserverTest, ForegroundToFirstMeaningfulPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_meaningful_paint = base::TimeDelta::FromSeconds(2);
  PopulateRequiredTimingFields(&timing);

  // Simulate "Open link in new tab."
  web_contents()->WasHidden();
  NavigateAndCommit(GURL(kDefaultTestUrl));

  // First Meaningful Paint happens after tab is foregrounded.
  web_contents()->WasShown();
  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramForegroundToFirstMeaningfulPaint, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, TimeToInteractiveAlwaysForeground) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.interactive_timing->interactive =
      base::TimeDelta::FromMilliseconds(100);
  timing.interactive_timing->interactive_detection =
      base::TimeDelta::FromMilliseconds(5200);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramTimeToInteractive, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramTimeToInteractiveStatus,
      internal::TIME_TO_INTERACTIVE_RECORDED, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, TimeToInteractiveStatusBackgrounded) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.interactive_timing->interactive =
      base::TimeDelta::FromMilliseconds(100);
  timing.interactive_timing->interactive_detection =
      base::TimeDelta::FromMilliseconds(5200);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));

  // Background the tab, then foreground it.
  web_contents()->WasHidden();
  web_contents()->WasShown();

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramTimeToInteractive, 0);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramTimeToInteractiveStatus,
      internal::TIME_TO_INTERACTIVE_BACKGROUNDED, 1);
}

TEST_F(CorePageLoadMetricsObserverTest,
       TimeToInteractiveStatusUserInteractionBeforeInteractive) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_meaningful_paint =
      base::TimeDelta::FromMilliseconds(200);
  timing.interactive_timing->first_invalidating_input =
      base::TimeDelta::FromMilliseconds(1000);
  timing.interactive_timing->interactive =
      base::TimeDelta::FromMilliseconds(2000);
  timing.interactive_timing->interactive_detection =
      base::TimeDelta::FromMilliseconds(7100);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramTimeToInteractive, 0);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramTimeToInteractiveStatus,
      internal::TIME_TO_INTERACTIVE_USER_INTERACTION_BEFORE_INTERACTIVE, 1);
}

TEST_F(CorePageLoadMetricsObserverTest,
       TimeToInteractiveStatusDidNotReachQuiescence) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_meaningful_paint =
      base::TimeDelta::FromMilliseconds(200);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramTimeToInteractive, 0);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramTimeToInteractiveStatus,
      internal::TIME_TO_INTERACTIVE_DID_NOT_REACH_QUIESCENCE, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, TimeToInteractiveStatusDidNotReachFMP) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_paint = base::TimeDelta::FromMilliseconds(200);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramTimeToInteractive, 0);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramTimeToInteractiveStatus,
      internal::TIME_TO_INTERACTIVE_DID_NOT_REACH_FIRST_MEANINGFUL_PAINT, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, FirstInputDelayAndTimestamp) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.interactive_timing->first_input_delay =
      base::TimeDelta::FromMilliseconds(5);
  // Pick a value that lines up with a histogram bucket.
  timing.interactive_timing->first_input_timestamp =
      base::TimeDelta::FromMilliseconds(4780);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramFirstInputDelay4),
              testing::ElementsAre(base::Bucket(5, 1)));
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramFirstInputTimestamp4),
              testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(CorePageLoadMetricsObserverTest, LongestInputDelayAndTimestamp) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.interactive_timing->longest_input_delay =
      base::TimeDelta::FromMilliseconds(5);
  // Pick a value that lines up with a histogram bucket.
  timing.interactive_timing->longest_input_timestamp =
      base::TimeDelta::FromMilliseconds(4780);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLongestInputDelay),
              testing::ElementsAre(base::Bucket(5, 1)));
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramLongestInputTimestamp),
              testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(CorePageLoadMetricsObserverTest,
       FirstInputDelayAndTimestampBackgrounded) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.interactive_timing->first_input_delay =
      base::TimeDelta::FromMilliseconds(5);
  timing.interactive_timing->first_input_timestamp =
      base::TimeDelta::FromMilliseconds(5000);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));

  // Background the tab, then foreground it.
  web_contents()->WasHidden();
  web_contents()->WasShown();

  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstInputDelay, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstInputTimestamp, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, NavigationToBackNavigationWithGesture) {
  GURL url(kDefaultTestUrl);

  // Navigate once to the page with a user gesture.
  auto simulator =
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  simulator->SetHasUserGesture(true);
  simulator->Commit();

  // Now the user presses the back button.
  tester()->NavigateWithPageTransitionAndCommit(
      url, ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORWARD_BACK));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramUserGestureNavigationToForwardBack, 1);
}

TEST_F(CorePageLoadMetricsObserverTest,
       BrowserNavigationToBackNavigationWithGesture) {
  GURL url(kDefaultTestUrl);

  // Navigate once to the page with a user gesture.
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  simulator->SetHasUserGesture(true);
  simulator->Commit();

  // Now the user presses the back button.
  tester()->NavigateWithPageTransitionAndCommit(
      url, ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORWARD_BACK));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramUserGestureNavigationToForwardBack, 0);
}

TEST_F(CorePageLoadMetricsObserverTest,
       NavigationToBackNavigationWithoutGesture) {
  GURL url(kDefaultTestUrl);

  // Navigate once to the page with a user gesture.
  auto simulator =
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  simulator->SetHasUserGesture(false);
  simulator->Commit();

  // Now the user presses the back button.
  tester()->NavigateWithPageTransitionAndCommit(
      url, ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORWARD_BACK));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramUserGestureNavigationToForwardBack, 0);
}

TEST_F(CorePageLoadMetricsObserverTest,
       AbortedNavigationToBackNavigationWithGesture) {
  GURL url(kDefaultTestUrl);

  // Navigate once to the page with a user gesture.
  auto simulator =
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  simulator->SetHasUserGesture(true);
  simulator->Start();

  // Now the user presses the back button before the first navigation committed.
  tester()->NavigateWithPageTransitionAndCommit(
      url, ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORWARD_BACK));

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramUserGestureNavigationToForwardBack, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, UnfinishedBytesRecorded) {
  NavigateAndCommit(GURL(kDefaultTestUrl));

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  // Incomplete resource.
  resources.push_back(
      CreateResource(false /* was_cached */, 10 * 1024 /* delta_bytes */,
                     0 /* encoded_body_length */, false /* is_complete */));
  tester()->SimulateResourceDataUseUpdate(resources);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  // Verify that the unfinished resource bytes are recorded.
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramPageLoadUnfinishedBytes, 10, 1);
}

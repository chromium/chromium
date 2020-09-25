// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"

#include <memory>

#include "base/test/power_monitor_test_base.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;
using LargestContentType =
    page_load_metrics::ContentfulPaintTimingInfo::LargestContentType;

namespace {

const char kDefaultTestUrl[] = "https://google.com";
const char kDefaultTestUrlAnchor[] = "https://google.com#samepage";
const char kDefaultTestUrl2[] = "https://whatever.com";

}  // namespace

class UmaPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverContentTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<UmaPageLoadMetricsObserver>());
  }

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverContentTestHarness::SetUp();
    page_load_metrics::LargestContentfulPaintHandler::SetTestMode(true);

    base::PowerMonitor::Initialize(
        std::make_unique<base::PowerMonitorTestSource>());
  }

  void TearDown() override {
    base::PowerMonitor::ShutdownForTesting();
    page_load_metrics::PageLoadMetricsObserverContentTestHarness::TearDown();
  }

  void OnCpuTimingUpdate(RenderFrameHost* render_frame_host,
                         base::TimeDelta cpu_time_spent) {
    page_load_metrics::mojom::CpuTiming cpu_timing(cpu_time_spent);
    tester()->SimulateCpuTimingUpdate(cpu_timing, render_frame_host);
  }

  void TestNoLCP() {
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramLargestContentfulPaint, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramLargestContentfulPaintContentType, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramLargestContentfulPaintMainFrame, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramLargestContentfulPaintMainFrameContentType, 0);

    // Experimental values
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramExperimentalLargestContentfulPaint, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramExperimentalLargestContentfulPaintContentType, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramExperimentalLargestContentfulPaintMainFrame, 0);
    tester()->histogram_tester().ExpectTotalCount(
        internal::
            kHistogramExperimentalLargestContentfulPaintMainFrameContentType,
        0);
  }

  void TestAllFramesLCP(int value, LargestContentType type) {
    EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                    internal::kHistogramLargestContentfulPaint),
                testing::ElementsAre(base::Bucket(value, 1)));
    EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                    internal::kHistogramLargestContentfulPaintContentType),
                testing::ElementsAre(base::Bucket(
                    static_cast<base::HistogramBase::Sample>(type), 1)));

    // Experimental values
    EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                    internal::kHistogramExperimentalLargestContentfulPaint),
                testing::ElementsAre(base::Bucket(value, 1)));
    EXPECT_THAT(
        tester()->histogram_tester().GetAllSamples(
            internal::kHistogramExperimentalLargestContentfulPaintContentType),
        testing::ElementsAre(
            base::Bucket(static_cast<base::HistogramBase::Sample>(type), 1)));
  }

  void TestMainFrameLCP(int value, LargestContentType type) {
    EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                    internal::kHistogramLargestContentfulPaintMainFrame),
                testing::ElementsAre(base::Bucket(value, 1)));
    EXPECT_THAT(
        tester()->histogram_tester().GetAllSamples(
            internal::kHistogramLargestContentfulPaintMainFrameContentType),
        testing::ElementsAre(
            base::Bucket(static_cast<base::HistogramBase::Sample>(type), 1)));

    // Experimental values
    EXPECT_THAT(
        tester()->histogram_tester().GetAllSamples(
            internal::kHistogramExperimentalLargestContentfulPaintMainFrame),
        testing::ElementsAre(base::Bucket(value, 1)));
    EXPECT_THAT(
        tester()->histogram_tester().GetAllSamples(
            internal::
                kHistogramExperimentalLargestContentfulPaintMainFrameContentType),
        testing::ElementsAre(
            base::Bucket(static_cast<base::HistogramBase::Sample>(type), 1)));
  }

  void TestEmptyMainFrameLCP() {
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

    // Experimental LCP histograms
    EXPECT_TRUE(
        tester()
            ->histogram_tester()
            .GetAllSamples(
                internal::kHistogramExperimentalLargestContentfulPaintMainFrame)
            .empty());
    EXPECT_TRUE(
        tester()
            ->histogram_tester()
            .GetAllSamples(
                internal::
                    kHistogramExperimentalLargestContentfulPaintMainFrameContentType)
            .empty());
  }
};

TEST_F(UmaPageLoadMetricsObserverTest, NoMetrics) {
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramDomContentLoaded, 0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstImagePaint, 0);
}

TEST_F(UmaPageLoadMetricsObserverTest,
       SameDocumentNoTriggerUntilTrueNavCommit) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);

  NavigateAndCommit(GURL(kDefaultTestUrlAnchor));

  NavigateAndCommit(GURL(kDefaultTestUrl2));
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramDomContentLoaded, 0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstImagePaint, 0);
}

TEST_F(UmaPageLoadMetricsObserverTest, SingleMetricAfterCommit) {
  base::TimeDelta parse_start = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta parse_stop = base::TimeDelta::FromMilliseconds(5);
  base::TimeDelta parse_script_load_duration =
      base::TimeDelta::FromMilliseconds(3);
  base::TimeDelta parse_script_exec_duration =
      base::TimeDelta::FromMilliseconds(1);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
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

TEST_F(UmaPageLoadMetricsObserverTest, MultipleMetricsAfterCommits) {
  base::TimeDelta parse_start = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta response = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta first_image_paint = base::TimeDelta::FromMilliseconds(30);
  base::TimeDelta first_contentful_paint = first_image_paint;
  base::TimeDelta dom_content = base::TimeDelta::FromMilliseconds(40);
  base::TimeDelta load = base::TimeDelta::FromMilliseconds(100);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.response_start = response;
  timing.parse_timing->parse_start = parse_start;
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
  PopulateRequiredTimingFields(&timing2);

  tester()->SimulateTimingUpdate(timing2);

  NavigateAndCommit(GURL(kDefaultTestUrl));

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

TEST_F(UmaPageLoadMetricsObserverTest, BackgroundDifferentHistogram) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
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
      internal::kBackgroundHistogramFirstImagePaint, 0);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramDomContentLoaded, 0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstImagePaint, 0);
}

TEST_F(UmaPageLoadMetricsObserverTest, OnlyBackgroundLaterEvents) {
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

TEST_F(UmaPageLoadMetricsObserverTest, DontBackgroundQuickerLoad) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
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
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstImagePaint, 0);
}

TEST_F(UmaPageLoadMetricsObserverTest, FailedProvisionalLoad) {
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
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstImagePaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFailedProvisionalLoad, 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDuration, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDurationNoCommit, 1);
}

TEST_F(UmaPageLoadMetricsObserverTest, FailedBackgroundProvisionalLoad) {
  // Test that failed provisional event does not get logged in the
  // histogram if it happened in the background
  GURL url(kDefaultTestUrl);
  web_contents()->WasHidden();
  content::NavigationSimulator::NavigateAndFailFromDocument(
      url, net::ERR_TIMED_OUT, main_rfh());

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFailedProvisionalLoad, 0);
}

TEST_F(UmaPageLoadMetricsObserverTest, Reload) {
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

TEST_F(UmaPageLoadMetricsObserverTest, ForwardBack) {
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

TEST_F(UmaPageLoadMetricsObserverTest, NavigationTiming) {
  GURL url(kDefaultTestUrl);
  tester()->NavigateWithPageTransitionAndCommit(url, ui::PAGE_TRANSITION_LINK);
  tester()->NavigateToUntrackedUrl();

  // Verify if the elapsed times from the navigation start are recorded.
  std::vector<const char*> metrics_from_navigation_start = {
      internal::kHistogramNavigationTimingNavigationStartToFirstRequestStart,
      internal::kHistogramNavigationTimingNavigationStartToFirstResponseStart,
      internal::kHistogramNavigationTimingNavigationStartToFirstLoaderCallback,
      internal::kHistogramNavigationTimingNavigationStartToFinalRequestStart,
      internal::kHistogramNavigationTimingNavigationStartToFinalResponseStart,
      internal::kHistogramNavigationTimingNavigationStartToFinalLoaderCallback,
      internal::
          kHistogramNavigationTimingNavigationStartToNavigationCommitSent};
  for (const char* metric : metrics_from_navigation_start)
    tester()->histogram_tester().ExpectTotalCount(metric, 1);

  // Verify if the intervals between adjacent milestones are recorded.
  std::vector<const char*> metrics_between_milestones = {
      internal::kHistogramNavigationTimingFirstRequestStartToFirstResponseStart,
      internal::
          kHistogramNavigationTimingFirstResponseStartToFirstLoaderCallback,
      internal::kHistogramNavigationTimingFinalRequestStartToFinalResponseStart,
      internal::
          kHistogramNavigationTimingFinalResponseStartToFinalLoaderCallback,
      internal::
          kHistogramNavigationTimingFinalLoaderCallbackToNavigationCommitSent};
  for (const char* metric : metrics_between_milestones)
    tester()->histogram_tester().ExpectTotalCount(metric, 1);
}

TEST_F(UmaPageLoadMetricsObserverTest, NewNavigation) {
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

TEST_F(UmaPageLoadMetricsObserverTest, BytesAndResourcesCounted) {
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

TEST_F(UmaPageLoadMetricsObserverTest, CpuUsageCounted) {
  NavigateAndCommit(GURL(kDefaultTestUrl));
  OnCpuTimingUpdate(web_contents()->GetMainFrame(),
                    base::TimeDelta::FromMilliseconds(750));
  web_contents()->WasHidden();  // Set the web contents as backgrounded.
  OnCpuTimingUpdate(web_contents()->GetMainFrame(),
                    base::TimeDelta::FromMilliseconds(250));
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramPageLoadCpuTotalUsage, 1000, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramPageLoadCpuTotalUsageForegrounded, 750, 1);
}

TEST_F(UmaPageLoadMetricsObserverTest, FirstMeaningfulPaint) {
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

TEST_F(UmaPageLoadMetricsObserverTest, LargestImageLoading) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Largest image is loading so its timestamp is TimeDelta().
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta();
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      100u;
  // There is a text paint but it's smaller than image. Pick a value that lines
  // up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 70u;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  // The image was larger so LCP should NOT be reported.
  TestNoLCP();
}

TEST_F(UmaPageLoadMetricsObserverTest, LargestImageLoadingSmallerThanText) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Largest image is loading so its timestamp is TimeDelta().
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta();
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      100u;
  // There is a text paint but it's smaller than image. Pick a value that lines
  // up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 120u;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestAllFramesLCP(4780, LargestContentType::kText);
}

TEST_F(UmaPageLoadMetricsObserverTest,
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
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 100u;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
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

  TestAllFramesLCP(4780, LargestContentType::kImage);
  TestEmptyMainFrameLCP();
}

TEST_F(UmaPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_SubframeImageLoading) {
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
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta();
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 100u;
  subframe_timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromMilliseconds(500);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_text_paint_size = 80u;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
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

  TestNoLCP();
}

TEST_F(UmaPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_OnlyMainFrameProvided) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  PopulateExperimentalLCP(timing.paint_timing);
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

  TestAllFramesLCP(4780, LargestContentType::kImage);
  TestMainFrameLCP(4780, LargestContentType::kImage);
}

// This is to test whether LargestContentfulPaintAllFrames could merge
// candidates from different frames correctly. The merging will substitutes the
// existing candidate if a larger candidate from subframe is provided.
TEST_F(UmaPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_MergeFromFramesBySize_SubframeLarger) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  // Create a main frame timing with a largest_image_paint that happens late.
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(9382);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  // Create a candidate in subframe with a larger size.
  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 100u;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
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

  TestAllFramesLCP(4780, LargestContentType::kImage);
  TestMainFrameLCP(9382, LargestContentType::kImage);
}

// This is to test whether LargestContentfulPaintAllFrames could merge
// candidates from different frames correctly. The merging will substitutes the
// existing candidate if a larger candidate from main frame is provided.
TEST_F(UmaPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_MergeFromFramesBySize_MainFrameLarger) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 100u;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  // Create a candidate in subframe with a smaller size.
  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromMilliseconds(300);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_text_paint_size = 50u;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
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

  TestAllFramesLCP(4780, LargestContentType::kText);
  TestMainFrameLCP(4780, LargestContentType::kText);
}

// This tests a trade-off we have made - aggregating all subframe candidates,
// which makes LCP unable to substitute the subframe candidate with a smaller
// candidate. This test provides two subframe candidates, the later larger than
// the first one.
TEST_F(UmaPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_SubframesCandidateOnlyGetLarger_Larger) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 50u;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
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

  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(300);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 10u;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  // Ensure that the largest_image_paint timing for the main frame is recorded.
  TestAllFramesLCP(4780, LargestContentType::kImage);
}

// This tests a trade-off we have made - aggregating all subframe candidates,
// which makes LCP unable to substitute the subframe candidate with a smaller
// candidate. This test provides two subframe candidates, the later smaller than
// the first one.
TEST_F(
    UmaPageLoadMetricsObserverTest,
    LargestContentfulPaintAllFrames_SubframesCandidateOnlyGetLarger_Smaller) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 10u;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
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

  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(990);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 50u;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  // Ensure that the largest_image_paint timing for the main frame is recorded.
  TestAllFramesLCP(990, LargestContentType::kImage);
}

TEST_F(UmaPageLoadMetricsObserverTest, LargestContentfulPaint_NoTextOrImage) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // When the size is 0, the timing is regarded as not set and should be
  // excluded from recording to UMA.
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 0u;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestNoLCP();
}

TEST_F(UmaPageLoadMetricsObserverTest, LargestContentfulPaint_OnlyText) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 100;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestAllFramesLCP(4780, LargestContentType::kText);
}

TEST_F(UmaPageLoadMetricsObserverTest, LargestContentfulPaint_OnlyImage) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 100;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestAllFramesLCP(4780, LargestContentType::kImage);
}

TEST_F(UmaPageLoadMetricsObserverTest,
       LargestContentfulPaint_ImageLargerThanText) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 100;
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromMilliseconds(1000);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 10;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestAllFramesLCP(4780, LargestContentType::kImage);
}

TEST_F(UmaPageLoadMetricsObserverTest,
       LargestContentfulPaint_TextLargerThanImage) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 10;
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromMilliseconds(990);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 100;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestAllFramesLCP(990, LargestContentType::kText);
}

TEST_F(UmaPageLoadMetricsObserverTest, FirstInputDelayAndTimestamp) {
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
                  internal::kHistogramFirstInputDelay),
              testing::ElementsAre(base::Bucket(5, 1)));
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  internal::kHistogramFirstInputTimestamp),
              testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(UmaPageLoadMetricsObserverTest, LongestInputDelayAndTimestamp) {
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

TEST_F(UmaPageLoadMetricsObserverTest,
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

TEST_F(UmaPageLoadMetricsObserverTest, NavigationToBackNavigationWithGesture) {
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

TEST_F(UmaPageLoadMetricsObserverTest,
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

TEST_F(UmaPageLoadMetricsObserverTest,
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

TEST_F(UmaPageLoadMetricsObserverTest,
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

TEST_F(UmaPageLoadMetricsObserverTest, UnfinishedBytesRecorded) {
  NavigateAndCommit(GURL(kDefaultTestUrl));

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  // Incomplete resource.
  resources.push_back(
      CreateResource(false /* was_cached */, 10 * 1024 /* delta_bytes */,
                     0 /* encoded_body_length */, 0 /* decoded_body_length */,
                     false /* is_complete */));
  tester()->SimulateResourceDataUseUpdate(resources);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  // Verify that the unfinished resource bytes are recorded.
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramPageLoadUnfinishedBytes, 10, 1);
}

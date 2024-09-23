// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_trace_processor.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/page_load_metrics_memory_tracker.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;
using LargestContentTextOrImage =
    page_load_metrics::ContentfulPaintTimingInfo::LargestContentTextOrImage;
using UserInteractionLatenciesPtr =
    page_load_metrics::mojom::UserInteractionLatenciesPtr;
using UserInteractionLatencies =
    page_load_metrics::mojom::UserInteractionLatencies;
using UserInteractionLatency = page_load_metrics::mojom::UserInteractionLatency;
using UserInteractionType = page_load_metrics::mojom::UserInteractionType;

namespace {

const char kDefaultTestUrl[] = "https://google.com";
const char kDefaultTestUrlAnchor[] = "https://google.com#samepage";
const char kDefaultTestUrl2[] = "https://whatever.com";

const char kHistogramFirstContentfulPaintDataScheme[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.DataScheme";
const char kHistogramFirstContentfulPaintFileScheme[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.FileScheme";

}  // namespace

class UmaPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverContentTestHarness,
      public testing::WithParamInterface<bool>,
      public content::WebContentsObserver {
 public:
  using page_load_metrics::PageLoadMetricsObserverContentTestHarness::
      web_contents;
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<UmaPageLoadMetricsObserver>());
  }

  ::base::test::TracingEnvironment tracing_environment_;

 protected:
  bool WithFencedFrames() { return GetParam(); }

  content::RenderFrameHost* AppendChildFrame(content::RenderFrameHost* parent,
                                             const char* frame_name) {
    if (WithFencedFrames()) {
      return content::RenderFrameHostTester::For(parent)->AppendFencedFrame();
    } else {
      return content::RenderFrameHostTester::For(parent)->AppendChild(
          frame_name);
    }
  }

  content::RenderFrameHost* AppendChildFrameAndNavigateAndCommit(
      content::RenderFrameHost* parent,
      const char* frame_name,
      const GURL& url) {
    content::RenderFrameHost* subframe = AppendChildFrame(parent, frame_name);
    std::unique_ptr<NavigationSimulator> simulator =
        NavigationSimulator::CreateRendererInitiated(url, subframe);
    simulator->Commit();
    return simulator->GetFinalRenderFrameHost();
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kV8PerFrameMemoryMonitoring);
    page_load_metrics::PageLoadMetricsObserverContentTestHarness::SetUp();
    page_load_metrics::LargestContentfulPaintHandler::SetTestMode(true);
    WebContentsObserver::Observe(web_contents());
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
  }

  void TestAllFramesLCP(int value, LargestContentTextOrImage text_or_image) {
    EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                    internal::kHistogramLargestContentfulPaint),
                testing::ElementsAre(base::Bucket(value, 1)));
    EXPECT_THAT(
        tester()->histogram_tester().GetAllSamples(
            internal::kHistogramLargestContentfulPaintContentType),
        testing::ElementsAre(base::Bucket(
            static_cast<base::HistogramBase::Sample>(text_or_image), 1)));
  }

  void TestCrossSiteSubFrameLCP(int value) {
    EXPECT_THAT(
        tester()->histogram_tester().GetAllSamples(
            internal::kHistogramLargestContentfulPaintCrossSiteSubFrame),
        testing::ElementsAre((base::Bucket(value, 1))));
  }

  void TestMainFrameLCP(int value, LargestContentTextOrImage text_or_image) {
    EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                    internal::kHistogramLargestContentfulPaintMainFrame),
                testing::ElementsAre(base::Bucket(value, 1)));
    EXPECT_THAT(
        tester()->histogram_tester().GetAllSamples(
            internal::kHistogramLargestContentfulPaintMainFrameContentType),
        testing::ElementsAre(base::Bucket(
            static_cast<base::HistogramBase::Sample>(text_or_image), 1)));
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
  }

  void TestHistogram(const char* name,
                     std::vector<base::Bucket> buckets,
                     const base::Location& location = FROM_HERE) {
    EXPECT_THAT(tester()->histogram_tester().GetAllSamples(name),
                base::BucketsAreArray(buckets))
        << location.ToString();
  }

  const base::HistogramTester& histogram_tester() {
    return tester()->histogram_tester();
  }

  void SimulateV8MemoryChange(content::RenderFrameHost* render_frame_host,
                              int64_t delta_bytes) {
    tester()->SimulateMemoryUpdate(render_frame_host, delta_bytes);
  }

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    last_navigation_id_ = navigation_handle->GetNavigationId();
  }

  int64_t last_navigation_id_ = -1;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, UmaPageLoadMetricsObserverTest, testing::Bool());

TEST_P(UmaPageLoadMetricsObserverTest, NoMetrics) {
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramDomContentLoaded, 0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstImagePaint, 0);
}

TEST_P(UmaPageLoadMetricsObserverTest,
       SameDocumentNoTriggerUntilTrueNavCommit) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
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

TEST_P(UmaPageLoadMetricsObserverTest, SingleMetricAfterCommit) {
  base::TimeDelta parse_start = base::Milliseconds(1);
  base::TimeDelta parse_stop = base::Milliseconds(5);
  base::TimeDelta parse_script_load_duration = base::Milliseconds(3);
  base::TimeDelta parse_script_exec_duration = base::Milliseconds(1);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
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

TEST_P(UmaPageLoadMetricsObserverTest, MultipleMetricsAfterCommits) {
  base::TimeDelta parse_start = base::Milliseconds(1);
  base::TimeDelta response = base::Milliseconds(1);
  base::TimeDelta first_image_paint = base::Milliseconds(30);
  base::TimeDelta first_contentful_paint = first_image_paint;
  base::TimeDelta dom_content = base::Milliseconds(40);
  base::TimeDelta load = base::Milliseconds(100);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
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
  tester()->histogram_tester().ExpectTotalCount(
      kHistogramFirstContentfulPaintDataScheme, 0);
  tester()->histogram_tester().ExpectTotalCount(
      kHistogramFirstContentfulPaintFileScheme, 0);

  NavigateAndCommit(GURL(kDefaultTestUrl2));

  page_load_metrics::mojom::PageLoadTiming timing2;
  page_load_metrics::InitPageLoadTimingForTest(&timing2);
  timing2.navigation_start = base::Time::FromSecondsSinceUnixEpoch(200);
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

TEST_P(UmaPageLoadMetricsObserverTest,
       PaintMetricsAreNotRecordedForDataScheme) {
  base::TimeDelta first_contentful_paint = base::Milliseconds(30);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.response_start = base::Milliseconds(1);
  timing.paint_timing->first_paint = first_contentful_paint;
  timing.paint_timing->first_contentful_paint = first_contentful_paint;

  NavigateAndCommit(GURL("data:text/html,Hello world"));
  tester()->SimulateTimingUpdate(timing);

  // This class does not observe the data:// scheme,
  // so FCP and LCP should not be recorded.
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstContentfulPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLargestContentfulPaint, 0);
}

TEST_P(UmaPageLoadMetricsObserverTest,
       PaintMetricsAreNotRecordedForFileScheme) {
  base::TimeDelta first_contentful_paint = base::Milliseconds(30);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.response_start = base::Milliseconds(1);
  timing.paint_timing->first_paint = first_contentful_paint;
  timing.paint_timing->first_contentful_paint = first_contentful_paint;

  NavigateAndCommit(GURL("file:///file.txt"));
  tester()->SimulateTimingUpdate(timing);

  // This class does not observe the file:// scheme,
  // so FCP and LCP should not be recorded.
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstContentfulPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramLargestContentfulPaint, 0);
}

TEST_P(UmaPageLoadMetricsObserverTest, BackgroundDifferentHistogram) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
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

TEST_F(UmaPageLoadMetricsObserverTest,
       RelevantBackgroundMetricsAreRecordedForHttpsScheme) {
  base::TimeDelta first_contentful_paint = base::Milliseconds(30);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.response_start = base::Milliseconds(1);
  timing.paint_timing->first_paint = first_contentful_paint;
  timing.paint_timing->first_contentful_paint = first_contentful_paint;
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(15);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 10;

  // Send the page to background.
  web_contents()->WasHidden();

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);

  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.Background."
      "HttpsOrDataOrFileScheme",
      first_contentful_paint.InMilliseconds(), 1);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl));

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.Background."
      "HttpsOrDataOrFileScheme",
      1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.Background."
      "HttpsOrDataOrFileScheme",
      timing.paint_timing->largest_contentful_paint->largest_text_paint
          ->InMilliseconds(),
      1);
}

TEST_P(UmaPageLoadMetricsObserverTest, OnlyBackgroundLaterEvents) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.document_timing->dom_content_loaded_event_start =
      base::Microseconds(1);
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
  timing.paint_timing->first_image_paint = base::Seconds(4);
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

TEST_P(UmaPageLoadMetricsObserverTest, DontBackgroundQuickerLoad) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
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

TEST_P(UmaPageLoadMetricsObserverTest, FailedProvisionalLoad) {
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
      internal::kHistogramPageTimingForegroundDuration, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDurationNoCommit, 1);
}

TEST_P(UmaPageLoadMetricsObserverTest, Reload) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(5);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(10);
  PopulateRequiredTimingFields(&timing);

  GURL url(kDefaultTestUrl);
  tester()->NavigateWithPageTransitionAndCommit(url,
                                                ui::PAGE_TRANSITION_RELOAD);
  tester()->SimulateTimingUpdate(timing);

  auto resources =
      GetSampleResourceDataUpdateForTesting(10 * 1024 /* resource_size */);
  tester()->SimulateResourceDataUseUpdate(resources);

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
}

TEST_P(UmaPageLoadMetricsObserverTest, ForwardBack) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(5);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(10);
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
}

TEST_P(UmaPageLoadMetricsObserverTest, NavigationTiming) {
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

TEST_P(UmaPageLoadMetricsObserverTest, NewNavigation) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(5);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(10);
  PopulateRequiredTimingFields(&timing);

  GURL url(kDefaultTestUrl);
  tester()->NavigateWithPageTransitionAndCommit(url, ui::PAGE_TRANSITION_LINK);
  tester()->SimulateTimingUpdate(timing);

  auto resources =
      GetSampleResourceDataUpdateForTesting(10 * 1024 /* resource_size */);
  tester()->SimulateResourceDataUseUpdate(resources);

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
}

TEST_P(UmaPageLoadMetricsObserverTest, BytesAndResourcesCounted) {
  NavigateAndCommit(GURL(kDefaultTestUrl));
  NavigateAndCommit(GURL(kDefaultTestUrl2));
}

TEST_P(UmaPageLoadMetricsObserverTest, CpuUsageCounted) {
  NavigateAndCommit(GURL(kDefaultTestUrl));
  OnCpuTimingUpdate(web_contents()->GetPrimaryMainFrame(),
                    base::Milliseconds(750));
  web_contents()->WasHidden();  // Set the web contents as backgrounded.
  OnCpuTimingUpdate(web_contents()->GetPrimaryMainFrame(),
                    base::Milliseconds(250));
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramPageLoadCpuTotalUsage, 1000, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramPageLoadCpuTotalUsageForegrounded, 750, 1);
}

TEST_P(UmaPageLoadMetricsObserverTest, LargestImageLoading) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  // Largest image is loading so its timestamp is TimeDelta().
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta();
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      100u;
  // There is a text paint but it's smaller than image. Pick a value that lines
  // up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 70u;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  // The image was larger so LCP should NOT be reported.
  TestNoLCP();
}

TEST_P(UmaPageLoadMetricsObserverTest, LargestImageLoadingSmallerThanText) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  // Largest image is loading so its timestamp is TimeDelta().
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta();
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      100u;
  // There is a text paint but it's smaller than image. Pick a value that lines
  // up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 120u;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestAllFramesLCP(4780, LargestContentTextOrImage::kText);
}

TEST_P(UmaPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_OnlySubframeProvided) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(100);
  // Intentionally not set candidates for the main frame.
  PopulateRequiredTimingFields(&timing);

  // Create a subframe timing with a largest_image_paint.
  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(200);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 100u;
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kDefaultTestUrl));
  RenderFrameHost* subframe = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe",
      GURL(kSubframeTestUrl));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestAllFramesLCP(4780, LargestContentTextOrImage::kImage);
  TestEmptyMainFrameLCP();
}

TEST_P(UmaPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_SubframeImageLoading) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(100);
  // Intentionally not set candidates for the main frame.
  PopulateRequiredTimingFields(&timing);

  // Create a subframe timing with a largest_image_paint.
  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(200);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta();
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 100u;
  subframe_timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(500);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_text_paint_size = 80u;
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kDefaultTestUrl));
  RenderFrameHost* subframe = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe",
      GURL(kSubframeTestUrl));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestNoLCP();
}

TEST_P(UmaPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_OnlyMainFrameProvided) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  // Intentionally not set candidates for the subframes.
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kDefaultTestUrl));
  RenderFrameHost* subframe = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe",
      GURL(kSubframeTestUrl));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestAllFramesLCP(4780, LargestContentTextOrImage::kImage);
  TestMainFrameLCP(4780, LargestContentTextOrImage::kImage);
}

// This is to test whether LargestContentfulPaintAllFrames could merge
// candidates from different frames correctly. The merging will substitutes the
// existing candidate if a larger candidate from subframe is provided.
TEST_P(UmaPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_MergeFromFramesBySize_SubframeLarger) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  // Create a main frame timing with a largest_image_paint that happens late.
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(9382);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  PopulateRequiredTimingFields(&timing);

  // Create a candidate in subframe with a larger size.
  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 100u;
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kDefaultTestUrl));
  RenderFrameHost* subframe = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe",
      GURL(kSubframeTestUrl));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestAllFramesLCP(4780, LargestContentTextOrImage::kImage);
  TestMainFrameLCP(9382, LargestContentTextOrImage::kImage);
}

// This is to test whether LargestContentfulPaintAllFrames could merge
// candidates from different frames correctly. The merging will substitutes the
// existing candidate if a larger candidate from main frame is provided.
TEST_P(UmaPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_MergeFromFramesBySize_MainFrameLarger) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 100u;
  PopulateRequiredTimingFields(&timing);

  // Create a candidate in subframe with a smaller size.
  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(300);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_text_paint_size = 50u;
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kDefaultTestUrl));
  RenderFrameHost* subframe = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe",
      GURL(kSubframeTestUrl));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestAllFramesLCP(4780, LargestContentTextOrImage::kText);
  TestMainFrameLCP(4780, LargestContentTextOrImage::kText);
}

// This tests a trade-off we have made - aggregating all subframe candidates,
// which makes LCP unable to substitute the subframe candidate with a smaller
// candidate. This test provides two subframe candidates, the later larger than
// the first one.
TEST_P(UmaPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_SubframesCandidateOnlyGetLarger_Larger) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 50u;
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kDefaultTestUrl));
  RenderFrameHost* subframe = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe",
      GURL(kSubframeTestUrl));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(300);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 10u;
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  // Ensure that the largest_image_paint timing for the main frame is recorded.
  TestAllFramesLCP(4780, LargestContentTextOrImage::kImage);
}

// This tests a trade-off we have made - aggregating all subframe candidates,
// which makes LCP unable to substitute the subframe candidate with a smaller
// candidate. This test provides two subframe candidates, the later smaller than
// the first one.
TEST_P(
    UmaPageLoadMetricsObserverTest,
    LargestContentfulPaintAllFrames_SubframesCandidateOnlyGetLarger_Smaller) {
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 10u;
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kDefaultTestUrl));
  RenderFrameHost* subframe = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe",
      GURL(kSubframeTestUrl));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(990);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 50u;
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  // Ensure that the largest_image_paint timing for the main frame is recorded.
  TestAllFramesLCP(990, LargestContentTextOrImage::kImage);
}

TEST_P(UmaPageLoadMetricsObserverTest, LargestContentfulPaint_NoTextOrImage) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  // When the size is 0, the timing is regarded as not set and should be
  // excluded from recording to UMA.
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 0u;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestNoLCP();
}

TEST_P(UmaPageLoadMetricsObserverTest, LargestContentfulPaint_OnlyText) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 100;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestAllFramesLCP(4780, LargestContentTextOrImage::kText);
}

TEST_P(UmaPageLoadMetricsObserverTest, LargestContentfulPaint_OnlyImage) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 100;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestAllFramesLCP(4780, LargestContentTextOrImage::kImage);
}

TEST_P(UmaPageLoadMetricsObserverTest,
       LargestContentfulPaint_ImageLargerThanText) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 100;
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(1000);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 10;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestAllFramesLCP(4780, LargestContentTextOrImage::kImage);
}

TEST_P(UmaPageLoadMetricsObserverTest,
       LargestContentfulPaint_TextLargerThanImage) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 10;
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(990);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 100;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestAllFramesLCP(990, LargestContentTextOrImage::kText);
}

TEST_P(UmaPageLoadMetricsObserverTest, NormalizedResponsivenessMetrics) {
  page_load_metrics::mojom::InputTiming input_timing;
  input_timing.num_interactions = 3;
  input_timing.max_event_durations =
      UserInteractionLatencies::NewUserInteractionLatencies({});
  auto& max_event_durations =
      input_timing.max_event_durations->get_user_interaction_latencies();
  base::TimeTicks current_time = base::TimeTicks::Now();
  max_event_durations.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(50), UserInteractionType::kKeyboard, 0,
      current_time + base::Milliseconds(1000)));
  max_event_durations.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(100), UserInteractionType::kTapOrClick, 1,
      current_time + base::Milliseconds(2000)));
  max_event_durations.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(150), UserInteractionType::kDrag, 2,
      current_time + base::Milliseconds(3000)));
  NavigateAndCommit(GURL(kDefaultTestUrl));
  tester()->SimulateInputTimingUpdate(input_timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  std::vector<std::pair<std::string, int>> uma_list = {
      std::make_pair(
          internal::kHistogramWorstUserInteractionLatencyMaxEventDuration, 146),
      std::make_pair(
          internal::
              kHistogramUserInteractionLatencyHighPercentile2MaxEventDuration,
          146),
      std::make_pair(internal::kHistogramInpOffset, 2),
      std::make_pair(internal::kHistogramNumInteractions, 3)};

  for (auto& metric : uma_list) {
    EXPECT_THAT(
        tester()->histogram_tester().GetAllSamples(metric.first.c_str()),
        // metric.second is the minimum value of the bucket, not the
        // actual value.
        testing::ElementsAre(base::Bucket(metric.second, 1)));
  }
}

TEST_P(UmaPageLoadMetricsObserverTest, FirstInputDelayAndTimestamp) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.interactive_timing->first_input_delay = base::Milliseconds(5);
  // Pick a value that lines up with a histogram bucket.
  timing.interactive_timing->first_input_timestamp = base::Milliseconds(4780);
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

TEST_P(UmaPageLoadMetricsObserverTest,
       FirstInputDelayAndTimestampBackgrounded) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.interactive_timing->first_input_delay = base::Milliseconds(5);
  timing.interactive_timing->first_input_timestamp = base::Milliseconds(5000);
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

TEST_P(UmaPageLoadMetricsObserverTest, NavigationToBackNavigationWithGesture) {
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

TEST_P(UmaPageLoadMetricsObserverTest,
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

TEST_P(UmaPageLoadMetricsObserverTest,
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

TEST_P(UmaPageLoadMetricsObserverTest,
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

TEST_P(UmaPageLoadMetricsObserverTest, MainFrame_MaxMemoryBytesRecorded) {
  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kDefaultTestUrl));

  // Notify that memory measurements are available for the main frame.
  SimulateV8MemoryChange(main_rfh(), 100 * 1024);

  // Simulate positive and negative shifts to memory usage and ensure the
  // maximum value is properly tracked.
  SimulateV8MemoryChange(main_rfh(), 50 * 1024);
  SimulateV8MemoryChange(main_rfh(), -150 * 1024);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectUniqueSample(internal::kHistogramMemoryMainframe,
                                        150, 1);
  histogram_tester().ExpectUniqueSample(
      internal::kHistogramMemorySubframeAggregate, 0, 1);
  histogram_tester().ExpectUniqueSample(internal::kHistogramMemoryTotal, 150,
                                        1);
}

TEST_P(UmaPageLoadMetricsObserverTest, SingleSubFrame_MaxMemoryBytesRecorded) {
  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kDefaultTestUrl));

  RenderFrameHost* subframe = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe",
      GURL("https://google.com/subframe.html"));

  // Notify that memory measurements are available for each frame.
  SimulateV8MemoryChange(main_rfh(), 100 * 1024);
  SimulateV8MemoryChange(subframe, 10 * 1024);

  // Simulate positive and negative shifts to memory usage and ensure the
  // maximum value is properly tracked.
  SimulateV8MemoryChange(subframe, 30 * 1024);
  SimulateV8MemoryChange(subframe, -20 * 1024);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectUniqueSample(internal::kHistogramMemoryMainframe,
                                        100, 1);
  histogram_tester().ExpectUniqueSample(
      internal::kHistogramMemorySubframeAggregate, 40, 1);
  histogram_tester().ExpectUniqueSample(internal::kHistogramMemoryTotal, 140,
                                        1);
}

TEST_P(UmaPageLoadMetricsObserverTest, MultiSubFrames_MaxMemoryBytesRecorded) {
  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kDefaultTestUrl));

  RenderFrameHost* subframe1 = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe1",
      GURL("https://google.com/subframe.html"));
  RenderFrameHost* subframe2 = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe2",
      GURL("https://google.com/subframe2.html"));
  RenderFrameHost* subframe3 = AppendChildFrameAndNavigateAndCommit(
      subframe2, "subframe3", GURL("https://google.com/subframe3.html"));

  // Notify that memory measurements are available for each frame.
  SimulateV8MemoryChange(main_rfh(), 500 * 1024);
  SimulateV8MemoryChange(subframe1, 10 * 1024);
  SimulateV8MemoryChange(subframe2, 20 * 1024);
  SimulateV8MemoryChange(subframe3, 30 * 1024);

  // Simulate positive and negative shifts to memory usage and ensure the
  // maximum value is properly tracked.
  SimulateV8MemoryChange(main_rfh(), 100 * 1024);
  SimulateV8MemoryChange(subframe1, 5 * 1024);
  SimulateV8MemoryChange(subframe1, -2 * 1024);
  SimulateV8MemoryChange(subframe2, 5 * 1024);
  SimulateV8MemoryChange(subframe2, -2 * 1024);
  SimulateV8MemoryChange(main_rfh(), -200 * 1024);
  SimulateV8MemoryChange(subframe3, 5 * 1024);
  SimulateV8MemoryChange(subframe3, -2 * 1024);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectUniqueSample(internal::kHistogramMemoryMainframe,
                                        500 + 100, 1);
  histogram_tester().ExpectUniqueSample(
      internal::kHistogramMemorySubframeAggregate,
      10 + 20 + 30 + 5 - 2 + 5 - 2 + 5, 1);
  histogram_tester().ExpectUniqueSample(
      internal::kHistogramMemoryTotal, 500 + 10 + 20 + 30 + 100 + 5 - 2 + 5, 1);
}

TEST_P(UmaPageLoadMetricsObserverTest,
       CrossSiteSubframeLargestContentfulPaint_SubframeLarger) {
  // Use the page having no subframes to record the passed LCP candidate.
  const char kNoSubFramesTestUrl[] = "https://example.com";
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";
  // Create a main frame.
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(9382);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  PopulateRequiredTimingFields(&timing);

  // Create a candidate in subframe with a larger size.
  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 100u;
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kNoSubFramesTestUrl));
  RenderFrameHost* subframe = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe",
      GURL(kSubframeTestUrl));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestCrossSiteSubFrameLCP(4780);
}

TEST_P(UmaPageLoadMetricsObserverTest,
       CrossSiteSubframeLargestContentfulPaint_SubframeSmaller) {
  // Use the page having no subframes to record the passed LCP candidate.
  const char kNoSubFramesTestUrl[] = "https://example.com";
  const char kSubframeTestUrl[] = "https://google.com/subframe.html";
  // Create a main frame.
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(900);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  PopulateRequiredTimingFields(&timing);

  // Create a candidate in subframe with a smaller size.
  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 30u;
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kNoSubFramesTestUrl));
  RenderFrameHost* subframe = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe",
      GURL(kSubframeTestUrl));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  TestCrossSiteSubFrameLCP(4780);
}

TEST_P(UmaPageLoadMetricsObserverTest,
       CrossSiteSubframeLargestContentfulPaint_MultiSubframes) {
  // Use the page having no subframes to record the passed LCP candidate.
  const char kNoSubFramesTestUrl[] = "https://example.com";
  const char kSameSiteSubFrameTestUrl[] =
      "https://same-site.example.com/subframe.html";
  const char kCrossSiteSubFrameTestUrl[] = "https://google.com/subframe.html";
  // Create a main frame.
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(900);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  PopulateRequiredTimingFields(&timing);

  // Create a candidates in subframes from same-site and cross-site
  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(3000);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 100u;
  PopulateRequiredTimingFields(&subframe_timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing2;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing2);
  subframe_timing2.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  subframe_timing2.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  subframe_timing2.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 30u;
  PopulateRequiredTimingFields(&subframe_timing2);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kNoSubFramesTestUrl));

  RenderFrameHost* first_party_subframe = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe1",
      GURL(kSameSiteSubFrameTestUrl));
  RenderFrameHost* cross_site_subframe = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe2",
      GURL(kCrossSiteSubFrameTestUrl));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, first_party_subframe);
  tester()->SimulateTimingUpdate(subframe_timing2, cross_site_subframe);

  // Navigate again to force histogram recording in the main frame.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  // Make sure subframe LCP from same-site is ignored
  TestCrossSiteSubFrameLCP(4780);
}

// The following tests are ensure that Page Load metrics are recorded in a
// trace. Currently enabled only for platforms where USE_PERFETTO_CLIENT_LIBRARY
// is true (Android, Linux) as test infra (TestTraceProcessor) requires it.
TEST_F(UmaPageLoadMetricsObserverTest, TestTracingFirstContentfulPaint) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("interactions");

  base::TimeDelta parse_start = base::Milliseconds(1);
  base::TimeDelta response = base::Milliseconds(1);
  base::TimeDelta first_image_paint = base::Milliseconds(30);
  base::TimeDelta first_contentful_paint = first_image_paint;
  base::TimeDelta dom_content = base::Milliseconds(40);
  base::TimeDelta load = base::Milliseconds(100);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.response_start = response;
  timing.parse_timing->parse_start = parse_start;
  timing.paint_timing->first_image_paint = first_image_paint;
  timing.paint_timing->first_contentful_paint = first_contentful_paint;
  timing.document_timing->dom_content_loaded_event_start = dom_content;
  timing.document_timing->load_event_start = load;
  PopulateRequiredTimingFields(&timing);

  GURL url(kDefaultTestUrl);
  NavigateAndCommit(url);
  tester()->SimulateTimingUpdate(timing);

  // Ensure that the "PageLoadMetrics.NavigationToFirstContentfulPaint" trace
  // event is emitted.
  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  std::string query = R"(
    SELECT
      EXTRACT_ARG(arg_set_id, 'page_load.url') AS url,
      EXTRACT_ARG(arg_set_id, 'page_load.navigation_id')
        AS navigation_id
    FROM slice
    WHERE name = 'PageLoadMetrics.NavigationToFirstContentfulPaint'
  )";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(
      result.value(),
      ::testing::ElementsAre(
          std::vector<std::string>{"url", "navigation_id"},
          std::vector<std::string>{url.possibly_invalid_spec(),
                                   base::NumberToString(last_navigation_id_)}));
}

TEST_F(UmaPageLoadMetricsObserverTest, TestTracingLargestContentfulPaint) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("interactions");

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 10;
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(990);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 100;
  PopulateRequiredTimingFields(&timing);

  GURL url(kDefaultTestUrl);
  NavigateAndCommit(url);
  tester()->SimulateTimingUpdate(timing);
  int64_t navigation_id = last_navigation_id_;
  // Navigate again to force histogram recording. This also increments the
  // navigation id.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  std::string query = R"(
    SELECT
      EXTRACT_ARG(arg_set_id, 'page_load.navigation_id')
        AS navigation_id
    FROM slice
    WHERE name = 'PageLoadMetrics.NavigationToLargestContentfulPaint'
  )";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"navigation_id"},
                                     std::vector<std::string>{
                                         base::NumberToString(navigation_id)}));
}

TEST_F(UmaPageLoadMetricsObserverTest, TestTracingLoadEventStart) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("interactions");

  base::TimeDelta load = base::Milliseconds(100);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.document_timing->load_event_start = load;
  PopulateRequiredTimingFields(&timing);

  GURL url(kDefaultTestUrl);
  NavigateAndCommit(url);
  tester()->SimulateTimingUpdate(timing);
  int64_t navigation_id = last_navigation_id_;
  // Navigate again to force histogram recording. This also increments the
  // navigation id.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  std::string query = R"(
    SELECT
      EXTRACT_ARG(arg_set_id, 'page_load.navigation_id')
        AS navigation_id
    FROM slice
    WHERE name = 'PageLoadMetrics.NavigationToMainFrameOnLoad'
  )";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"navigation_id"},
                                     std::vector<std::string>{
                                         base::NumberToString(navigation_id)}));
}

TEST_F(UmaPageLoadMetricsObserverTest, TestTracingDomContentLoadedEventStart) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("interactions");

  base::TimeDelta dom_content = base::Milliseconds(40);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.document_timing->dom_content_loaded_event_start = dom_content;
  PopulateRequiredTimingFields(&timing);

  GURL url(kDefaultTestUrl);
  NavigateAndCommit(url);
  tester()->SimulateTimingUpdate(timing);
  int64_t navigation_id = last_navigation_id_;
  // Navigate again to force histogram recording. This also increments the
  // navigation id.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  std::string query = R"(
    SELECT
      EXTRACT_ARG(arg_set_id, 'page_load.navigation_id')
        AS navigation_id
    FROM slice
    WHERE name = 'PageLoadMetrics.NavigationToDOMContentLoadedEventFired'
  )";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"navigation_id"},
                                     std::vector<std::string>{
                                         base::NumberToString(navigation_id)}));
}

TEST_P(UmaPageLoadMetricsObserverTest, LCPSpeculationRulesPrerender) {
  const int kExpected = 4780;
  const char* kHistogram =
      internal::kHistogramLargestContentfulPaintSetSpeculationRulesPrerender;

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(kExpected);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 120u;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL("https://a.test"));
  content::test::SetHasSpeculationRulesPrerender(
      content::PreloadingData::GetOrCreateForWebContents(web_contents()));
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL("https://b.test"));
  TestHistogram(kHistogram, {{kExpected, 1}});

  content::PreloadingData::GetOrCreateForWebContents(web_contents());
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording without setting the flag.
  NavigateAndCommit(GURL("https://c.test"));
  TestHistogram(kHistogram, {{kExpected, 1}});
}

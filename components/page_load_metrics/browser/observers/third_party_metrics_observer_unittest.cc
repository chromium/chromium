// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/third_party_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gmock/include/gmock/gmock.h"

const char kReadCookieHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.CookieRead2";
const char kSubframeFCPHistogram[] =
    "PageLoad.Clients.ThirdParty.Frames.NavigationToFirstContentfulPaint3";

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;

class ThirdPartyMetricsObserverTestBase
    : public page_load_metrics::PageLoadMetricsObserverContentTestHarness {
 public:
  ThirdPartyMetricsObserverTestBase(const ThirdPartyMetricsObserverTestBase&) =
      delete;
  ThirdPartyMetricsObserverTestBase& operator=(
      const ThirdPartyMetricsObserverTestBase&) = delete;

 protected:
  ThirdPartyMetricsObserverTestBase() = default;

  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::WrapUnique(new ThirdPartyMetricsObserver()));
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* NavigateFrame(const std::string& url,
                                 content::RenderFrameHost* frame) {
    auto navigation_simulator =
        NavigationSimulator::CreateRendererInitiated(GURL(url), frame);
    navigation_simulator->Commit();
    return navigation_simulator->GetFinalRenderFrameHost();
  }

  // Returns the final RenderFrameHost after navigation commits.
  content::RenderFrameHost* NavigateMainFrame(const std::string& url) {
    return NavigateFrame(url, web_contents()->GetPrimaryMainFrame());
  }

  RenderFrameHost* AppendChildFrame(content::RenderFrameHost* parent) {
    if (WithFencedFrames()) {
      return content::RenderFrameHostTester::For(parent)->AppendFencedFrame();
    }
    return content::RenderFrameHostTester::For(parent)->AppendChild("iframe");
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* CreateAndNavigateSubFrame(const std::string& url,
                                             content::RenderFrameHost* parent) {
    return NavigateFrame(url, AppendChildFrame(parent));
  }

  virtual bool WithFencedFrames() = 0;
};

class ThirdPartyMetricsObserverTest : public ThirdPartyMetricsObserverTestBase,
                                      public testing::WithParamInterface<bool> {
 private:
  bool WithFencedFrames() override { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All, ThirdPartyMetricsObserverTest, testing::Bool());

TEST_P(ThirdPartyMetricsObserverTest, NoThirdPartyFrame_NoneRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame("https://top.com");
  RenderFrameHost* sub_frame =
      CreateAndNavigateSubFrame("https://a.top.com/foo", main_frame);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.paint_timing->first_contentful_paint = base::Seconds(1);
  tester()->SimulateTimingUpdate(timing, sub_frame);
  tester()->histogram_tester().ExpectTotalCount(kSubframeFCPHistogram, 0);
}

TEST_P(ThirdPartyMetricsObserverTest, OneThirdPartyFrame_OneRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame("https://top.com");
  RenderFrameHost* sub_frame =
      CreateAndNavigateSubFrame("https://x-origin.com", main_frame);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.paint_timing->first_contentful_paint = base::Seconds(1);
  tester()->SimulateTimingUpdate(timing, sub_frame);
  tester()->histogram_tester().ExpectUniqueSample(kSubframeFCPHistogram, 1000,
                                                  1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       OneThirdPartyFrameWithTwoSameUpdates_OneRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame("https://top.com");
  RenderFrameHost* sub_frame =
      CreateAndNavigateSubFrame("https://x-origin.com", main_frame);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.paint_timing->first_contentful_paint = base::Seconds(1);
  tester()->SimulateTimingUpdate(timing, sub_frame);
  tester()->SimulateTimingUpdate(timing, sub_frame);
  tester()->histogram_tester().ExpectUniqueSample(kSubframeFCPHistogram, 1000,
                                                  1);
}

TEST_P(ThirdPartyMetricsObserverTest, SixtyFrames_FiftyRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame("https://top.com");

  // Add more frames than we're supposed to track.
  for (int i = 0; i < 60; ++i) {
    RenderFrameHost* sub_frame =
        CreateAndNavigateSubFrame("https://x-origin.com", main_frame);

    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.paint_timing->first_contentful_paint = base::Seconds(1);
    tester()->SimulateTimingUpdate(timing, sub_frame);
  }

  // Keep this synchronized w/ the max frame count in the cc file.
  tester()->histogram_tester().ExpectTotalCount(kSubframeFCPHistogram, 50);
}

TEST_P(ThirdPartyMetricsObserverTest, ThreeThirdPartyFrames_ThreeRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame("https://top.com");

  // Create three third-party frames.
  RenderFrameHost* sub_frame_a =
      CreateAndNavigateSubFrame("https://x-origin.com", main_frame);
  RenderFrameHost* sub_frame_b =
      CreateAndNavigateSubFrame("https://y-origin.com", main_frame);
  RenderFrameHost* sub_frame_c =
      CreateAndNavigateSubFrame("https://x-origin.com", main_frame);

  // Create a same-origin frame.
  RenderFrameHost* sub_frame_d =
      CreateAndNavigateSubFrame("https://top.com/foo", main_frame);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.paint_timing->first_contentful_paint = base::Seconds(1);
  tester()->SimulateTimingUpdate(timing, sub_frame_a);

  timing.paint_timing->first_contentful_paint = base::Seconds(2);
  tester()->SimulateTimingUpdate(timing, sub_frame_b);

  timing.paint_timing->first_contentful_paint = base::Seconds(3);
  tester()->SimulateTimingUpdate(timing, sub_frame_c);

  timing.paint_timing->first_contentful_paint = base::Seconds(4);
  tester()->SimulateTimingUpdate(timing, sub_frame_d);

  tester()->histogram_tester().ExpectTotalCount(kSubframeFCPHistogram, 3);
  tester()->histogram_tester().ExpectTimeBucketCount(kSubframeFCPHistogram,
                                                     base::Seconds(1), 1);
  tester()->histogram_tester().ExpectTimeBucketCount(kSubframeFCPHistogram,
                                                     base::Seconds(2), 1);
  tester()->histogram_tester().ExpectTimeBucketCount(kSubframeFCPHistogram,
                                                     base::Seconds(3), 1);
}

TEST_P(ThirdPartyMetricsObserverTest, NoCookiesRead_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 0, 1);
}

TEST_P(ThirdPartyMetricsObserverTest, BlockedCookiesRead_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // If there are any blocked_by_policy reads, nothing should be recorded. Even
  // if there are subsequent non-blocked third-party reads.
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CookieWithAccessResult()},
                                  true /* blocked_by_policy */});
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CookieWithAccessResult()},
                                  false /* blocked_by_policy */});

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(kReadCookieHistogram, 0);
}

TEST_P(ThirdPartyMetricsObserverTest,
       NoRegistrableDomainNoHostCookiesRead_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  GURL url = GURL("data:,Hello%2C%20World!");
  ASSERT_FALSE(url.has_host());
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  url,
                                  GURL("https://top.com"),
                                  {net::CookieWithAccessResult()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 0, 1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       NoRegistrableDomainWithHostCookiesRead_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  GURL url = GURL("https://127.0.0.1/cookies");
  ASSERT_TRUE(url.has_host());
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  url,
                                  GURL("https://top.com"),
                                  {net::CookieWithAccessResult()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       DifferentSchemeSameRegistrableDomain_OneRecorded) {
  NavigateAndCommit(GURL("http://top.com"));

  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://top.com"),
                                  GURL("http://top.com"),
                                  {net::CookieWithAccessResult()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
}

TEST_P(ThirdPartyMetricsObserverTest, OnlyFirstPartyCookiesRead_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://top.com"),
                                  GURL("https://top.com"),
                                  {net::CookieWithAccessResult()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 0, 1);
}

TEST_P(ThirdPartyMetricsObserverTest, OneCookieRead_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CookieWithAccessResult()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       ThreeCookiesReadSameThirdParty_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CookieWithAccessResult()},
                                  false /* blocked_by_policy */});
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://a.com/foo"),
                                  GURL("https://top.com"),
                                  {net::CookieWithAccessResult()},
                                  false /* blocked_by_policy */});
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://sub.a.com/bar"),
                                  GURL("https://top.com"),
                                  {net::CookieWithAccessResult()},
                                  false /* blocked_by_policy */});

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       CookiesReadMultipleThirdParties_MultipleRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // Simulate third-party cookie reads from two different origins.
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CookieWithAccessResult()},
                                  false /* blocked_by_policy */});
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CookieWithAccessResult()},
                                  false /* blocked_by_policy */});
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://b.com"),
                                  GURL("https://top.com"),
                                  {net::CookieWithAccessResult()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 2, 1);
}

TEST_P(ThirdPartyMetricsObserverTest, OneCookieChanged_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kChange,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CookieWithAccessResult()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 0, 1);
}

TEST_P(ThirdPartyMetricsObserverTest, ReadAndChangeCookies_BothRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // Simulate third-party cookie reads from two different origins.
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CookieWithAccessResult()},
                                  false /* blocked_by_policy */});
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kChange,
                                  GURL("https://b.com"),
                                  GURL("https://top.com"),
                                  {net::CookieWithAccessResult()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       LargestContentfulPaint_HasThirdPartyFont) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta();
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      100u;

  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 120u;

  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL("https://foo.test"));
  tester()->SimulateTimingUpdate(timing);

  content::FrameTreeNodeId frame_tree_node_id =
      main_rfh()->GetFrameTreeNodeId();
  tester()->SimulateLoadedResource(
      {url::SchemeHostPort(GURL("https://bar.test")), net::IPEndPoint(),
       frame_tree_node_id, false /* was_cached */,
       1024 * 20 /* raw_body_bytes */, 0 /* original_network_content_length */,
       network::mojom::RequestDestination::kFont, 0,
       nullptr /* load_timing_info */},
      content::GlobalRequestID());

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL("https://foo.test"));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.Clients.ThirdParty.PaintTiming."
                  "NavigationToLargestContentfulPaint.HasThirdPartyFont"),
              testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_P(ThirdPartyMetricsObserverTest,
       NoLargestContentfulPaint_HasThirdPartyFont) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta();
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      100u;

  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 120u;

  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL("http://a.foo.test"));
  tester()->SimulateTimingUpdate(timing);

  // Load a same-site font, the histogram should not be recorded.
  content::FrameTreeNodeId frame_tree_node_id =
      main_rfh()->GetFrameTreeNodeId();
  tester()->SimulateLoadedResource(
      {url::SchemeHostPort(GURL("http://b.foo.test")), net::IPEndPoint(),
       frame_tree_node_id, false /* was_cached */,
       1024 * 20 /* raw_body_bytes */, 0 /* original_network_content_length */,
       network::mojom::RequestDestination::kFont, 0,
       nullptr /* load_timing_info */},
      content::GlobalRequestID());

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL("https://foo.test"));

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.ThirdParty.PaintTiming."
      "NavigationToLargestContentfulPaint.HasThirdPartyFont",
      0);
}

TEST_P(ThirdPartyMetricsObserverTest,
       NoTextLargestContentfulPaint_HasThirdPartyFont) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      120u;

  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL("https://foo.test"));
  tester()->SimulateTimingUpdate(timing);

  content::FrameTreeNodeId frame_tree_node_id =
      main_rfh()->GetFrameTreeNodeId();
  tester()->SimulateLoadedResource(
      {url::SchemeHostPort(GURL("https://bar.test")), net::IPEndPoint(),
       frame_tree_node_id, false /* was_cached */,
       1024 * 20 /* raw_body_bytes */, 0 /* original_network_content_length */,
       network::mojom::RequestDestination::kFont, 0,
       nullptr /* load_timing_info */},
      content::GlobalRequestID());

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL("https://foo.test"));

  // Since largest contentful paint is of type image, the histogram will not be
  // recorded.
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.ThirdParty.PaintTiming."
      "NavigationToLargestContentfulPaint.HasThirdPartyFont",
      0);
}

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "components/page_load_metrics/browser/metrics_lifecycle_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_content_browser_client.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/browser/test_metrics_web_contents_observer_embedder.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
#include "third_party/blink/public/common/performance/performance_timeline_constants.h"
#include "third_party/blink/public/common/use_counter/use_counter_feature.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "third_party/blink/public/mojom/use_counter/use_counter_feature.mojom-shared.h"
#include "url/url_constants.h"

using content::NavigationSimulator;

namespace page_load_metrics {

namespace {

const char kDefaultTestUrl[] = "https://google.com/";
const char kDefaultTestUrlAnchor[] = "https://google.com/#samedocument";
const char kDefaultTestUrl2[] = "https://whatever.com/";
const char kFilteredStartUrl[] = "https://whatever.com/ignore-on-start";
const char kFilteredCommitUrl[] = "https://whatever.com/ignore-on-commit";

void PopulatePageLoadTiming(mojom::PageLoadTiming* timing) {
  page_load_metrics::InitPageLoadTimingForTest(timing);
  timing->navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing->response_start = base::Milliseconds(10);
  timing->parse_timing->parse_start = base::Milliseconds(20);
}

blink::mojom::ResourceLoadInfoPtr CreateResourceLoadInfo(
    const GURL& url,
    network::mojom::RequestDestination request_destination) {
  blink::mojom::ResourceLoadInfoPtr resource_load_info =
      blink::mojom::ResourceLoadInfo::New();
  resource_load_info->final_url = url;
  resource_load_info->original_url = url;
  resource_load_info->request_destination = request_destination;
  resource_load_info->was_cached = false;
  resource_load_info->raw_body_bytes = 0;
  resource_load_info->net_error = net::OK;
  resource_load_info->network_info = blink::mojom::CommonNetworkInfo::New();
  resource_load_info->network_info->remote_endpoint = net::IPEndPoint();
  resource_load_info->load_timing_info.request_start = base::TimeTicks::Now();
  return resource_load_info;
}

}  //  namespace

class MetricsWebContentsObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  MetricsWebContentsObserverTest() {
    mojom::PageLoadTiming timing;
    PopulatePageLoadTiming(&timing);
    previous_timing_ = timing.Clone();
  }

  MetricsWebContentsObserverTest(const MetricsWebContentsObserverTest&) =
      delete;
  MetricsWebContentsObserverTest& operator=(
      const MetricsWebContentsObserverTest&) = delete;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    original_browser_client_ =
        content::SetBrowserClientForTesting(&browser_client_);
    AttachObserver();
  }

  void TearDown() override {
    embedder_interface_ = nullptr;
    content::SetBrowserClientForTesting(original_browser_client_);
    RenderViewHostTestHarness::TearDown();
  }

  void NavigateToUntrackedUrl() {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL(url::kAboutBlankURL));
  }

  // Returns the mock timer used for buffering updates in the
  // PageLoadMetricsUpdateDispatcher.
  base::MockOneShotTimer* GetMostRecentTimer() {
    return embedder_interface_->GetMockTimer();
  }

  void SimulateTimingUpdate(const mojom::PageLoadTiming& timing) {
    SimulateTimingUpdate(timing, web_contents()->GetPrimaryMainFrame());
  }

  void SimulateCpuTimingUpdate(const mojom::CpuTiming& timing,
                               content::RenderFrameHost* render_frame_host) {
    observer()->OnTimingUpdated(
        render_frame_host, previous_timing_->Clone(),
        mojom::FrameMetadataPtr(std::in_place),
        std::vector<blink::UseCounterFeature>(),
        std::vector<mojom::ResourceDataUpdatePtr>(),
        mojom::FrameRenderDataUpdatePtr(std::in_place), timing.Clone(),
        mojom::InputTimingPtr(std::in_place), std::nullopt,
        mojom::SoftNavigationMetrics::New(
            blink::kSoftNavigationCountDefaultValue, base::Milliseconds(0),
            std::string(), mojom::LargestContentfulPaintTiming::New()));
  }

  void SimulateTimingUpdate(const mojom::PageLoadTiming& timing,
                            content::RenderFrameHost* render_frame_host) {
    SimulateTimingUpdateWithoutFiringDispatchTimer(timing, render_frame_host);
    // If sending the timing update caused the PageLoadMetricsUpdateDispatcher
    // to schedule a buffering timer, then fire it now so metrics are dispatched
    // to observers.
    base::MockOneShotTimer* mock_timer = GetMostRecentTimer();
    if (mock_timer && mock_timer->IsRunning())
      mock_timer->Fire();
  }

  void SimulateTimingUpdateWithoutFiringDispatchTimer(
      const mojom::PageLoadTiming& timing,
      content::RenderFrameHost* render_frame_host) {
    previous_timing_ = timing.Clone();
    observer()->OnTimingUpdated(
        render_frame_host, timing.Clone(),
        mojom::FrameMetadataPtr(std::in_place),
        std::vector<blink::UseCounterFeature>(),
        std::vector<mojom::ResourceDataUpdatePtr>(),
        mojom::FrameRenderDataUpdatePtr(std::in_place),
        mojom::CpuTimingPtr(std::in_place),
        mojom::InputTimingPtr(std::in_place), std::nullopt,
        mojom::SoftNavigationMetrics::New(
            blink::kSoftNavigationCountDefaultValue, base::Milliseconds(0),
            std::string(), mojom::LargestContentfulPaintTiming::New()));
  }

  void SimulateCustomUserTimingUpdate(
      const mojom::CustomUserTimingMark& custom_timing,
      content::RenderFrameHost* render_frame_host) {
    observer()->OnCustomUserTimingUpdated(render_frame_host,
                                          custom_timing.Clone());
  }

  virtual std::unique_ptr<TestMetricsWebContentsObserverEmbedder>
  CreateEmbedder() {
    return std::make_unique<TestMetricsWebContentsObserverEmbedder>();
  }

  void AttachObserver() {
    auto embedder_interface = CreateEmbedder();
    embedder_interface_ = embedder_interface.get();
    MetricsWebContentsObserver* observer =
        MetricsWebContentsObserver::CreateForWebContents(
            web_contents(), std::move(embedder_interface));
    observer->OnVisibilityChanged(content::Visibility::VISIBLE);
  }

  void CheckErrorEvent(InternalErrorLoadEvent error, int count) {
    histogram_tester_.ExpectBucketCount(internal::kErrorEvents, error, count);
    num_errors_ += count;
  }

  void CheckTotalErrorEvents() {
    histogram_tester_.ExpectTotalCount(internal::kErrorEvents, num_errors_);
  }

  void CheckNoErrorEvents() {
    histogram_tester_.ExpectTotalCount(internal::kErrorEvents, 0);
  }

  int CountEmptyCompleteTimingReported() {
    int empty = 0;
    for (const auto& timing : embedder_interface_->complete_timings()) {
      if (page_load_metrics::IsEmpty(*timing))
        ++empty;
    }
    return empty;
  }

  void CheckErrorNoIPCsReceivedIfNeeded(int count) {
    // With BackForwardCache, page is kept alive after navigation.
    // ERR_NO_IPCS_RECEIVED isn't recorded as it is reported during destruction
    // of page after navigation which doesn't happen with BackForwardCache.
    if (!content::BackForwardCache::IsBackForwardCacheFeatureEnabled())
      CheckErrorEvent(ERR_NO_IPCS_RECEIVED, count);
  }

  const std::vector<mojom::PageLoadTimingPtr>& updated_timings() const {
    return embedder_interface_->updated_timings();
  }
  const std::vector<mojom::CpuTimingPtr>& updated_cpu_timings() const {
    return embedder_interface_->updated_cpu_timings();
  }
  const std::vector<mojom::PageLoadTimingPtr>& complete_timings() const {
    return embedder_interface_->complete_timings();
  }
  const std::vector<mojom::PageLoadTimingPtr>& updated_subframe_timings()
      const {
    return embedder_interface_->updated_subframe_timings();
  }
  const std::vector<mojom::CustomUserTimingMarkPtr>&
  updated_custom_user_timings() const {
    return embedder_interface_->updated_custom_user_timings();
  }
  int CountCompleteTimingReported() { return complete_timings().size(); }
  int CountUpdatedTimingReported() { return updated_timings().size(); }
  int CountUpdatedCpuTimingReported() { return updated_cpu_timings().size(); }
  int CountUpdatedSubFrameTimingReported() {
    return updated_subframe_timings().size();
  }
  int CountOnBackForwardCacheEntered() const {
    return embedder_interface_->count_on_enter_back_forward_cache();
  }
  int CountUpdatedCustomUserTimingReported() {
    return embedder_interface_->updated_custom_user_timings().size();
  }

  const std::vector<GURL>& observed_committed_urls_from_on_start() const {
    return embedder_interface_->observed_committed_urls_from_on_start();
  }

  const std::vector<GURL>& observed_aborted_urls() const {
    return embedder_interface_->observed_aborted_urls();
  }

  const std::vector<blink::UseCounterFeature>& observed_features() const {
    return embedder_interface_->observed_features();
  }

  const std::optional<bool>& is_first_navigation_in_web_contents() const {
    return embedder_interface_->is_first_navigation_in_web_contents();
  }

  const std::vector<GURL>& completed_filtered_urls() const {
    return embedder_interface_->completed_filtered_urls();
  }

  const std::vector<ExtraRequestCompleteInfo>& loaded_resources() const {
    return embedder_interface_->loaded_resources();
  }

 protected:
  MetricsWebContentsObserver* observer() {
    return MetricsWebContentsObserver::FromWebContents(web_contents());
  }

  base::HistogramTester histogram_tester_;
  raw_ptr<TestMetricsWebContentsObserverEmbedder, DanglingUntriaged>
      embedder_interface_;

 private:
  int num_errors_ = 0;
  // Since we have two types of updates, both CpuTiming and PageLoadTiming, and
  // these feed into a singular OnTimingUpdated, we need to pass in an unchanged
  // PageLoadTiming structure to this function, so we need to keep track of the
  // previous structure that was passed when updating the PageLoadTiming.
  mojom::PageLoadTimingPtr previous_timing_;
  PageLoadMetricsTestContentBrowserClient browser_client_;
  raw_ptr<content::ContentBrowserClient> original_browser_client_ = nullptr;
};

TEST_F(MetricsWebContentsObserverTest, SuccessfulMainFrameNavigation) {
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);

  ASSERT_TRUE(observed_committed_urls_from_on_start().empty());
  ASSERT_FALSE(is_first_navigation_in_web_contents().has_value());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  ASSERT_EQ(1u, observed_committed_urls_from_on_start().size());
  ASSERT_TRUE(observed_committed_urls_from_on_start().at(0).is_empty());
  ASSERT_TRUE(is_first_navigation_in_web_contents().has_value());
  ASSERT_TRUE(is_first_navigation_in_web_contents().value());

  ASSERT_EQ(0, CountUpdatedTimingReported());
  SimulateTimingUpdate(timing);
  ASSERT_EQ(1, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2));
  ASSERT_FALSE(is_first_navigation_in_web_contents().value());
  ASSERT_EQ(1, CountCompleteTimingReported());
  ASSERT_EQ(0, CountEmptyCompleteTimingReported());
  ASSERT_EQ(2u, observed_committed_urls_from_on_start().size());
  ASSERT_EQ(kDefaultTestUrl,
            observed_committed_urls_from_on_start().at(1).spec());
  ASSERT_EQ(1, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountUpdatedSubFrameTimingReported());

  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest,
       DISABLED_MainFrameNavigationInternalAbort) {
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kDefaultTestUrl), web_contents());
  navigation->Fail(net::ERR_ABORTED);
  ASSERT_EQ(1u, observed_aborted_urls().size());
  ASSERT_EQ(kDefaultTestUrl, observed_aborted_urls().front().spec());
}

TEST_F(MetricsWebContentsObserverTest, SubFrame) {
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.response_start = base::Milliseconds(10);
  timing.parse_timing->parse_start = base::Milliseconds(20);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  ASSERT_EQ(1, CountUpdatedTimingReported());
  EXPECT_TRUE(timing.Equals(*updated_timings().back()));

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  // Dispatch a timing update for the child frame that includes a first paint.
  mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  subframe_timing.response_start = base::Milliseconds(10);
  subframe_timing.parse_timing->parse_start = base::Milliseconds(20);
  subframe_timing.paint_timing->first_paint = base::Milliseconds(40);
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kDefaultTestUrl2), subframe);
  SimulateTimingUpdate(subframe_timing, subframe);

  ASSERT_EQ(1, CountUpdatedSubFrameTimingReported());
  EXPECT_TRUE(subframe_timing.Equals(*updated_subframe_timings().back()));

  // The subframe update which included a paint should have also triggered
  // a main frame update, which includes a first paint.
  ASSERT_EQ(2, CountUpdatedTimingReported());
  EXPECT_FALSE(timing.Equals(*updated_timings().back()));
  EXPECT_TRUE(updated_timings().back()->paint_timing->first_paint);

  // Navigate again to see if the timing updated for a subframe message.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2));

  ASSERT_EQ(1, CountCompleteTimingReported());
  ASSERT_EQ(2, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountEmptyCompleteTimingReported());

  ASSERT_EQ(1, CountUpdatedSubFrameTimingReported());
  EXPECT_TRUE(subframe_timing.Equals(*updated_subframe_timings().back()));

  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, SameDocumentNoTrigger) {
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  ASSERT_EQ(0, CountUpdatedTimingReported());
  SimulateTimingUpdate(timing);
  ASSERT_EQ(1, CountUpdatedTimingReported());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrlAnchor));
  // Send the same timing update. The original tracker for kDefaultTestUrl
  // should dedup the update, and the tracker for kDefaultTestUrlAnchor should
  // have been destroyed as a result of its being a same page navigation, so
  // CountUpdatedTimingReported() should continue to return 1.
  SimulateTimingUpdate(timing);

  ASSERT_EQ(1, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());

  // Navigate again to force histogram logging.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2));

  // A same page navigation shouldn't trigger logging UMA for the original.
  ASSERT_EQ(1, CountUpdatedTimingReported());
  ASSERT_EQ(1, CountCompleteTimingReported());
  ASSERT_EQ(0, CountEmptyCompleteTimingReported());
  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, DontLogIrrelevantNavigation) {
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(10);

  GURL about_blank_url = GURL("about:blank");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             about_blank_url);
  SimulateTimingUpdate(timing);
  ASSERT_EQ(0, CountUpdatedTimingReported());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());

  // Ensure that NTP and other untracked loads are still accounted for as part
  // of keeping track of the first navigation in the WebContents.
  ASSERT_TRUE(is_first_navigation_in_web_contents().has_value());
  ASSERT_FALSE(is_first_navigation_in_web_contents().value());

  CheckErrorEvent(ERR_IPC_FROM_BAD_URL_SCHEME, 0);
  CheckErrorEvent(ERR_IPC_WITH_NO_RELEVANT_LOAD, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, EmptyTimingError) {
  // Page load timing errors are not being reported when the error occurs for a
  // page that gets preserved in the back/forward cache.
  // TODO(crbug.com/40213776): Fix this.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  ASSERT_EQ(0, CountUpdatedTimingReported());
  NavigateToUntrackedUrl();
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(1, CountCompleteTimingReported());

  CheckErrorEvent(ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);
  CheckErrorEvent(ERR_NO_IPCS_RECEIVED, 1);
  CheckTotalErrorEvents();

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::INVALID_EMPTY_TIMING, 1);
}

TEST_F(MetricsWebContentsObserverTest, NullNavigationStartError) {
  // Page load timing errors are not being reported when the error occurs for a
  // page that gets preserved in the back/forward cache.
  // TODO(crbug.com/40213776): Fix this.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.parse_timing->parse_start = base::Milliseconds(1);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  ASSERT_EQ(0, CountUpdatedTimingReported());
  NavigateToUntrackedUrl();
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(1, CountCompleteTimingReported());

  CheckErrorEvent(ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);
  CheckErrorEvent(ERR_NO_IPCS_RECEIVED, 1);
  CheckTotalErrorEvents();

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::INVALID_NULL_NAVIGATION_START, 1);
}

TEST_F(MetricsWebContentsObserverTest, TimingOrderError) {
  // Page load timing errors are not being reported when the error occurs for a
  // page that gets preserved in the back/forward cache.
  // TODO(crbug.com/40213776): Fix this.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_stop = base::Milliseconds(1);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  ASSERT_EQ(0, CountUpdatedTimingReported());
  NavigateToUntrackedUrl();
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(1, CountCompleteTimingReported());

  CheckErrorEvent(ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);
  CheckErrorEvent(ERR_NO_IPCS_RECEIVED, 1);
  CheckTotalErrorEvents();

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::INVALID_ORDER_PARSE_START_PARSE_STOP, 1);
}

TEST_F(MetricsWebContentsObserverTest, BadIPC) {
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(10);
  mojom::PageLoadTiming timing2;
  page_load_metrics::InitPageLoadTimingForTest(&timing2);
  timing2.navigation_start = base::Time::FromSecondsSinceUnixEpoch(100);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));

  SimulateTimingUpdate(timing);
  ASSERT_EQ(1, CountUpdatedTimingReported());
  SimulateTimingUpdate(timing2);
  ASSERT_EQ(1, CountUpdatedTimingReported());

  CheckErrorEvent(ERR_BAD_TIMING_IPC_INVALID_TIMING_DESCENDENT, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, ObservePartialNavigation) {
  // Reset the state of the tests, and attach the MetricsWebContentsObserver in
  // the middle of a navigation. This tests that the class is robust to only
  // observing some of a navigation.
  DeleteContents();
  SetContents(CreateTestWebContents());

  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(10);

  // Start the navigation, then start observing the web contents. This used to
  // crash us. Make sure we bail out and don't log histograms.
  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateBrowserInitiated(GURL(kDefaultTestUrl),
                                                  web_contents());
  navigation->Start();
  AttachObserver();
  navigation->Commit();

  SimulateTimingUpdate(timing);

  // Navigate again to force histogram logging.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2));
  ASSERT_EQ(0, CountCompleteTimingReported());
  ASSERT_EQ(0, CountUpdatedTimingReported());
  CheckErrorEvent(ERR_IPC_WITH_NO_RELEVANT_LOAD, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, StopObservingOnCommit) {
  ASSERT_TRUE(completed_filtered_urls().empty());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  ASSERT_TRUE(completed_filtered_urls().empty());

  // kFilteredCommitUrl should stop observing in OnCommit, and thus should not
  // reach OnComplete().
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kFilteredCommitUrl));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl)}),
            completed_filtered_urls());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl)}),
            completed_filtered_urls());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl), GURL(kDefaultTestUrl2)}),
            completed_filtered_urls());
}

TEST_F(MetricsWebContentsObserverTest, StopObservingOnStart) {
  ASSERT_TRUE(completed_filtered_urls().empty());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  ASSERT_TRUE(completed_filtered_urls().empty());

  // kFilteredCommitUrl should stop observing in OnStart, and thus should not
  // reach OnComplete().
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kFilteredStartUrl));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl)}),
            completed_filtered_urls());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl)}),
            completed_filtered_urls());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl), GURL(kDefaultTestUrl2)}),
            completed_filtered_urls());
}

// We buffer cross frame timings in order to provide a consistent view of
// timing data to observers. See crbug.com/722860 for more.
TEST_F(MetricsWebContentsObserverTest, OutOfOrderCrossFrameTiming) {
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.response_start = base::Milliseconds(10);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  ASSERT_EQ(1, CountUpdatedTimingReported());
  EXPECT_TRUE(timing.Equals(*updated_timings().back()));

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  // Dispatch a timing update for the child frame that includes a first paint.
  mojom::PageLoadTiming subframe_timing;
  PopulatePageLoadTiming(&subframe_timing);
  subframe_timing.paint_timing->first_paint = base::Milliseconds(40);
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kDefaultTestUrl2), subframe);
  SimulateTimingUpdate(subframe_timing, subframe);

  // Though a first paint was dispatched in the child, it should not yet be
  // reflected as an updated timing in the main frame, since the main frame
  // hasn't received updates for required earlier events such as parse_start.
  ASSERT_EQ(1, CountUpdatedSubFrameTimingReported());
  EXPECT_TRUE(subframe_timing.Equals(*updated_subframe_timings().back()));
  ASSERT_EQ(1, CountUpdatedTimingReported());
  EXPECT_TRUE(timing.Equals(*updated_timings().back()));

  // Dispatch the parse_start event in the parent. We should now unbuffer the
  // first paint main frame update and receive a main frame update with a first
  // paint value.
  timing.parse_timing->parse_start = base::Milliseconds(20);
  SimulateTimingUpdate(timing);
  ASSERT_EQ(2, CountUpdatedTimingReported());
  EXPECT_FALSE(timing.Equals(*updated_timings().back()));
  EXPECT_TRUE(
      updated_timings().back()->parse_timing->Equals(*timing.parse_timing));
  EXPECT_TRUE(updated_timings().back()->document_timing->Equals(
      *timing.document_timing));
  EXPECT_FALSE(
      updated_timings().back()->paint_timing->Equals(*timing.paint_timing));
  EXPECT_TRUE(updated_timings().back()->paint_timing->first_paint);

  // Navigate again to see if the timing updated for a subframe message.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2));

  ASSERT_EQ(1, CountCompleteTimingReported());
  ASSERT_EQ(2, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountEmptyCompleteTimingReported());

  ASSERT_EQ(1, CountUpdatedSubFrameTimingReported());
  EXPECT_TRUE(subframe_timing.Equals(*updated_subframe_timings().back()));

  CheckNoErrorEvents();
}

// We buffer cross-frame paint updates to account for paint timings from
// different frames arriving out of order.
TEST_F(MetricsWebContentsObserverTest, OutOfOrderCrossFrameTiming2) {
  // Dispatch a timing update for the main frame that includes a first
  // paint. This should be buffered, with the dispatch timer running.
  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  // Ensure this is much bigger than the subframe first paint below. We
  // currently can't inject the navigation start offset, so we must ensure that
  // subframe first paint + navigation start offset < main frame first paint.
  timing.paint_timing->first_paint = base::Milliseconds(100000);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  SimulateTimingUpdateWithoutFiringDispatchTimer(timing, main_rfh());

  EXPECT_TRUE(GetMostRecentTimer()->IsRunning());
  ASSERT_EQ(0, CountUpdatedTimingReported());

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());

  // Dispatch a timing update for a child frame that includes a first paint.
  mojom::PageLoadTiming subframe_timing;
  PopulatePageLoadTiming(&subframe_timing);
  subframe_timing.paint_timing->first_paint = base::Milliseconds(500);
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kDefaultTestUrl2), subframe);
  SimulateTimingUpdateWithoutFiringDispatchTimer(subframe_timing, subframe);

  EXPECT_TRUE(GetMostRecentTimer()->IsRunning());
  ASSERT_EQ(0, CountUpdatedTimingReported());

  // At this point, the timing update is buffered, waiting for the timer to
  // fire.
  GetMostRecentTimer()->Fire();

  // Firing the timer should produce a timing update. The update should be a
  // merged view of the main frame timing, with a first paint timestamp from the
  // subframe.
  ASSERT_EQ(1, CountUpdatedTimingReported());
  EXPECT_FALSE(timing.Equals(*updated_timings().back()));
  EXPECT_TRUE(
      updated_timings().back()->parse_timing->Equals(*timing.parse_timing));
  EXPECT_TRUE(updated_timings().back()->document_timing->Equals(
      *timing.document_timing));
  EXPECT_FALSE(
      updated_timings().back()->paint_timing->Equals(*timing.paint_timing));
  EXPECT_TRUE(updated_timings().back()->paint_timing->first_paint);

  // The first paint value should be the min of all received first paints, which
  // in this case is the first paint from the subframe. Since it is offset by
  // the subframe's navigation start, the received value should be >= the first
  // paint value specified in the subframe.
  EXPECT_GE(updated_timings().back()->paint_timing->first_paint,
            subframe_timing.paint_timing->first_paint);
  EXPECT_LT(updated_timings().back()->paint_timing->first_paint,
            timing.paint_timing->first_paint);

  base::TimeDelta initial_first_paint =
      updated_timings().back()->paint_timing->first_paint.value();

  // Dispatch a timing update for an additional child frame, with an earlier
  // first paint time. This should cause an immediate update, without a timer
  // delay.
  subframe_timing.paint_timing->first_paint = base::Milliseconds(50);
  content::RenderFrameHost* subframe2 = rfh_tester->AppendChild("subframe");
  subframe2 = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kDefaultTestUrl2), subframe2);
  SimulateTimingUpdateWithoutFiringDispatchTimer(subframe_timing, subframe2);

  base::TimeDelta updated_first_paint =
      updated_timings().back()->paint_timing->first_paint.value();

  EXPECT_FALSE(GetMostRecentTimer()->IsRunning());
  ASSERT_EQ(2, CountUpdatedTimingReported());
  EXPECT_LT(updated_first_paint, initial_first_paint);

  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, FlushBufferOnAppBackground) {
  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  timing.paint_timing->first_paint = base::Milliseconds(100000);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  SimulateTimingUpdateWithoutFiringDispatchTimer(timing, main_rfh());

  ASSERT_EQ(0, CountUpdatedTimingReported());
  observer()->FlushMetricsOnAppEnterBackground();
  ASSERT_EQ(1, CountUpdatedTimingReported());
}

TEST_F(MetricsWebContentsObserverTest,
       FirstInputDelayMissingFirstInputTimestamp) {
  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  timing.interactive_timing->first_input_delay = base::Milliseconds(10);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2));

  const mojom::InteractiveTiming& interactive_timing =
      *complete_timings().back()->interactive_timing;

  // Won't have been set, as we're missing the first_input_timestamp.
  EXPECT_FALSE(interactive_timing.first_input_delay.has_value());

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::INVALID_NULL_FIRST_INPUT_TIMESTAMP, 1);

  CheckErrorEvent(ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);
  CheckErrorNoIPCsReceivedIfNeeded(1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest,
       FirstInputTimestampMissingFirstInputDelay) {
  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  timing.interactive_timing->first_input_timestamp = base::Milliseconds(10);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2));

  const mojom::InteractiveTiming& interactive_timing =
      *complete_timings().back()->interactive_timing;

  // Won't have been set, as we're missing the first_input_delay.
  EXPECT_FALSE(interactive_timing.first_input_timestamp.has_value());

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::INVALID_NULL_FIRST_INPUT_DELAY, 1);

  CheckErrorEvent(ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);
  CheckErrorNoIPCsReceivedIfNeeded(1);
  CheckTotalErrorEvents();
}

// Main frame delivers an input notification. Subsequently, a subframe delivers
// an input notification, where the input occurred first. Verify that
// FirstInputDelay and FirstInputTimestamp come from the subframe.
TEST_F(MetricsWebContentsObserverTest,
       FirstInputDelayAndTimingSubframeFirstDeliveredSecond) {
  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  timing.interactive_timing->first_input_delay = base::Milliseconds(10);
  // Set this far in the future. We currently can't control the navigation start
  // offset, so we ensure that the subframe timestamp + the unknown offset is
  // less than the main frame timestamp.
  timing.interactive_timing->first_input_timestamp = base::Minutes(100);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  // Dispatch a timing update for the child frame that includes a first input
  // earlier than the one for the main frame.
  mojom::PageLoadTiming subframe_timing;
  PopulatePageLoadTiming(&subframe_timing);
  subframe_timing.interactive_timing->first_input_delay =
      base::Milliseconds(15);
  subframe_timing.interactive_timing->first_input_timestamp =
      base::Milliseconds(90);

  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kDefaultTestUrl2), subframe);
  SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to confirm the timing updated for a subframe message.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2));

  const mojom::InteractiveTiming& interactive_timing =
      *complete_timings().back()->interactive_timing;

  EXPECT_EQ(base::Milliseconds(15), interactive_timing.first_input_delay);
  // Ensure the timestamp is from the subframe. The main frame timestamp was 100
  // minutes.
  EXPECT_LT(interactive_timing.first_input_timestamp, base::Minutes(10));

  CheckNoErrorEvents();
}

// A subframe delivers an input notification. Subsequently, the mainframe
// delivers an input notification, where the input occurred first. Verify that
// FirstInputDelay and FirstInputTimestamp come from the main frame.
TEST_F(MetricsWebContentsObserverTest,
       FirstInputDelayAndTimingMainframeFirstDeliveredSecond) {
  // We need to navigate before we can navigate the subframe.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  mojom::PageLoadTiming subframe_timing;
  PopulatePageLoadTiming(&subframe_timing);
  subframe_timing.interactive_timing->first_input_delay =
      base::Milliseconds(10);
  subframe_timing.interactive_timing->first_input_timestamp =
      base::Minutes(100);

  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kDefaultTestUrl2), subframe);
  SimulateTimingUpdate(subframe_timing, subframe);

  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  // Dispatch a timing update for the main frame that includes a first input
  // earlier than the one for the subframe.

  timing.interactive_timing->first_input_delay = base::Milliseconds(15);
  // Set this far in the future. We currently can't control the navigation start
  // offset, so we ensure that the main frame timestamp + the unknown offset is
  // less than the subframe timestamp.
  timing.interactive_timing->first_input_timestamp = base::Milliseconds(90);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Navigate again to confirm the timing updated for the mainframe message.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2));

  const mojom::InteractiveTiming& interactive_timing =
      *complete_timings().back()->interactive_timing;

  EXPECT_EQ(base::Milliseconds(15), interactive_timing.first_input_delay);
  // Ensure the timestamp is from the main frame. The subframe timestamp was 100
  // minutes.
  EXPECT_LT(interactive_timing.first_input_timestamp, base::Minutes(10));

  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, DispatchDelayedMetricsOnPageClose) {
  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  timing.paint_timing->first_paint = base::Milliseconds(1000);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  SimulateTimingUpdateWithoutFiringDispatchTimer(timing, main_rfh());

  // Throw in a cpu timing update, shouldn't affect the page timing results.
  mojom::CpuTiming cpu_timing;
  cpu_timing.task_time = base::Milliseconds(1000);
  SimulateCpuTimingUpdate(cpu_timing, main_rfh());

  EXPECT_TRUE(GetMostRecentTimer()->IsRunning());
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());

  // Navigate to a new page. This should force dispatch of the buffered timing
  // update.
  NavigateToUntrackedUrl();

  ASSERT_EQ(1, CountUpdatedTimingReported());
  ASSERT_EQ(1, CountUpdatedCpuTimingReported());
  ASSERT_EQ(1, CountCompleteTimingReported());
  EXPECT_TRUE(timing.Equals(*updated_timings().back()));
  EXPECT_TRUE(timing.Equals(*complete_timings().back()));
  EXPECT_TRUE(cpu_timing.Equals(*updated_cpu_timings().back()));

  CheckNoErrorEvents();
}

// Make sure the dispatch of CPU occurs immediately.
TEST_F(MetricsWebContentsObserverTest, DispatchCpuMetricsImmediately) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));

  mojom::CpuTiming timing;
  timing.task_time = base::Milliseconds(1000);
  SimulateCpuTimingUpdate(timing, main_rfh());
  ASSERT_EQ(1, CountUpdatedCpuTimingReported());
  EXPECT_TRUE(timing.Equals(*updated_cpu_timings().back()));

  // Navigate to a new page. This should force dispatch of the buffered timing
  // update.
  NavigateToUntrackedUrl();

  ASSERT_EQ(1, CountUpdatedCpuTimingReported());
  EXPECT_TRUE(timing.Equals(*updated_cpu_timings().back()));

  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, OnLoadedResource_MainFrame) {
  GURL main_resource_url(kDefaultTestUrl);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             main_resource_url);

  auto navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          main_resource_url, web_contents()->GetPrimaryMainFrame());
  navigation_simulator->Start();
  navigation_simulator->Commit();

  const auto request_id = navigation_simulator->GetGlobalRequestID();

  observer()->ResourceLoadComplete(
      web_contents()->GetPrimaryMainFrame(), request_id,
      *CreateResourceLoadInfo(main_resource_url,
                              network::mojom::RequestDestination::kFrame));
  EXPECT_EQ(1u, loaded_resources().size());
  EXPECT_EQ(url::SchemeHostPort(main_resource_url),
            loaded_resources().back().final_url);

  NavigateToUntrackedUrl();

  // Deliver a second main frame resource. This one should be ignored, since the
  // specified |request_id| is no longer associated with any tracked page loads.
  observer()->ResourceLoadComplete(
      web_contents()->GetPrimaryMainFrame(), request_id,
      *CreateResourceLoadInfo(main_resource_url,
                              network::mojom::RequestDestination::kFrame));
  EXPECT_EQ(1u, loaded_resources().size());
  EXPECT_EQ(url::SchemeHostPort(main_resource_url),
            loaded_resources().back().final_url);
}

TEST_F(MetricsWebContentsObserverTest, OnLoadedResource_Subresource) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  GURL loaded_resource_url("http://www.other.com/");
  observer()->ResourceLoadComplete(
      web_contents()->GetPrimaryMainFrame(), content::GlobalRequestID(),
      *CreateResourceLoadInfo(loaded_resource_url,
                              network::mojom::RequestDestination::kScript));

  EXPECT_EQ(1u, loaded_resources().size());
  EXPECT_EQ(url::SchemeHostPort(loaded_resource_url),
            loaded_resources().back().final_url);
}

TEST_F(MetricsWebContentsObserverTest,
       OnLoadedResource_ResourceFromOtherRFHIgnored) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));

  content::RenderFrameHost* old_rfh = web_contents()->GetPrimaryMainFrame();
  content::LeaveInPendingDeletionState(old_rfh);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2));

  DCHECK(!old_rfh->IsActive());
  observer()->ResourceLoadComplete(
      old_rfh, content::GlobalRequestID(),
      *CreateResourceLoadInfo(GURL("http://www.other.com/"),
                              network::mojom::RequestDestination::kScript));

  EXPECT_TRUE(loaded_resources().empty());
}

TEST_F(MetricsWebContentsObserverTest, RecordFeatureUsage) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  ASSERT_EQ(main_rfh()->GetLastCommittedURL().spec(), GURL(kDefaultTestUrl));

  MetricsWebContentsObserver::RecordFeatureUsage(
      main_rfh(), {blink::mojom::WebFeature::kHTMLMarqueeElement,
                   blink::mojom::WebFeature::kFormAttribute});

  blink::UseCounterFeature feature1 = {
      blink::mojom::UseCounterFeatureType::kWebFeature,
      static_cast<uint32_t>(blink::mojom::WebFeature::kHTMLMarqueeElement),
  };

  blink::UseCounterFeature feature2 = {
      blink::mojom::UseCounterFeatureType::kWebFeature,
      static_cast<uint32_t>(blink::mojom::WebFeature::kFormAttribute),
  };

  ASSERT_EQ(observed_features().size(), 2ul);
  EXPECT_EQ(observed_features()[0], feature1);
  EXPECT_EQ(observed_features()[1], feature2);
}

TEST_F(MetricsWebContentsObserverTest, RecordFeatureUsageNoObserver) {
  // Reset the state of the tests, and don't add an observer.
  DeleteContents();
  SetContents(CreateTestWebContents());

  // This call should just do nothing, and should not crash - if that happens,
  // we are good.
  MetricsWebContentsObserver::RecordFeatureUsage(
      main_rfh(), {blink::mojom::WebFeature::kHTMLMarqueeElement,
                   blink::mojom::WebFeature::kFormAttribute});
}

TEST_F(MetricsWebContentsObserverTest, CustomUserTiming) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();

  mojom::CustomUserTimingMark custom_timing;
  custom_timing.mark_name = "fake_custom_mark";
  custom_timing.start_time = base::Milliseconds(1000);

  SimulateCustomUserTimingUpdate(custom_timing, rfh);
  ASSERT_EQ(1, CountUpdatedCustomUserTimingReported());
  EXPECT_TRUE(custom_timing.Equals(*updated_custom_user_timings().back()));
  CheckNoErrorEvents();
}

class MetricsWebContentsObserverBackForwardCacheTest
    : public MetricsWebContentsObserverTest,
      public content::WebContentsDelegate {
  class CreatedPageLoadTrackerObserver : public MetricsLifecycleObserver {
   public:
    explicit CreatedPageLoadTrackerObserver(content::WebContents* web_contents)
        : MetricsLifecycleObserver(web_contents) {}

    int tracker_committed_count() const { return tracker_committed_count_; }

    void OnCommit(PageLoadTracker* tracker) override {
      tracker_committed_count_++;
    }

   private:
    int tracker_committed_count_ = 0;
  };

 public:
  MetricsWebContentsObserverBackForwardCacheTest() {
    feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

  ~MetricsWebContentsObserverBackForwardCacheTest() override = default;

  int tracker_committed_count() const {
    return created_page_load_tracker_observer_->tracker_committed_count();
  }

  void SetUp() override {
    MetricsWebContentsObserverTest::SetUp();
    created_page_load_tracker_observer_ =
        std::make_unique<CreatedPageLoadTrackerObserver>(web_contents());
    observer()->AddLifecycleObserver(created_page_load_tracker_observer_.get());
    web_contents()->SetDelegate(this);
  }

  // content::WebContentsDelegate:
  bool IsBackForwardCacheSupported(
      content::WebContents& web_contents) override {
    return true;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<CreatedPageLoadTrackerObserver>
      created_page_load_tracker_observer_;
};

TEST_F(MetricsWebContentsObserverBackForwardCacheTest,
       RecordFeatureUsageWithBackForwardCache) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  ASSERT_EQ(main_rfh()->GetLastCommittedURL().spec(), GURL(kDefaultTestUrl));

  MetricsWebContentsObserver::RecordFeatureUsage(
      main_rfh(), blink::mojom::WebFeature::kHTMLMarqueeElement);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2));
  content::NavigationSimulator::GoBack(web_contents());

  MetricsWebContentsObserver::RecordFeatureUsage(
      main_rfh(), blink::mojom::WebFeature::kFormAttribute);

  // For now back-forward cached navigations are not tracked and the events
  // after the history navigation are not tracked.
  blink::UseCounterFeature feature = {
      blink::mojom::UseCounterFeatureType::kWebFeature,
      static_cast<uint32_t>(blink::mojom::WebFeature::kHTMLMarqueeElement),
  };
  EXPECT_THAT(observed_features(), testing::ElementsAre(feature));
}

// Checks OnEnterBackForwardCache is called appropriately with back-forward
// cache enabled.
TEST_F(MetricsWebContentsObserverBackForwardCacheTest, EnterBackForwardCache) {
  // Go to the URL1.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  ASSERT_EQ(main_rfh()->GetLastCommittedURL().spec(), GURL(kDefaultTestUrl));

  ASSERT_EQ(0, CountCompleteTimingReported());
  EXPECT_EQ(0, CountOnBackForwardCacheEntered());
  EXPECT_EQ(1, tracker_committed_count());

  // Go to the URL2.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2));
  ASSERT_EQ(main_rfh()->GetLastCommittedURL().spec(), GURL(kDefaultTestUrl2));

  // With the default implementation of PageLoadMetricsObserver,
  // OnEnteringBackForwardCache invokes OnComplete and returns STOP_OBSERVING.
  ASSERT_EQ(1, CountCompleteTimingReported());
  EXPECT_EQ(1, CountOnBackForwardCacheEntered());
  EXPECT_EQ(2, tracker_committed_count());

  // Go back.
  content::NavigationSimulator::GoBack(web_contents());
  EXPECT_EQ(2, CountOnBackForwardCacheEntered());

  // Again, OnComplete is assured to be called.
  ASSERT_EQ(2, CountCompleteTimingReported());

  // A new page load tracker is not created or committed. A page load tracker in
  // the cache is used instead.
  EXPECT_EQ(2, tracker_committed_count());
}

// TODO(hajimehoshi): Detect the document eviction so that PageLoadTracker in
// the cache is destroyed. This would call PageLoadMetricsObserver::OnComplete.
// This test can be implemented after a PageLoadMetricsObserver's
// OnEnterBackForwardCache returns CONTINUE_OBSERVING.

class MetricsWebContentsObserverBackForwardCacheDisabledTest
    : public MetricsWebContentsObserverTest {
 public:
  MetricsWebContentsObserverBackForwardCacheDisabledTest() {
    feature_list_.InitWithFeaturesAndParameters({},
                                                {features::kBackForwardCache});
  }

  ~MetricsWebContentsObserverBackForwardCacheDisabledTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Checks OnEnterBackForwardCache is NOT called without back-forward cache
// enabled.
TEST_F(MetricsWebContentsObserverBackForwardCacheDisabledTest,
       EnterBackForwardCacheNotCalled) {
  // Go to the URL1.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  ASSERT_EQ(main_rfh()->GetLastCommittedURL().spec(), GURL(kDefaultTestUrl));

  ASSERT_EQ(0, CountCompleteTimingReported());

  // Go to the URL2.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2));
  ASSERT_EQ(main_rfh()->GetLastCommittedURL().spec(), GURL(kDefaultTestUrl2));

  ASSERT_EQ(1, CountCompleteTimingReported());
  EXPECT_EQ(0, CountOnBackForwardCacheEntered());

  // Go back.
  content::NavigationSimulator::GoBack(web_contents());
  EXPECT_EQ(0, CountOnBackForwardCacheEntered());

  ASSERT_EQ(2, CountCompleteTimingReported());
}

class MetricsWebContentsObserverNonPrimaryPageTest
    : public MetricsWebContentsObserverBackForwardCacheTest {
 public:
  class MetricsObserver : public PageLoadMetricsObserver {
   public:
    explicit MetricsObserver(
        MetricsWebContentsObserverNonPrimaryPageTest* owner)
        : owner_(owner) {}

    const char* GetObserverName() const override {
      static const char kName[] =
          "MetricsWebContentsObserverNonPrimaryPageTest::MetricsObserver";
      return kName;
    }

    ObservePolicy OnFencedFramesStart(
        content::NavigationHandle* navigation_handle,
        const GURL& currently_committed_url) override {
      // Takes the default option to test general cases.
      return FORWARD_OBSERVING;
    }

    ObservePolicy OnPrerenderStart(
        content::NavigationHandle* navigation_handle,
        const GURL& currently_committed_url) override {
      // This class's users don't need to support Prerendering yet.
      return STOP_OBSERVING;
    }

    ObservePolicy OnCommit(content::NavigationHandle* handle) override {
      committed_url_ = handle->GetURL();
      return CONTINUE_OBSERVING;
    }

    ObservePolicy OnEnterBackForwardCache(
        const mojom::PageLoadTiming& timing) override {
      return CONTINUE_OBSERVING;
    }

    void OnV8MemoryChanged(
        const std::vector<MemoryUpdate>& memory_updates) override {
      owner_->OnV8MemoryChanged(committed_url_, memory_updates);
    }

   private:
    raw_ptr<MetricsWebContentsObserverNonPrimaryPageTest> owner_;
    GURL committed_url_;
  };

  class Embedder : public TestMetricsWebContentsObserverEmbedder {
   public:
    explicit Embedder(MetricsWebContentsObserverNonPrimaryPageTest* owner)
        : owner_(owner) {}

    void RegisterObservers(
        PageLoadTracker* tracker,
        content::NavigationHandle* navigation_handle) override {
      TestMetricsWebContentsObserverEmbedder::RegisterObservers(
          tracker, navigation_handle);
      tracker->AddObserver(std::make_unique<MetricsObserver>(owner_));
    }

   private:
    raw_ptr<MetricsWebContentsObserverNonPrimaryPageTest> owner_;
  };

  std::unique_ptr<TestMetricsWebContentsObserverEmbedder> CreateEmbedder()
      override {
    return std::make_unique<Embedder>(this);
  }

  void OnV8MemoryChanged(const GURL& url,
                         const std::vector<MemoryUpdate>& memory_updates) {
    std::vector<MemoryUpdate>& updates_for_url = observed_memory_updates_[url];
    updates_for_url.insert(updates_for_url.end(), memory_updates.begin(),
                           memory_updates.end());
  }

 protected:
  std::map<GURL, std::vector<MemoryUpdate>> observed_memory_updates_;
};

TEST_F(MetricsWebContentsObserverNonPrimaryPageTest, MemoryUpdates) {
  // Go to the URL1.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));
  ASSERT_EQ(main_rfh()->GetLastCommittedURL().spec(), GURL(kDefaultTestUrl));
  content::GlobalRenderFrameHostId rfh1_id = main_rfh()->GetGlobalId();

  ASSERT_EQ(0, CountCompleteTimingReported());
  EXPECT_EQ(0, CountOnBackForwardCacheEntered());
  EXPECT_EQ(1, tracker_committed_count());

  // Go to the URL2.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2));
  ASSERT_EQ(main_rfh()->GetLastCommittedURL().spec(), GURL(kDefaultTestUrl2));
  content::GlobalRenderFrameHostId rfh2_id = main_rfh()->GetGlobalId();

  ASSERT_EQ(1, CountCompleteTimingReported());
  EXPECT_EQ(1, CountOnBackForwardCacheEntered());
  EXPECT_EQ(2, tracker_committed_count());

  std::vector<MemoryUpdate> memory_updates = {{rfh1_id, 100}, {rfh2_id, 200}};
  observer()->OnV8MemoryChanged(memory_updates);

  // Verify that memory updates are observed both in primary URL2 and
  // non-primary URL1.
  ASSERT_EQ(2u, observed_memory_updates_.size());
  ASSERT_EQ(1u, observed_memory_updates_[GURL(kDefaultTestUrl)].size());
  EXPECT_EQ(100,
            observed_memory_updates_[GURL(kDefaultTestUrl)][0].delta_bytes);
  ASSERT_EQ(1u, observed_memory_updates_[GURL(kDefaultTestUrl2)].size());
  EXPECT_EQ(200,
            observed_memory_updates_[GURL(kDefaultTestUrl2)][0].delta_bytes);
}

}  // namespace page_load_metrics

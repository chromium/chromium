// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/ad_metrics/ads_page_load_metrics_observer.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_data.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_delegate.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
#include "components/heavy_ad_intervention/heavy_ad_blocklist.h"
#include "components/heavy_ad_intervention/heavy_ad_features.h"
#include "components/page_load_metrics/browser/metrics_navigation_throttle.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/frame_tree_data.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_tester.h"
#include "components/page_load_metrics/browser/page_load_metrics_memory_tracker.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_metrics_util.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_test_harness.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/fake_local_frame.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "net/base/host_port_pair.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "url/gurl.h"

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;
using content::TestNavigationThrottle;

namespace page_load_metrics {

namespace {

struct ExpectedFrameBytes {
  ExpectedFrameBytes(size_t cached_kb, size_t uncached_kb)
      : cached_kb(cached_kb), uncached_kb(uncached_kb) {}
  size_t cached_kb;
  size_t uncached_kb;

  bool operator<(const ExpectedFrameBytes& other) const {
    return cached_kb < other.cached_kb ||
           (cached_kb == other.cached_kb && uncached_kb < other.uncached_kb);
  }
};

struct CreativeOriginTest {
  std::vector<std::string> urls;
  size_t creative_index;
  OriginStatus expected_origin_status;
};

struct CreativeOriginTestWithThrottling {
  std::string page_url;
  std::vector<std::string> subframe_urls;
  std::vector<bool> throttled;
  size_t creative_index;
  bool should_paint;
  OriginStatusWithThrottling expected_origin_status;
};

struct CompleteAuctionResult {
  bool is_server_auction;
  bool is_on_device_auction;
  content::AuctionResult result;
};

enum class ResourceCached { kNotCached = 0, kCachedHttp, kCachedMemory };
enum class FrameType { AD = 0, NON_AD };

const base::TimeDelta kParseStartTime = base::Milliseconds(3);
const base::TimeDelta kCreativeEligibleToPaintTime = base::Milliseconds(4);
const base::TimeDelta kCreativeFCPTime = base::Milliseconds(5);
const base::TimeDelta kOtherFrameEligibleToPaintTime = base::Milliseconds(9);
const base::TimeDelta kOtherFrameFCPTime = base::Milliseconds(10);
const char kAdUrl[] = "https://ads.com/ad/disallowed.html";
const char kOtherAdUrl[] = "https://other-ads.com/ad/disallowed.html";
const char kNonAdUrl[] = "https://foo.com/";
const char kNonAdUrlSameOrigin[] = "https://ads.com/foo";
const char kAllowedUrl[] = "https://foo.com/ad/not_disallowed.html";
const char kMemoryMainFrameMaxHistogramId[] =
    "PageLoad.Clients.Ads.Memory.MainFrame.Max";
const char kMemoryUpdateCountHistogramId[] =
    "PageLoad.Clients.Ads.Memory.UpdateCount";

const int kMaxHeavyAdNetworkBytes =
    heavy_ad_thresholds::kMaxNetworkBytes +
    AdsPageLoadMetricsObserver::HeavyAdThresholdNoiseProvider::
        kMaxNetworkThresholdNoiseBytes;

// Calls PopulateRequiredTimingFields with |first_eligible_to_paint| and
// |first_contentful_paint| fields temporarily nullified.
void PopulateRequiredTimingFieldsExceptFEtPAndFCP(
    mojom::PageLoadTiming* inout_timing) {
  // Save FEtP and FCP values in temp variables and then reset the fields.
  auto first_eligible_to_paint =
      inout_timing->paint_timing->first_eligible_to_paint;
  inout_timing->paint_timing->first_eligible_to_paint.reset();

  auto first_contentful_paint =
      inout_timing->paint_timing->first_contentful_paint;
  inout_timing->paint_timing->first_contentful_paint.reset();

  // Populate required fields that don't depend on FEtP or FCP.
  PopulateRequiredTimingFields(inout_timing);

  // Reinstate REtP and FCP values.
  inout_timing->paint_timing->first_eligible_to_paint = first_eligible_to_paint;
  inout_timing->paint_timing->first_contentful_paint = first_contentful_paint;

  // Populate |first_paint| field if needed.
  if ((inout_timing->paint_timing->first_image_paint ||
       inout_timing->paint_timing->first_contentful_paint) &&
      !inout_timing->paint_timing->first_paint) {
    inout_timing->paint_timing->first_paint =
        OptionalMin(inout_timing->paint_timing->first_image_paint,
                    inout_timing->paint_timing->first_contentful_paint);
  }
}

// Asynchronously cancels the navigation at WillProcessResponse. Before
// cancelling, simulates loading a main frame resource.
class ResourceLoadingCancellingThrottle
    : public content::TestNavigationThrottle {
 public:
  static std::unique_ptr<content::NavigationThrottle> Create(
      content::NavigationHandle* handle) {
    return std::make_unique<ResourceLoadingCancellingThrottle>(handle);
  }

  explicit ResourceLoadingCancellingThrottle(
      content::NavigationHandle* navigation_handle)
      : content::TestNavigationThrottle(navigation_handle) {
    SetResponse(TestNavigationThrottle::WILL_PROCESS_RESPONSE,
                TestNavigationThrottle::ASYNCHRONOUS, CANCEL);
  }

  ResourceLoadingCancellingThrottle(const ResourceLoadingCancellingThrottle&) =
      delete;
  ResourceLoadingCancellingThrottle& operator=(
      const ResourceLoadingCancellingThrottle&) = delete;

 private:
  // content::TestNavigationThrottle:
  void OnWillRespond(NavigationThrottle::ThrottleCheckResult result) {
    if (result.action() != CANCEL) {
      return;
    }

    auto* observer = MetricsWebContentsObserver::FromWebContents(
        navigation_handle()->GetWebContents());
    DCHECK(observer);

    // Load a resource for the main frame before it commits.
    std::vector<mojom::ResourceDataUpdatePtr> resources;
    mojom::ResourceDataUpdatePtr resource = mojom::ResourceDataUpdate::New();
    resource->received_data_length = 10 * 1024;
    resource->delta_bytes = 10 * 1024;
    resource->encoded_body_length = 10 * 1024;
    resource->cache_type = mojom::CacheType::kNotCached;
    resource->is_complete = true;
    resource->is_primary_frame_resource = true;
    resources.push_back(std::move(resource));
    auto timing = mojom::PageLoadTimingPtr(std::in_place);
    InitPageLoadTimingForTest(timing.get());
    observer->OnTimingUpdated(
        navigation_handle()->GetRenderFrameHost(), std::move(timing),
        mojom::FrameMetadataPtr(std::in_place),
        std::vector<blink::UseCounterFeature>(), resources,
        mojom::FrameRenderDataUpdatePtr(std::in_place),
        mojom::CpuTimingPtr(std::in_place),
        mojom::InputTimingPtr(std::in_place), std::nullopt,
        mojom::SoftNavigationMetrics::New());
  }
};

// Mock noise provider which always gives a supplied value of noise for the
// heavy ad intervention thresholds.
class MockNoiseProvider
    : public AdsPageLoadMetricsObserver::HeavyAdThresholdNoiseProvider {
 public:
  explicit MockNoiseProvider(int noise)
      : HeavyAdThresholdNoiseProvider(true /* use_noise */), noise_(noise) {}
  ~MockNoiseProvider() override = default;

  int GetNetworkThresholdNoiseForFrame() const override { return noise_; }

 private:
  int noise_;
};

std::string SuffixedHistogram(const std::string& suffix) {
  return base::StringPrintf("PageLoad.Clients.Ads.%s", suffix.c_str());
}

// Verifies that the histograms match what is expected. Frames that should not
// be recorded (due to zero bytes and zero CPU usage) should be not be
// represented in |ad_frames|.
void TestHistograms(const base::HistogramTester& histograms,
                    const ukm::TestAutoSetUkmRecorder& ukm_recorder,
                    const std::vector<ExpectedFrameBytes>& ad_frames,
                    size_t non_ad_cached_kb,
                    size_t non_ad_uncached_kb) {
  size_t total_ad_uncached_kb = 0;
  size_t total_ad_kb = 0;
  size_t ad_frame_count = 0;

  std::map<size_t, int> frames_with_total_byte_count;
  std::map<size_t, int> frames_with_network_byte_count;
  std::map<size_t, int> frames_with_percent_network_count;

  // This map is keyed by (total bytes, network bytes).
  std::map<ExpectedFrameBytes, int> frame_byte_counts;

  // Perform some initial calculations on the number of bytes, of each type,
  // in each ad frame.
  for (const ExpectedFrameBytes& bytes : ad_frames) {
    total_ad_uncached_kb += bytes.uncached_kb;
    total_ad_kb += bytes.cached_kb + bytes.uncached_kb;

    ad_frame_count += 1;

    size_t total_frame_kb = bytes.cached_kb + bytes.uncached_kb;

    frames_with_total_byte_count[total_frame_kb] += 1;
    frames_with_network_byte_count[bytes.uncached_kb] += 1;
    if (total_frame_kb > 0) {
      frames_with_percent_network_count[(bytes.uncached_kb * 100) /
                                        total_frame_kb] += 1;
    }
    frame_byte_counts[bytes] += 1;
  }

  // Test the histograms.
  histograms.ExpectUniqueSample(SuffixedHistogram("FrameCounts.AdFrames.Total"),
                                ad_frame_count, 1);

  if (ad_frame_count == 0)
    return;

  for (const auto& total_bytes_and_count : frames_with_total_byte_count) {
    histograms.ExpectBucketCount(
        SuffixedHistogram("Bytes.AdFrames.PerFrame.Total2"),
        total_bytes_and_count.first, total_bytes_and_count.second);
  }
  for (const auto& network_bytes_and_count : frames_with_network_byte_count) {
    histograms.ExpectBucketCount(
        SuffixedHistogram("Bytes.AdFrames.PerFrame.Network"),
        network_bytes_and_count.first, network_bytes_and_count.second);
  }

  histograms.ExpectUniqueSample(
      SuffixedHistogram("Bytes.AdFrames.Aggregate.Total2"), total_ad_kb, 1);
  histograms.ExpectUniqueSample(
      SuffixedHistogram("Bytes.AdFrames.Aggregate.Network"),
      total_ad_uncached_kb, 1);
  histograms.ExpectUniqueSample(
      SuffixedHistogram("Bytes.FullPage.Total2"),
      non_ad_cached_kb + non_ad_uncached_kb + total_ad_kb, 1);
  histograms.ExpectUniqueSample(SuffixedHistogram("Bytes.FullPage.Network"),
                                non_ad_uncached_kb + total_ad_uncached_kb, 1);
  histograms.ExpectUniqueSample(
      SuffixedHistogram("Bytes.NonAdFrames.Aggregate.Total2"),
      non_ad_cached_kb + non_ad_uncached_kb, 1);
  if (total_ad_kb + non_ad_cached_kb + non_ad_uncached_kb > 0) {
    histograms.ExpectUniqueSample(
        SuffixedHistogram("Bytes.FullPage.Total2.PercentAdFrames"),
        (total_ad_kb * 100) /
            (total_ad_kb + non_ad_cached_kb + non_ad_uncached_kb),
        1);
  }
  if (total_ad_uncached_kb + non_ad_uncached_kb > 0) {
    histograms.ExpectUniqueSample(
        SuffixedHistogram("Bytes.FullPage.Network.PercentAdFrames"),
        (total_ad_uncached_kb * 100) /
            (total_ad_uncached_kb + non_ad_uncached_kb),
        1);
  }

  // Verify AdFrameLoad UKM metrics.
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(ad_frame_count, entries.size());

  for (const auto& byte_count : frame_byte_counts) {
    size_t cached_bytes = byte_count.first.cached_kb * 1024;
    size_t network_bytes = byte_count.first.uncached_kb * 1024;
    int matching_entries = 0;
    for (const ukm::mojom::UkmEntry* entry : entries) {
      int64_t entry_cache_bytes = *ukm_recorder.GetEntryMetric(
          entry, ukm::builders::AdFrameLoad::kLoading_CacheBytes2Name);
      int64_t entry_network_bytes = *ukm_recorder.GetEntryMetric(
          entry, ukm::builders::AdFrameLoad::kLoading_NetworkBytesName);
      if (entry_cache_bytes ==
              ukm::GetExponentialBucketMinForBytes(cached_bytes) &&
          entry_network_bytes ==
              ukm::GetExponentialBucketMinForBytes(network_bytes))
        matching_entries++;
    }
    EXPECT_EQ(matching_entries, byte_count.second);
  }
}

// Waits for an error page for the heavy ad intervention to be navigated to.
class ErrorPageWaiter : public content::WebContentsObserver {
 public:
  explicit ErrorPageWaiter(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}
  ~ErrorPageWaiter() override = default;

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(content::NavigationHandle* handle) override {
    if (handle->GetNetErrorCode() != net::ERR_BLOCKED_BY_CLIENT) {
      is_error_page_ = false;
      return;
    }

    is_error_page_ = true;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  // Immediately returns if we are on an error page.
  void WaitForError() {
    if (is_error_page_)
      return;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Returns if the last observed navigation was an error page.
  bool LastPageWasErrorPage() { return is_error_page_; }

 private:
  base::OnceClosure quit_closure_;
  bool is_error_page_ = false;
};

// Mock frame remote. Processes calls to SendInterventionReport and waits
// for all pending messages to be sent.
class FrameRemoteTester : public content::FakeLocalFrame {
 public:
  FrameRemoteTester() = default;
  ~FrameRemoteTester() override = default;

  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(this,
                   mojo::PendingAssociatedReceiver<blink::mojom::LocalFrame>(
                       std::move(handle)));
  }

  // blink::mojom::LocalFrame
  void SendInterventionReport(const std::string& id,
                              const std::string& message) override {
    if (id.empty()) {
      if (!on_empty_report_callback_)
        return;

      std::move(on_empty_report_callback_).Run();
      return;
    }

    last_message_ = message;
    had_message_ = true;
  }

  // Sends an empty message and waits for it to be received. Returns true if any
  // other messages were received.
  bool FlushForTesting(RenderFrameHost* render_frame_host) {
    base::RunLoop run_loop;
    on_empty_report_callback_ = run_loop.QuitClosure();
    render_frame_host->SendInterventionReport("", "");
    run_loop.Run();
    int had_message = had_message_;
    had_message_ = false;
    return had_message;
  }

  // Returns the last observed report message and then clears it.
  std::string PopLastInterventionReportMessage() {
    std::string last_message = last_message_;
    last_message_ = "";
    return last_message;
  }

 private:
  bool had_message_ = false;

  // The message string for the last received non-empty intervention report.
  std::string last_message_;
  base::OnceClosure on_empty_report_callback_;
  mojo::AssociatedReceiverSet<blink::mojom::LocalFrame> receivers_;
};

}  // namespace

class AdsPageLoadMetricsObserverTest
    : public subresource_filter::SubresourceFilterTestHarness,
      public blocklist::OptOutBlocklistDelegate,
      public testing::WithParamInterface<bool> {
 public:
  AdsPageLoadMetricsObserverTest()
      : test_blocklist_(
            std::make_unique<heavy_ad_intervention::HeavyAdBlocklist>(
                nullptr,
                base::DefaultClock::GetInstance(),
                this)) {}

  AdsPageLoadMetricsObserverTest(const AdsPageLoadMetricsObserverTest&) =
      delete;
  AdsPageLoadMetricsObserverTest& operator=(
      const AdsPageLoadMetricsObserverTest&) = delete;

  void SetUp() override {
    SetUpScopedFeatureList();
    SubresourceFilterTestHarness::SetUp();
    tester_ = std::make_unique<PageLoadMetricsObserverTester>(
        web_contents(), this,
        base::BindRepeating(&AdsPageLoadMetricsObserverTest::RegisterObservers,
                            base::Unretained(this)));
    ConfigureAsSubresourceFilterOnlyURL(GURL(kAdUrl));

    // Run all sites in dry run mode, so that AdTagging works as expected. In
    // browser environments, all sites activate with dry run by default.
    scoped_configuration().ResetConfiguration(subresource_filter::Configuration(
        subresource_filter::mojom::ActivationLevel::kDryRun,
        subresource_filter::ActivationScope::ALL_SITES,
        subresource_filter::ActivationList::SUBRESOURCE_FILTER));
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* NavigateFrame(
      const std::string& url,
      content::RenderFrameHost* frame,
      base::TimeTicks nav_start = base::TimeTicks()) {
    std::unique_ptr<NavigationSimulator> navigation_simulator =
        CreateNavigationSimulator(url, frame);
    ;
    if (!nav_start.is_null()) {
      navigation_simulator->SetNavigationStart(nav_start);
    }
    navigation_simulator->Commit();
    return navigation_simulator->GetFinalRenderFrameHost();
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* NavigateMainFrame(const std::string& url) {
    return NavigateFrame(url, web_contents()->GetPrimaryMainFrame());
  }

  void OnCpuTimingUpdate(RenderFrameHost* render_frame_host,
                         base::TimeDelta cpu_time_spent) {
    mojom::CpuTiming cpu_timing(cpu_time_spent);
    tester_->SimulateCpuTimingUpdate(cpu_timing, render_frame_host);
  }

  // Sends |total_time| in CPU timing updates spread across a variable amount of
  // 30 second windows to not hit the peak window usage cap for the heavy ad
  // intervention.
  void UseCpuTimeUnderThreshold(RenderFrameHost* render_frame_host,
                                base::TimeDelta total_time) {
    const base::TimeDelta peak_threshold = base::Milliseconds(
        heavy_ad_thresholds::kMaxPeakWindowedPercent * 30000 / 100 - 1);
    for (; total_time > peak_threshold; total_time -= peak_threshold) {
      OnCpuTimingUpdate(render_frame_host, peak_threshold);
      AdvancePageDuration(base::Seconds(31));
    }
    OnCpuTimingUpdate(render_frame_host, total_time);
  }

  void AdvancePageDuration(base::TimeDelta delta) { clock_->Advance(delta); }

  RenderFrameHost* AppendChildFrame(content::RenderFrameHost* parent) {
    if (WithFencedFrames()) {
      return RenderFrameHostTester::For(parent)->AppendFencedFrame();
    }
    return RenderFrameHostTester::For(parent)->AppendChild("frame_name");
  }

  std::unique_ptr<NavigationSimulator> CreateNavigationSimulator(
      const std::string& url,
      content::RenderFrameHost* frame) {
    return NavigationSimulator::CreateRendererInitiated(GURL(url), frame);
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* CreateAndNavigateSubFrame(
      const std::string& url,
      content::RenderFrameHost* parent,
      base::TimeTicks nav_start_override = base::TimeTicks()) {
    RenderFrameHost* subframe = AppendChildFrame(parent);
    std::unique_ptr<NavigationSimulator> navigation_simulator =
        CreateNavigationSimulator(url, subframe);
    if (!nav_start_override.is_null()) {
      navigation_simulator->SetNavigationStart(nav_start_override);
    }
    navigation_simulator->Commit();

    blink::AssociatedInterfaceProvider* remote_interfaces =
        navigation_simulator->GetFinalRenderFrameHost()
            ->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        blink::mojom::LocalFrame::Name_,
        base::BindRepeating(&FrameRemoteTester::BindPendingReceiver,
                            base::Unretained(&frame_remote_tester_)));
    // The override above will only apply when a new LocalFrame is bound. Reset the existing
    // LocalFrame to force binding of a new LocalFrame.
    RenderFrameHostTester::For(navigation_simulator->GetFinalRenderFrameHost())
        ->ResetLocalFrame();

    return navigation_simulator->GetFinalRenderFrameHost();
  }

  void ResourceDataUpdate(RenderFrameHost* render_frame_host,
                          ResourceCached resource_cached,
                          int resource_size_in_kbyte,
                          std::string mime_type = "",
                          bool is_ad_resource = false) {
    std::vector<mojom::ResourceDataUpdatePtr> resources;
    mojom::ResourceDataUpdatePtr resource = mojom::ResourceDataUpdate::New();
    resource->received_data_length =
        static_cast<bool>(resource_cached) ? 0 : resource_size_in_kbyte << 10;
    resource->delta_bytes = resource->received_data_length;
    resource->encoded_body_length = resource_size_in_kbyte << 10;
    resource->reported_as_ad_resource = is_ad_resource;
    resource->is_complete = true;
    switch (resource_cached) {
      case ResourceCached::kNotCached:
        resource->cache_type = mojom::CacheType::kNotCached;
        break;
      case ResourceCached::kCachedHttp:
        resource->cache_type = mojom::CacheType::kHttp;
        break;
      case ResourceCached::kCachedMemory:
        resource->cache_type = mojom::CacheType::kMemory;
        break;
    }
    resource->mime_type = mime_type;
    resource->is_primary_frame_resource = true;
    resource->is_main_frame_resource =
        render_frame_host->GetFrameTreeNodeId() ==
        main_rfh()->GetFrameTreeNodeId();
    resources.push_back(std::move(resource));
    tester_->SimulateResourceDataUseUpdate(resources, render_frame_host);
  }

  // Simulates FirstEligibleToPaint and/or FirstContentfulPaint
  // and then runs a timing update. Note that a simulation of
  // both of these separately one after the other doesn't work because
  // the second call to SimulateTimingUpdate interferes with the results
  // of the first call.
  void SimulateFirstEligibleToPaintOrFirstContentfulPaint(
      RenderFrameHost* frame,
      std::optional<base::TimeDelta> first_eligible_to_paint,
      std::optional<base::TimeDelta> first_contentful_paint) {
    InitPageLoadTimingForTest(&timing_);
    timing_.navigation_start = base::Time::Now();
    timing_.parse_timing->parse_start = kParseStartTime;
    timing_.paint_timing->first_eligible_to_paint = first_eligible_to_paint;
    if (first_contentful_paint.has_value())
      timing_.paint_timing->first_contentful_paint =
          first_contentful_paint.value();
    PopulateRequiredTimingFieldsExceptFEtPAndFCP(&timing_);
    tester()->SimulateTimingUpdate(timing_, frame);
  }

  void SimulateFirstContentfulPaint(
      RenderFrameHost* frame,
      std::optional<base::TimeDelta> first_contentful_paint) {
    SimulateFirstEligibleToPaintOrFirstContentfulPaint(
        frame, first_contentful_paint /* first_eligible_to_paint */,
        first_contentful_paint /* first_contentful_paint */);
  }

  // Given |creative_origin_test|, creates nested frames in the order given in
  // |creative_origin_test.urls|, causes the frame with index
  // |creative_origin_test.creative_index| to paint text first, and verifies
  // that the creative's origin matches
  // |creative_origin_test.expected_origin_status|.
  void TestCreativeOriginStatus(
      const CreativeOriginTest& creative_origin_test) {
    const char kCreativeOriginStatusHistogramId[] =
        "PageLoad.Clients.Ads.FrameCounts.AdFrames.PerFrame."
        "CreativeOriginStatus";

    base::HistogramTester histograms;

    // Navigate main frame.
    RenderFrameHost* main_frame =
        NavigateMainFrame(creative_origin_test.urls.front());
    std::vector<RenderFrameHost*> frames;
    frames.push_back(main_frame);

    // Create and navigate each subframe so that it has the origin given at
    // the corresponding index of |creative_origin_test.urls|.
    RenderFrameHost* current_frame = main_frame;
    for (size_t i = 1; i < creative_origin_test.urls.size(); ++i) {
      // Create subframe and page load timing.
      current_frame = CreateAndNavigateSubFrame(creative_origin_test.urls[i],
                                                current_frame);
      frames.push_back(current_frame);

      // Load bytes in frame.
      ResourceDataUpdate(current_frame, ResourceCached::kNotCached, 10);
    }

    // In order to test that |creative_origin_status_| in FrameTreeData is
    // properly computed, we need to simulate first contentful paint for the ad
    // creative first at |kCreativeFCPTime|.
    base::TimeDelta eligible_time = kCreativeEligibleToPaintTime;
    base::TimeDelta fcp_time = kCreativeFCPTime;
    SimulateFirstEligibleToPaintOrFirstContentfulPaint(
        frames[creative_origin_test.creative_index], eligible_time, fcp_time);

    // Now simulate first contentful paint for the other frames at
    // |kOtherFrameFCPTime|.
    eligible_time = kOtherFrameEligibleToPaintTime;
    fcp_time = kOtherFrameFCPTime;

    for (size_t i = 0; i < frames.size(); ++i) {
      if (i == creative_origin_test.creative_index)
        continue;

      SimulateFirstEligibleToPaintOrFirstContentfulPaint(
          frames[i], eligible_time, fcp_time);
    }

    // Navigate again to trigger histograms, then test them.
    NavigateFrame(kNonAdUrl, main_frame);
    histograms.ExpectUniqueSample(kCreativeOriginStatusHistogramId,
                                  creative_origin_test.expected_origin_status,
                                  1);
  }

  // Given |creative_origin_test|, creates nested frames in the order given in
  // |creative_origin_test.urls|, causes the frame with index
  // |creative_origin_test.creative_index| to paint text first, and verifies
  // that the creative's origin matches
  // |creative_origin_test.expected_origin_status|. This test variation has
  // added parameters in the CreativeOriginTestWithThrottling struct, namely
  // a vector of booleans to denote whether the corresponding frame in |urls|
  // is to be throttled, and a single bool indicating whether or not to simulate
  // any first contentful paints, so that the case
  // OriginStatusWithThrottling::kUnknownAndUnthrottled
  // can be tested.
  void TestCreativeOriginStatusWithThrottling(
      const CreativeOriginTestWithThrottling& creative_origin_test) {
    const char kCreativeOriginStatusWithThrottlingHistogramId[] =
        "PageLoad.Clients.Ads.FrameCounts.AdFrames.PerFrame."
        "CreativeOriginStatusWithThrottling";

    base::HistogramTester histograms;

    // Navigate main frame.
    RenderFrameHost* main_frame =
        NavigateMainFrame(creative_origin_test.page_url);

    // Create and navigate each subframe so that it has the origin given at
    // the corresponding index of |frame_origins.subframe_urls|.
    std::vector<RenderFrameHost*> frames;
    RenderFrameHost* current_frame = main_frame;
    for (const std::string& url : creative_origin_test.subframe_urls) {
      // Create subframe and page load timing.
      current_frame = CreateAndNavigateSubFrame(url, current_frame);
      frames.push_back(current_frame);

      // Load bytes in frame.
      ResourceDataUpdate(current_frame, ResourceCached::kNotCached, 10);
    }

    // Create a vector of indices to easily ensure frames are processed in
    // correct order. The creative frame must be processed before any of
    // the other ad subframes.
    std::vector<size_t> indices;
    indices.push_back(creative_origin_test.creative_index);
    for (size_t i = 0; i < frames.size(); ++i) {
      if (i == creative_origin_test.creative_index)
        continue;
      indices.push_back(i);
    }

    // In order to test that |creative_origin_status_| and
    // |first_eligible_to_paint_| in FrameTreeData are properly
    // computed, we need to simulate eligibility to paint and first
    // contentful paint for the ad creative, unless it is render-throttled,
    // and then do similarly for the other subframes.
    for (size_t i : indices) {
      bool is_creative = (i == creative_origin_test.creative_index);
      base::TimeDelta eligible_time = is_creative
                                          ? kCreativeEligibleToPaintTime
                                          : kOtherFrameEligibleToPaintTime;
      base::TimeDelta fcp_time =
          is_creative ? kCreativeFCPTime : kOtherFrameFCPTime;

      bool is_throttled = creative_origin_test.throttled[i];
      bool should_paint = creative_origin_test.should_paint;

      if (!is_throttled && should_paint) {
        SimulateFirstEligibleToPaintOrFirstContentfulPaint(
            frames[i], eligible_time, fcp_time);
      } else if (!is_throttled) {
        SimulateFirstEligibleToPaintOrFirstContentfulPaint(
            frames[i], eligible_time, std::nullopt);
      } else {
        SimulateFirstEligibleToPaintOrFirstContentfulPaint(
            frames[i], std::nullopt, std::nullopt);
      }
    }

    // Navigate again to trigger histograms, then test them.
    NavigateFrame(kNonAdUrl, main_frame);
    histograms.ExpectUniqueSample(
        kCreativeOriginStatusWithThrottlingHistogramId,
        creative_origin_test.expected_origin_status, 1);
  }

  void SimulateFCPPostAuctions(
      std::vector<CompleteAuctionResult> auction_results) {
    base::TimeTicks start = base::TimeTicks::Now();
    RenderFrameHost* main_frame =
        NavigateFrame(kNonAdUrl, web_contents()->GetPrimaryMainFrame(), start);

    auto* recorder =
        page_load_metrics::MetricsWebContentsObserver::FromWebContents(
            content::WebContents::FromRenderFrameHost(main_frame));
    for (const CompleteAuctionResult& auction_result : auction_results) {
      recorder->OnAdAuctionComplete(
          main_frame, auction_result.is_server_auction,
          auction_result.is_on_device_auction, auction_result.result);
    }

    RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(
        kAdUrl, main_frame, start + base::Milliseconds(100));

    // Load some bytes so that the frame is recorded.
    ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 100);

    // Set FirstContentfulPaint.
    SimulateFirstContentfulPaint(ad_frame, base::Milliseconds(100));

    // Navigate away.
    NavigateFrame(kNonAdUrl, main_frame);
  }

  PageLoadMetricsObserverTester* tester() { return tester_.get(); }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  const ukm::TestAutoSetUkmRecorder& test_ukm_recorder() const {
    return test_ukm_recorder_;
  }

  heavy_ad_intervention::HeavyAdBlocklist* blocklist() {
    return test_blocklist_.get();
  }

  // Flushes all intervention report messages and returns a bool if there was
  // a message.
  bool HasInterventionReportsAfterFlush(RenderFrameHost* render_frame_host) {
    return frame_remote_tester_.FlushForTesting(render_frame_host);
  }

  std::string PopLastInterventionReportMessage() {
    return frame_remote_tester_.PopLastInterventionReportMessage();
  }

  void OverrideWithMockClock() {
    clock_ = std::make_unique<base::SimpleTestTickClock>();
    clock_->SetNowTicks(base::TimeTicks::Now());
  }

  void OverrideHeavyAdNoiseProvider(
      std::unique_ptr<MockNoiseProvider> noise_provider) {
    ads_observer_->SetHeavyAdThresholdNoiseProviderForTesting(
        std::move(noise_provider));
  }

  // Given the prefix of the CPU TotalUsage2 histogram to check, either
  // "FullPage" or "AdFrames.PerFrame", as well as the suffix for distinguishing
  // between "Activated" and "Unactivated" (blank if none), will check the
  // relevant histogram, ensuring it's empty if there is no task_time, or it
  // has the correct task_time for the tasks performed otherwise.
  void CheckTotalUsageHistogram(std::string prefix,
                                std::optional<int> task_time,
                                std::string suffix = "") {
    suffix = suffix.empty() ? "" : "." + suffix;
    if (task_time.has_value()) {
      histogram_tester().ExpectUniqueSample(
          SuffixedHistogram("Cpu." + prefix + ".TotalUsage2" + suffix),
          task_time.value(), 1);
    } else {
      histogram_tester().ExpectTotalCount(
          SuffixedHistogram("Cpu." + prefix + ".TotalUsage2" + suffix), 0);
    }
  }

  // A shorcut that given pre- and post-activation task time (if they exist),
  // will check the three relevant TotalUsage histograms.
  void CheckActivatedTotalUsageHistograms(std::optional<int> pre_task_time,
                                          std::optional<int> post_task_time) {
    std::optional<int> total_task_time;
    if (pre_task_time.has_value() || post_task_time.has_value())
      total_task_time = pre_task_time.value_or(0) + post_task_time.value_or(0);

    CheckTotalUsageHistogram("AdFrames.PerFrame", total_task_time, "Activated");
    CheckTotalUsageHistogram("AdFrames.PerFrame", pre_task_time,
                             "Activated.PreActivation");
    CheckTotalUsageHistogram("AdFrames.PerFrame", post_task_time,
                             "Activated.PostActivation");
  }

  void SimulateV8MemoryChange(content::RenderFrameHost* render_frame_host,
                              int64_t delta_bytes) {
    tester()->SimulateMemoryUpdate(render_frame_host, delta_bytes);
  }

 protected:
  virtual void SetUpScopedFeatureList() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {blink::features::kFencedFrames,
             {{"implementation_type", "mparch"}}},
        },
        {});
  }

  bool WithFencedFrames() { return GetParam(); }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<PageLoadMetricsObserverTester> tester_;

 private:
  // SubresourceFilterTestHarness::
  void AppendCustomNavigationThrottles(
      content::NavigationHandle* navigation_handle,
      std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles)
      override {
    if (navigation_handle->IsInMainFrame()) {
      throttles->push_back(
          MetricsNavigationThrottle::Create(navigation_handle));
    }
  }

  void RegisterObservers(PageLoadTracker* tracker) {
    auto observer = std::make_unique<AdsPageLoadMetricsObserver>(
        /*heavy_ad_service=*/nullptr,
        base::BindRepeating([]() { return std::string("en-US"); }),
        clock_.get(), test_blocklist_.get());
    // Mock the noise provider to make tests deterministic. Tests can override
    // this again to test non-zero noise.
    observer->SetHeavyAdThresholdNoiseProviderForTesting(
        std::make_unique<MockNoiseProvider>(0 /* noise */));

    // Install the observer into each PageLoadTracker, but as now tests are
    // interested only in behaviors of the observer for the outermost page,
    // we'd take the only pointer of the outermost tracker.
    if (tracker->IsOutermostTracker()) {
      ads_observer_ = observer.get();
    }

    tracker->AddObserver(std::move(observer));

    // Swap out the ui::ScopedVisibilityTracker to use the test clock.
    if (clock_) {
      ui::ScopedVisibilityTracker visibility_tracker(clock_.get(), true);
      tracker->SetVisibilityTrackerForTesting(visibility_tracker);
    }
  }

  std::unique_ptr<heavy_ad_intervention::HeavyAdBlocklist> test_blocklist_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  FrameRemoteTester frame_remote_tester_;
  mojom::PageLoadTiming timing_;

  // The clock used by the ui::ScopedVisibilityTracker and PageAdDensityTracker,
  // assigned if non-null.
  std::unique_ptr<base::SimpleTestTickClock> clock_;

  // A pointer to the AdsPageLoadMetricsObserver used by the tests.
  raw_ptr<AdsPageLoadMetricsObserver, AcrossTasksDanglingUntriaged>
      ads_observer_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(All, AdsPageLoadMetricsObserverTest, testing::Bool());

TEST_P(AdsPageLoadMetricsObserverTest, PageWithNoAds) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* frame1 = CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  RenderFrameHost* frame2 = CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(frame1, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(frame2, ResourceCached::kNotCached, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(),
                 std::vector<ExpectedFrameBytes>(), 0 /* non_ad_cached_kb */,
                 30 /* non_ad_uncached_kb */);

  // Verify that other UMA wasn't written.
  histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Total", 0);
}

TEST_P(AdsPageLoadMetricsObserverTest, PageWithAds) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* frame1 = CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  RenderFrameHost* frame2 = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(frame1, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(frame2, ResourceCached::kNotCached, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 10}},
                 0 /* non_ad_cached_kb */, 20 /* non_ad_uncached_kb */);
}

TEST_P(AdsPageLoadMetricsObserverTest, PageWithAdsButNoAdFrame) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 40,
                     "" /* mime_type */, false /* is_ad_resource */);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10,
                     "" /* mime_type */, true /* is_ad_resource */);
  ResourceDataUpdate(main_frame, ResourceCached::kCachedHttp, 30,
                     "" /* mime_type */, false /* is_ad_resource */);
  ResourceDataUpdate(main_frame, ResourceCached::kCachedHttp, 20,
                     "" /* mime_type */, true /* is_ad_resource */);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {},
                 50 /* non_ad_cached_kb */, 50 /* non_ad_uncached_kb */);

  // We expect the ad bytes percentages to be correctly reported, even though
  // there was no ad frame.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("AllPages.PercentNetworkBytesAds"), 20, 1);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("AllPages.PercentTotalBytesAds"), 30, 1);

  // Verify that the non-ad network bytes were recorded correctly.
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.AllPages.NonAdNetworkBytes", 40, 1);
}

TEST_P(AdsPageLoadMetricsObserverTest, AdFrameMimeTypeBytes) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(
      ad_frame, ResourceCached::kNotCached, 10 /* resource_size_in_kbyte */,
      "application/javascript" /* mime_type */, true /* is_ad_resource */);
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     20 /* resource_size_in_kbyte */,
                     "image/png" /* mime_type */, true /* is_ad_resource */);
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     30 /* resource_size_in_kbyte */,
                     "video/webm" /* mime_type */, true /* is_ad_resource */);

  // Cached resource not counted.
  ResourceDataUpdate(ad_frame, ResourceCached::kCachedHttp,
                     40 /* resource_size_in_kbyte */,
                     "video/webm" /* mime_type */, true /* is_ad_resource */);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);
  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_JavascriptBytesName,
      ukm::GetExponentialBucketMinForBytes(10 * 1024));
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_ImageBytesName,
      ukm::GetExponentialBucketMinForBytes(20 * 1024));
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_VideoBytesName,
      ukm::GetExponentialBucketMinForBytes(30 * 1024));
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_NetworkBytesName,
      ukm::GetExponentialBucketMinForBytes(60 * 1024));
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_CacheBytes2Name,
      ukm::GetExponentialBucketMinForBytes(40 * 1024));
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_NumResourcesName,
      4);
}

TEST_P(AdsPageLoadMetricsObserverTest, ResourceBeforeAdFrameCommits) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);

  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);

  // Create subframe and load resource before commit.
  RenderFrameHost* subframe = AppendChildFrame(main_frame);
  auto navigation_simulator = CreateNavigationSimulator(kAdUrl, subframe);
  ResourceDataUpdate(subframe, ResourceCached::kNotCached, 10);
  navigation_simulator->Commit();

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 10}},
                 0 /* non_ad_cached_kb */, 10 /*non_ad_uncached_kb*/);
}

// Test that the cross-origin ad subframe navigation metric works as it's
// supposed to, triggering a false addition with each ad that's in the same
// origin as the main page, and a true when when the ad has a separate origin.
TEST_P(AdsPageLoadMetricsObserverTest, AdsOriginStatusMetrics) {
  const char kCrossOriginHistogramId[] =
      "PageLoad.Clients.Ads.FrameCounts.AdFrames.PerFrame."
      "OriginStatus";

  // Test that when the main frame origin is different from a direct ad
  // subframe it is correctly identified as cross-origin, but do not count
  // indirect ad subframes.
  {
    base::HistogramTester histograms;
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
    RenderFrameHost* ad_sub_frame =
        CreateAndNavigateSubFrame(kAdUrl, main_frame);
    ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
    ResourceDataUpdate(ad_sub_frame, ResourceCached::kNotCached, 10);
    ResourceDataUpdate(CreateAndNavigateSubFrame(kAdUrl, ad_sub_frame),
                       ResourceCached::kNotCached, 10);
    // Trigger histograms by navigating away, then test them.
    NavigateFrame(kAdUrl, main_frame);
    histograms.ExpectUniqueSample(kCrossOriginHistogramId, OriginStatus::kCross,
                                  1);
    auto entries =
        ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
    EXPECT_EQ(1u, entries.size());
    ukm_recorder.ExpectEntryMetric(
        entries.front(), ukm::builders::AdFrameLoad::kStatus_CrossOriginName,
        static_cast<int64_t>(OriginStatus::kCross));
  }

  // Add a non-ad subframe and an ad subframe and make sure the total count
  // only adjusts by one.
  {
    base::HistogramTester histograms;
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
    ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
    ResourceDataUpdate(CreateAndNavigateSubFrame(kAdUrl, main_frame),
                       ResourceCached::kNotCached, 10);
    ResourceDataUpdate(CreateAndNavigateSubFrame(kNonAdUrl, main_frame),
                       ResourceCached::kNotCached, 10);
    // Trigger histograms by navigating away, then test them.
    NavigateFrame(kAdUrl, main_frame);
    histograms.ExpectUniqueSample(kCrossOriginHistogramId, OriginStatus::kCross,
                                  1);
    auto entries =
        ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
    EXPECT_EQ(1u, entries.size());
    ukm_recorder.ExpectEntryMetric(
        entries.front(), ukm::builders::AdFrameLoad::kStatus_CrossOriginName,
        static_cast<int64_t>(OriginStatus::kCross));
  }

  // Add an ad subframe in the same origin as the parent frame and make sure it
  // gets identified as non-cross-origin. Note: top-level navigations are never
  // considered to be ads.
  {
    base::HistogramTester histograms;
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrlSameOrigin);
    ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
    ResourceDataUpdate(CreateAndNavigateSubFrame(kAdUrl, main_frame),
                       ResourceCached::kNotCached, 10);
    // Trigger histograms by navigating away, then test them.
    NavigateFrame(kAdUrl, main_frame);
    histograms.ExpectUniqueSample(kCrossOriginHistogramId, OriginStatus::kSame,
                                  1);
    auto entries =
        ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
    EXPECT_EQ(1u, entries.size());
    ukm_recorder.ExpectEntryMetric(
        entries.front(), ukm::builders::AdFrameLoad::kStatus_CrossOriginName,
        static_cast<int64_t>(OriginStatus::kSame));
  }
}

TEST_P(AdsPageLoadMetricsObserverTest, PageWithAdFrameThatRenavigates) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Navigate the ad frame again.
  ad_frame = NavigateFrame(kAdUrl, ad_frame);

  // In total, 30KB for entire page and 20 in one ad frame.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 20}},
                 0 /* non_ad_cached_kb */, 10 /* non_ad_uncached_kb */);
}

TEST_P(AdsPageLoadMetricsObserverTest, PageWithNonAdFrameThatRenavigatesToAd) {
  // Main frame.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);

  // Sub frame that is not an ad.
  RenderFrameHost* sub_frame = CreateAndNavigateSubFrame(kNonAdUrl, main_frame);

  // Child of the sub-frame that is an ad.
  RenderFrameHost* sub_frame_child_ad =
      CreateAndNavigateSubFrame(kAdUrl, sub_frame);

  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(sub_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(sub_frame_child_ad, ResourceCached::kNotCached, 10);

  // Navigate the subframe again, this time it's an ad.
  sub_frame = NavigateFrame(kAdUrl, sub_frame);
  ResourceDataUpdate(sub_frame, ResourceCached::kNotCached, 10);

  // In total, 40KB was loaded for the entire page and 20KB from ad
  // frames (the original child ad frame and the renavigated frame which
  // turned into an ad).

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 10}, {0, 10}},
                 0 /* non_ad_cached_kb */, 20 /* non_ad_uncached_kb */);
}

TEST_P(AdsPageLoadMetricsObserverTest, CountAbortedNavigation) {
  // If the first navigation in a frame is aborted, keep track of its bytes.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);

  // Create an ad subframe that aborts before committing.
  RenderFrameHost* subframe_ad = AppendChildFrame(main_frame);
  auto navigation_simulator = CreateNavigationSimulator(kAdUrl, subframe_ad);
  // The sub-frame renavigates before it commits.
  navigation_simulator->Start();
  SetIsAdFrame(subframe_ad, /*is_ad_frame=*/true);
  navigation_simulator->Fail(net::ERR_ABORTED);

  // Load resources for the aborted frame (e.g., simulate the navigation
  // aborting due to a doc.write during provisional navigation). They should
  // be counted.
  ResourceDataUpdate(subframe_ad, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(subframe_ad, ResourceCached::kNotCached, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 20}},
                 0 /* non_ad_cached_kb */, 10 /* non_ad_uncached_kb */);
}

TEST_P(AdsPageLoadMetricsObserverTest, CountAbortedSecondNavigationForFrame) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);

  // Sub frame that is not an ad.
  RenderFrameHost* sub_frame = CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  ResourceDataUpdate(sub_frame, ResourceCached::kNotCached, 10);

  // Now navigate (and abort) the subframe to an ad.
  auto navigation_simulator = CreateNavigationSimulator(kAdUrl, sub_frame);
  // The sub-frame renavigates before it commits.
  navigation_simulator->Start();
  SetIsAdFrame(sub_frame, /*is_ad_frame=*/true);
  navigation_simulator->Fail(net::ERR_ABORTED);

  // Load resources for the aborted frame (e.g., simulate the navigation
  // aborting due to a doc.write during provisional navigation). Since the
  // frame attempted to load an ad, the frame is tagged forever as an ad.
  ResourceDataUpdate(sub_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(sub_frame, ResourceCached::kNotCached, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 20}},
                 0 /* non_ad_cached_kb */, 20 /* non_ad_uncached_kb */);
}

TEST_P(AdsPageLoadMetricsObserverTest, TwoResourceLoadsBeforeCommit) {
  // Main frame.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);

  // Now open a subframe and have its resource load before notification of
  // navigation finishing.
  RenderFrameHost* subframe_ad = AppendChildFrame(main_frame);
  auto navigation_simulator = CreateNavigationSimulator(kAdUrl, subframe_ad);
  ResourceDataUpdate(subframe_ad, ResourceCached::kNotCached, 10);

  // The sub-frame renavigates before it commits.
  navigation_simulator->Start();
  SetIsAdFrame(subframe_ad, /*is_ad_frame=*/true);
  navigation_simulator->Fail(net::ERR_ABORTED);

  // Renavigate the subframe to a successful commit. But again, the resource
  // loads before the observer sees the finished navigation.
  ResourceDataUpdate(subframe_ad, ResourceCached::kNotCached, 10);
  NavigateFrame(kAdUrl, subframe_ad);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 20}},
                 0 /* non_ad_cached_kb */, 10 /* non_ad_uncached_kb */);
}

TEST_P(AdsPageLoadMetricsObserverTest, UntaggingAdFrame) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Renavigate and untag the ad frame.
  auto navigation_simulator = CreateNavigationSimulator(kNonAdUrl, ad_frame);
  SetIsAdFrame(ad_frame, /*is_ad_frame=*/false);
  navigation_simulator->Commit();

  ResourceDataUpdate(navigation_simulator->GetFinalRenderFrameHost(),
                     ResourceCached::kNotCached, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  // As the frame was untagged, no ad bytes should have been recorded.
  TestHistograms(histogram_tester(), test_ukm_recorder(), {},
                 0 /* non_ad_cached_kb */, 20 /* non_ad_uncached_kb */);
}

TEST_P(AdsPageLoadMetricsObserverTest, MainFrameResource) {
  // Start main-frame navigation
  auto navigation_simulator = CreateNavigationSimulator(
      kNonAdUrl, web_contents()->GetPrimaryMainFrame());
  navigation_simulator->Start();
  navigation_simulator->Commit();

  ResourceDataUpdate(navigation_simulator->GetFinalRenderFrameHost(),
                     ResourceCached::kNotCached, 10);

  NavigateMainFrame(kNonAdUrl);

  // We only log histograms if we observed bytes for the page. Verify that the
  // main frame resource was properly tracked and attributed.
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.FrameCounts.AdFrames.Total", 0, 1);

  // Verify that this histogram is also recorded for the Visible and NonVisible
  // suffixes.
  histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.Ads.Visible.FrameCounts.AdFrames.Total", 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.Ads.NonVisible.FrameCounts.AdFrames.Total", 1);

  // Verify that the ad bytes percentages were recorded as zero.
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.AllPages.PercentNetworkBytesAds", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.AllPages.PercentTotalBytesAds", 0, 1);

  // Verify that the non-ad bytes were recorded correctly.
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.AllPages.NonAdNetworkBytes", 10, 1);

  // Verify that the average-viewport-ad-density was recorded as zero.
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.AverageViewportAdDensity", 0, 1);

  // There are three FrameCounts.AdFrames.Total histograms (one for each
  // visibility type), three AllPages histograms, and one
  // AverageViewportAdDensity histogram recorded for each page load. There
  // shouldn't be any other histograms for a page with no ad resources.
  EXPECT_EQ(7u, histogram_tester()
                    .GetTotalCountsForPrefix("PageLoad.Clients.Ads.")
                    .size());
  EXPECT_EQ(0u, test_ukm_recorder()
                    .GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName)
                    .size());
}

TEST_P(AdsPageLoadMetricsObserverTest, NoBytesLoaded_NoHistogramsRecorded) {
  // Start main-frame navigation
  auto navigation_simulator = CreateNavigationSimulator(
      kNonAdUrl, web_contents()->GetPrimaryMainFrame());
  navigation_simulator->Start();
  navigation_simulator->Commit();

  NavigateMainFrame(kNonAdUrl);

  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.AverageViewportAdDensity", 0, 1);

  // Other histograms should not be recorded for a page with no bytes.
  EXPECT_EQ(1u, histogram_tester()
                    .GetTotalCountsForPrefix("PageLoad.Clients.Ads.")
                    .size());
  EXPECT_EQ(0u, test_ukm_recorder()
                    .GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName)
                    .size());
}

// Make sure that ads histograms aren't recorded if the tracker never commits
// (see https://crbug.com/723219).
TEST_P(AdsPageLoadMetricsObserverTest, NoHistogramWithoutCommit) {
  {
    // Once the metrics observer has the GlobalRequestID, throttle.
    content::TestNavigationThrottleInserter throttle_inserter(
        web_contents(),
        base::BindRepeating(&ResourceLoadingCancellingThrottle::Create));

    // Start main-frame navigation. The commit will defer after calling
    // WillProcessNavigationResponse, it will load a resource, and then the
    // throttle will cancel the commit.
    SimulateNavigateAndCommit(GURL(kNonAdUrl), main_rfh());
  }

  // Force navigation to a new page to make sure OnComplete() runs for the
  // previous failed navigation.
  NavigateMainFrame(kNonAdUrl);

  // There shouldn't be any histograms for an aborted main frame.
  EXPECT_EQ(0u, histogram_tester()
                    .GetTotalCountsForPrefix("PageLoad.Clients.Ads.")
                    .size());
  EXPECT_EQ(0u, test_ukm_recorder()
                    .GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName)
                    .size());
}

TEST_P(AdsPageLoadMetricsObserverTest,
       SubresourceFilterDisabled_NoAdsDetected) {
  // Setup the subresource filter as disabled on all sites.
  scoped_configuration().ResetConfiguration(subresource_filter::Configuration(
      subresource_filter::mojom::ActivationLevel::kDisabled,
      subresource_filter::ActivationScope::ALL_SITES,
      subresource_filter::ActivationList::SUBRESOURCE_FILTER));

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(),
                 std::vector<ExpectedFrameBytes>(), 0 /* non_ad_cached_kb */,
                 20 /* non_ad_uncached_kb */);

  // Verify that other UMA wasn't written.
  histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Total", 0);
}

// Frames that are disallowed (and filtered) by the subresource filter should
// not be counted.
TEST_P(AdsPageLoadMetricsObserverTest, FilterAds_DoNotLogMetrics) {
  // Setup the subresource filter in non-dryrun mode to trigger on a site.
  scoped_configuration().ResetConfiguration(subresource_filter::Configuration(
      subresource_filter::mojom::ActivationLevel::kEnabled,
      subresource_filter::ActivationScope::ACTIVATION_LIST,
      subresource_filter::ActivationList::SUBRESOURCE_FILTER));

  ConfigureAsSubresourceFilterOnlyURL(GURL(kNonAdUrl));
  NavigateMainFrame(kNonAdUrl);

  ResourceDataUpdate(main_rfh(), ResourceCached::kNotCached, 10,
                     "" /* mime_type */, false /* is_ad_resource */);

  RenderFrameHost* subframe = AppendChildFrame(main_rfh());
  std::unique_ptr<NavigationSimulator> simulator =
      CreateNavigationSimulator(kDefaultDisallowedUrl, subframe);
  ResourceDataUpdate(subframe, ResourceCached::kNotCached, 10,
                     "" /* mime_type */, true /* is_ad_resource */);
  simulator->Commit();

  EXPECT_NE(content::NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult());

  NavigateMainFrame(kNonAdUrl);

  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("FrameCounts.AdFrames.Total"), 0);
}

// Per-frame histograms recorded when root ad frame is destroyed.
TEST_P(AdsPageLoadMetricsObserverTest,
       FrameDestroyed_PerFrameHistogramsLogged) {
  // TODO(crbug.com/40216775): RenderFrameHostTester::Detach() doesn't
  // work well with FencedFrames. Find a graceful way to detach it and enable
  // the test.
  if (WithFencedFrames())
    return;
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  RenderFrameHost* child_ad_frame = CreateAndNavigateSubFrame(kAdUrl, ad_frame);

  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);

  // Add some data to the ad frame so it gets reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(child_ad_frame, ResourceCached::kNotCached, 10);

  // Just delete the child frame this time.
  content::RenderFrameHostTester::For(child_ad_frame)->Detach();

  // Verify per-frame histograms not recorded.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Bytes.AdFrames.PerFrame.Total2"), 0);

  // Delete the root ad frame.
  content::RenderFrameHostTester::For(ad_frame)->Detach();

  // Verify per-frame histograms are recorded.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Bytes.AdFrames.PerFrame.Total2"), 20, 1);

  // Verify page totals not reported yet.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("FrameCounts.AdFrames.Total"), 0);

  NavigateMainFrame(kNonAdUrl);

  // Verify histograms are logged correctly for the whole page.
  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 20}},
                 0 /* non_ad_cached_kb */, 10 /* non_ad_uncached_kb */);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       FrameAbortsCommitMatchingAllowedRule_FrameTracked) {
  RenderFrameHost* main_frame = NavigateMainFrame(kAdUrl);

  // Create a frame that is tagged as ad.
  RenderFrameHost* subframe = AppendChildFrame(main_frame);
  auto navigation_simulator =
      CreateNavigationSimulator("https://foo.com", subframe);
  SetIsAdFrame(subframe, /*is_ad_frame=*/true);
  navigation_simulator->Commit();

  subframe = navigation_simulator->GetFinalRenderFrameHost();

  RenderFrameHost* nested_subframe =
      CreateAndNavigateSubFrame(kNonAdUrl, subframe);

  // Navigate the frame same-origin to a url matching an allowlist rule, but
  // abort the navigation so it does not commit.
  auto navigation_simulator2 = CreateNavigationSimulator(kAllowedUrl, subframe);
  navigation_simulator2->ReadyToCommit();
  navigation_simulator2->AbortCommit();

  // Verify per-frame metrics were not flushed.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("FrameCounts.IgnoredByRestrictedAdTagging"), 0);

  // Update the nested subframe. If the frame was untracked the underlying
  // object would be deleted.
  ResourceDataUpdate(nested_subframe, ResourceCached::kNotCached, 10);

  NavigateMainFrame(kNonAdUrl);

  // Verify histograms for the frame.
  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 10}},
                 0 /* non_ad_cached_kb */, 0 /* non_ad_uncached_kb */);
}

// Tests that a non ad frame that is deleted does not cause any unspecified
// behavior (see https://crbug.com/973954).
TEST_P(AdsPageLoadMetricsObserverTest, NonAdFrameDestroyed_FrameDeleted) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* vanilla_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, main_frame);

  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);

  content::RenderFrameHostTester::For(vanilla_frame)->Detach();

  NavigateMainFrame(kNonAdUrl);
}

// Tests that main frame ad bytes are recorded correctly.
TEST_P(AdsPageLoadMetricsObserverTest, MainFrameAdBytesRecorded) {
  NavigateMainFrame(kNonAdUrl);

  ResourceDataUpdate(main_rfh(), ResourceCached::kNotCached, 10,
                     "" /* mime_type */, true /* is_ad_resource */);
  ResourceDataUpdate(main_rfh(), ResourceCached::kCachedHttp, 10,
                     "" /* mime_type */, true /* is_ad_resource */);

  RenderFrameHost* subframe = AppendChildFrame(main_rfh());
  std::unique_ptr<NavigationSimulator> simulator =
      CreateNavigationSimulator(kDefaultDisallowedUrl, subframe);
  ResourceDataUpdate(subframe, ResourceCached::kNotCached, 10,
                     "" /* mime_type */, true /* is_ad_resource */);
  ResourceDataUpdate(subframe, ResourceCached::kCachedHttp, 10,
                     "" /* mime_type */, true /* is_ad_resource */);
  simulator->Commit();

  NavigateMainFrame(kNonAdUrl);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Bytes.MainFrame.Ads.Total2"), 20, 1);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Bytes.MainFrame.Ads.Network"), 10, 1);

  // Verify page total for network bytes.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Resources.Bytes.Ads2"), 20, 1);

  // Verify main frame ad bytes recorded in UKM.
  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdPageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_EQ(
      *test_ukm_recorder().GetEntryMetric(
          entries.front(), ukm::builders::AdPageLoad::kMainframeAdBytesName),
      ukm::GetExponentialBucketMinForBytes(10 * 1024));
}

// Tests that memory cache ad bytes are recorded correctly.
TEST_P(AdsPageLoadMetricsObserverTest, MemoryCacheAdBytesRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* frame1 = CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  RenderFrameHost* frame2 = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(frame1, ResourceCached::kCachedMemory, 10);
  ResourceDataUpdate(frame2, ResourceCached::kCachedMemory, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {{10, 0}},
                 10 /* non_ad_cached_kb */, 10 /* non_ad_uncached_kb */);
}

// UKM metrics for ad page load are recorded correctly.
// TODO(crbug.com/40669132) test is flaky on bots.
TEST_P(AdsPageLoadMetricsObserverTest, AdPageLoadUKM) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::Now();
  timing.parse_timing->parse_start = base::Milliseconds(10);
  timing.response_start = base::Seconds(0);
  PopulateRequiredTimingFields(&timing);
  tester()->SimulateTimingUpdate(timing);
  ResourceDataUpdate(
      main_rfh(), ResourceCached::kNotCached, 10 /* resource_size_in_kbyte */,
      "application/javascript" /* mime_type */, false /* is_ad_resource */);
  ResourceDataUpdate(
      main_rfh(), ResourceCached::kNotCached, 10 /* resource_size_in_kbyte */,
      "application/javascript" /* mime_type */, true /* is_ad_resource */);
  ResourceDataUpdate(main_rfh(), ResourceCached::kNotCached,
                     10 /* resource_size_in_kbyte */,
                     "video/webm" /* mime_type */, true /* is_ad_resource */);

  // Update cpu timings.
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(500));
  OnCpuTimingUpdate(main_rfh(), base::Milliseconds(500));
  NavigateMainFrame(kNonAdUrl);

  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdPageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());

  EXPECT_EQ(*test_ukm_recorder().GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kTotalBytesName),
            30);
  EXPECT_EQ(*test_ukm_recorder().GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kAdBytesName),
            20);
  EXPECT_EQ(
      *test_ukm_recorder().GetEntryMetric(
          entries.front(), ukm::builders::AdPageLoad::kAdJavascriptBytesName),
      10);
  EXPECT_EQ(*test_ukm_recorder().GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kAdVideoBytesName),
            10);
  EXPECT_EQ(
      *test_ukm_recorder().GetEntryMetric(
          entries.front(), ukm::builders::AdPageLoad::kMainframeAdBytesName),
      ukm::GetExponentialBucketMinForBytes(20 * 1024));
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kAdCpuTimeName),
            500);
}

TEST_P(AdsPageLoadMetricsObserverTest, ZeroBytesZeroCpuUseFrame_NotRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  CreateAndNavigateSubFrame(kAdUrl, main_frame);

  NavigateFrame(kNonAdUrl, main_frame);

  // We expect frames with no bytes and no CPU usage to be ignored
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("FrameCounts.AdFrames.Total"), 0);
}

TEST_P(AdsPageLoadMetricsObserverTest, ZeroBytesNonZeroCpuFrame_Recorded) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);

  // Use CPU but maintain zero bytes in the ad frame
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(1000));

  NavigateFrame(kNonAdUrl, main_frame);

  // We expect the frame to be recorded as it has non-zero CPU usage
  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 0}},
                 0 /* non_ad_cached_kb */, 10 /* non_ad_uncached_kb */);

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.FullPage.TotalUsage2"), 1000, 1);
}

TEST_P(AdsPageLoadMetricsObserverTest, TestCpuTimingMetricsWindowUnactivated) {
  OverrideWithMockClock();
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add some data to the ad frame so it gets reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Perform some updates on ad and non-ad frames. Usage 1%.
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(500));

  // Advance time by twelve seconds.
  AdvancePageDuration(base::Seconds(12));

  // Do some more work on the ad frame. Usage 5%.
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(1000));

  // Advance time by twelve more seconds.
  AdvancePageDuration(base::Seconds(12));

  // Do some more work on the ad frame. Usage 8%.
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(1000));

  // Advance time by twelve more seconds.
  AdvancePageDuration(base::Seconds(12));

  // Perform some updates on ad and non-ad frames. Usage 10%/13%.
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(1000));
  OnCpuTimingUpdate(main_frame, base::Milliseconds(1000));

  // Advance time by twelve more seconds.
  AdvancePageDuration(base::Seconds(12));

  // Perform some updates on ad and non-ad frames. Usage 8%/11%.
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(500));

  // Navigate away and check the peak windowed cpu usage.
  NavigateFrame(kNonAdUrl, main_frame);

  // 10% is the maximum for the individual ad frame.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.AdFrames.PerFrame.PeakWindowedPercent2"), 10, 1);

  // 13% is the maximum for all frames (including main).
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.FullPage.PeakWindowedPercent2"), 13, 1);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.NonAdFrames.Aggregate.PeakWindowedPercent2"), 3,
      1);
}

TEST_P(AdsPageLoadMetricsObserverTest, AdDensityDistributionMoments) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OverrideWithMockClock();

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  RenderFrameHost* non_ad_frame =
      CreateAndNavigateSubFrame(kNonAdUrlSameOrigin, main_frame);

  page_load_metrics::mojom::FrameMetadata metadata1;
  metadata1.main_frame_intersection_rect = gfx::Rect(0, 0, 1, 100);
  metadata1.main_frame_viewport_rect = gfx::Rect(0, 0, 1, 100);
  tester_->SimulateMetadataUpdate(metadata1, main_frame);

  // Add some ad resource so that ad density metrics are recorded in the end.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     /*resource_size_in_kbyte=*/10,
                     /*mime_type=*/"",
                     /*is_ad_resource=*/true);

  page_load_metrics::mojom::FrameMetadata metadata2;
  metadata2.main_frame_intersection_rect = gfx::Rect(0, 0, 1, 10);
  tester_->SimulateMetadataUpdate(metadata2, ad_frame);
  AdvancePageDuration(base::Seconds(2));

  metadata2.main_frame_intersection_rect = gfx::Rect(0, 0, 1, 80);
  tester_->SimulateMetadataUpdate(metadata2, non_ad_frame);
  AdvancePageDuration(base::Seconds(1));

  metadata2.main_frame_intersection_rect = gfx::Rect(0, 0, 1, 50);
  tester_->SimulateMetadataUpdate(metadata2, ad_frame);
  AdvancePageDuration(base::Seconds(1));

  NavigateFrame(kNonAdUrl, main_frame);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdPageLoadCustomSampling3::kEntryName);
  EXPECT_EQ(1u, entries.size());

  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdPageLoadCustomSampling3::kAverageViewportAdDensityName,
      20);
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdPageLoadCustomSampling3::kVarianceViewportAdDensityName,
      /*ukm::GetExponentialBucketMin(300, 1.3)=*/248);
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdPageLoadCustomSampling3::kSkewnessViewportAdDensityName,
      /*ukm::GetExponentialBucketMin(std::llround(1.1547), 1.3)=*/1);
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdPageLoadCustomSampling3::kKurtosisViewportAdDensityName,
      /*-ukm::GetExponentialBucketMin(-std::llround(-0.666667), 1.3)=*/-1);

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("AverageViewportAdDensity"), 20, 1);
}

TEST_P(AdsPageLoadMetricsObserverTest, AdDensityOnPageWithoutAdBytes) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OverrideWithMockClock();

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);

  // No ad resource so that only AdPageLoadCustomSampling3 is recorded in the
  // end.

  AdvancePageDuration(base::Seconds(1));

  NavigateFrame(kNonAdUrl, main_frame);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdPageLoadCustomSampling3::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdPageLoadCustomSampling3::kAverageViewportAdDensityName,
      0);
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdPageLoadCustomSampling3::kVarianceViewportAdDensityName,
      0);
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdPageLoadCustomSampling3::kSkewnessViewportAdDensityName,
      0);
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdPageLoadCustomSampling3::kKurtosisViewportAdDensityName,
      -3);

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("AverageViewportAdDensity"), 0, 1);
}

TEST_P(AdsPageLoadMetricsObserverTest, TestCpuTimingMetricsWindowedActivated) {
  OverrideWithMockClock();
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add some data to the ad frame so it gets reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Perform some updates on ad and non-ad frames. Usage 1%.
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(500));

  // Advance time by twelve seconds.
  AdvancePageDuration(base::Seconds(12));

  // Do some more work on the ad frame. Usage 8%.
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(2000));

  // Advance time by twelve more seconds.
  AdvancePageDuration(base::Seconds(12));

  // Do some more work on the ad frame. Usage 11%.
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(1000));

  // Set the page activation and advance time by twelve more seconds.
  tester()->SimulateFrameReceivedUserActivation(ad_frame);
  AdvancePageDuration(base::Seconds(12));

  // Perform some updates on ad and main frames. Usage 13%/16%.
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(1000));
  OnCpuTimingUpdate(main_frame, base::Milliseconds(1000));

  // Advance time by twelve more seconds.
  AdvancePageDuration(base::Seconds(12));

  // Perform some updates on ad and non-ad frames. Usage 8%/11%.
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(500));

  // Navigate away and check the peak windowed cpu usage.
  NavigateFrame(kNonAdUrl, main_frame);

  // 11% is the maximum before activation for the ad frame.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.AdFrames.PerFrame.PeakWindowedPercent2"), 11, 1);

  // 16% is the maximum for all frames (including main), ignores activation.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.FullPage.PeakWindowedPercent2"), 16, 1);
}

TEST_P(AdsPageLoadMetricsObserverTest, TestCpuTimingMetricsNoActivation) {
  OverrideWithMockClock();
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* non_ad_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add some data to the ad frame so it gets reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Perform some updates on ad and non-ad frames.
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(500));
  OnCpuTimingUpdate(non_ad_frame, base::Milliseconds(500));

  // Hide the page, and ensure we keep recording information.
  web_contents()->WasHidden();

  // Do some more work on the ad frame.
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(1000));

  // Show the page, nothing should change.
  web_contents()->WasShown();

  // Do some more work on the main frame.
  OnCpuTimingUpdate(main_frame, base::Milliseconds(500));

  // Navigate away after 4 seconds.
  AdvancePageDuration(base::Milliseconds(4000));
  NavigateFrame(kNonAdUrl, main_frame);

  // Check the cpu histograms.
  CheckTotalUsageHistogram("FullPage", 500 + 500 + 1000 + 500);
  CheckTotalUsageHistogram("NonAdFrames.Aggregate", 500 + 500);
  CheckActivatedTotalUsageHistograms(std::nullopt, std::nullopt);
  CheckTotalUsageHistogram("AdFrames.PerFrame", 500 + 1000, "Unactivated");
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.AdFrames.Aggregate.TotalUsage2"), 500 + 1000, 1);

  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kCpuTime_TotalName, 1500);
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kCpuTime_PeakWindowedPercentName,
      100 * 1500 / 30000);
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entries.front(), ukm::builders::AdFrameLoad::kCpuTime_PreActivationName));
}

TEST_P(AdsPageLoadMetricsObserverTest, TestCpuTimingMetricsOnActivation) {
  OverrideWithMockClock();
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* non_ad_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add some data to the ad frame so it get reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Perform some updates on ad and non-ad frames.
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(1000));
  OnCpuTimingUpdate(non_ad_frame, base::Milliseconds(500));

  // Set the frame as activated after 2.5 seconds
  AdvancePageDuration(base::Milliseconds(2500));
  tester()->SimulateFrameReceivedUserActivation(ad_frame);

  // Do some more work on the main frame.
  OnCpuTimingUpdate(main_frame, base::Milliseconds(500));

  // Do some more work on the ad frame.
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(500));

  // Navigate away after 4 seconds.
  AdvancePageDuration(base::Milliseconds(1500));
  NavigateFrame(kNonAdUrl, main_frame);

  // Check the cpu histograms.
  CheckTotalUsageHistogram("FullPage", 500 + 500 + 1000 + 500);
  CheckTotalUsageHistogram("NonAdFrames.Aggregate", 500 + 500);
  CheckTotalUsageHistogram("AdFrames.PerFrame", std::nullopt, "Unactivated");
  CheckActivatedTotalUsageHistograms(500 + 500, 500);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.AdFrames.Aggregate.TotalUsage2"), 1000 + 500, 1);

  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kCpuTime_TotalName, 1500);
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kCpuTime_PeakWindowedPercentName,
      100 * 1000 / 30000);
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kCpuTime_PreActivationName,
      1000);
}

// Tests that creative origin status is computed as intended, i.e. as the origin
// status of the frame in the ad frame tree that has its first contentful paint
// occur first.
TEST_P(AdsPageLoadMetricsObserverTest, CreativeOriginStatus) {
  using OriginStatus = OriginStatus;

  // Each CreativeOriginTest struct lists the urls of the frames in the frame
  // tree, from main frame to leaf ad frame, along with the index of the ad
  // creative and the expected creative origin status.
  std::vector<CreativeOriginTest> test_cases = {
      {{"http://a.com", "http://a.com/disallowed.html"},
       1 /* creative_index */,
       OriginStatus::kSame},
      {{"http://a.com", "http://b.com/disallowed.html"},
       1 /* creative_index */,
       OriginStatus::kCross},
      {{"http://a.com", "http://a.com/disallowed.html", "http://b.com"},
       1 /* creative_index */,
       OriginStatus::kSame},
      {{"http://a.com", "http://a.com/disallowed.html", "http://b.com"},
       2 /* creative_index */,
       OriginStatus::kCross},
      {{"http://a.com", "http://b.com/disallowed.html", "http://a.com"},
       1 /* creative_index */,
       OriginStatus::kCross},
      {{"http://a.com", "http://b.com/disallowed.html", "http://a.com"},
       2 /* creative_index */,
       OriginStatus::kSame},
      {{"http://a.com", "http://b.com/disallowed.html", "http://a.com",
        "http://b.com"},
       1 /* creative_index */,
       OriginStatus::kCross},
      {{"http://a.com", "http://b.com/disallowed.html", "http://a.com",
        "http://b.com"},
       2 /* creative_index */,
       OriginStatus::kSame},
      {{"http://a.com", "http://b.com/disallowed.html", "http://a.com",
        "http://b.com"},
       3 /* creative_index */,
       OriginStatus::kCross},
  };

  for (const auto& creative_origin_test : test_cases) {
    TestCreativeOriginStatus(creative_origin_test);
  }
}

// Tests that creative origin status with throttling is computed as intended,
// i.e. as the origin status of the frame in the ad frame tree that has its
// first contentful paint occur first, with throttling status determined by
// whether or not at least one frame in the ad frame tree was unthrottled.
TEST_P(AdsPageLoadMetricsObserverTest, CreativeOriginStatusWithThrottling) {
  using OriginStatusWithThrottling = OriginStatusWithThrottling;

  // Each CreativeOriginTestWithThrottling struct lists the urls of the frames
  // in the frame tree, from main frame to leaf ad frame, and a corresponding
  // bool for each to denote whether that frame is throttled, along with the
  // index of the ad creative and the expected creative origin status with
  // throttling.
  std::vector<CreativeOriginTestWithThrottling> test_cases = {
      {"http://a.com",
       {"http://a.com/disallowed.html"},
       {false} /* throttled */,
       0 /* creative_index */,
       true /* should_paint */,
       OriginStatusWithThrottling::kSameAndUnthrottled},
      {"http://a.com",
       {"http://b.com/disallowed.html"},
       {false} /* throttled */,
       0 /* creative_index */,
       true /* should_paint */,
       OriginStatusWithThrottling::kCrossAndUnthrottled},
      {"http://a.com",
       {"http://a.com/disallowed.html"},
       {true} /* throttled */,
       0 /* creative_index */,
       true /* should_paint */,
       OriginStatusWithThrottling::kUnknownAndThrottled},
      {"http://a.com",
       {"http://b.com/disallowed.html"},
       {true} /* throttled */,
       0 /* creative_index */,
       true /* should_paint */,
       OriginStatusWithThrottling::kUnknownAndThrottled},
      {"http://a.com",
       {"http://a.com/disallowed.html", "http://b.com"},
       {false, false} /* throttled */,
       0 /* creative_index */,
       true /* should_paint */,
       OriginStatusWithThrottling::kSameAndUnthrottled},
      {"http://a.com",
       {"http://a.com/disallowed.html", "http://b.com"},
       {false, false} /* throttled */,
       1 /* creative_index */,
       true /* should_paint */,
       OriginStatusWithThrottling::kCrossAndUnthrottled},
      {"http://a.com",
       {"http://b.com/disallowed.html", "http://a.com"},
       {true, true} /* throttled */,
       0 /* creative_index */,
       true /* should_paint */,
       OriginStatusWithThrottling::kUnknownAndThrottled},
      {"http://a.com",
       {"http://b.com/disallowed.html", "http://a.com"},
       {true, true} /* throttled */,
       1 /* creative_index */,
       true /* should_paint */,
       OriginStatusWithThrottling::kUnknownAndThrottled},
      {"http://a.com",
       {"http://b.com/disallowed.html", "http://a.com"},
       {true, true} /* throttled */,
       0 /* creative_index */,
       true /* should_paint */,
       OriginStatusWithThrottling::kUnknownAndThrottled},
      {"http://a.com",
       {"http://a.com/disallowed.html", "http://b.com"},
       {false, true} /* throttled */,
       0 /* creative_index */,
       true /* should_paint */,
       OriginStatusWithThrottling::kSameAndUnthrottled},
      {"http://a.com",
       {"http://b.com/disallowed.html", "http://a.com"},
       {false, false} /* throttled */,
       0 /* creative_index */,
       true /* should_paint */,
       OriginStatusWithThrottling::kCrossAndUnthrottled},
      {"http://a.com",
       {"http://a.com/disallowed.html", "http://b.com"},
       {false, false} /* throttled */,
       0 /* creative_index */,
       false /* should_paint */,
       OriginStatusWithThrottling::kUnknownAndUnthrottled},
      {"http://a.com",
       {"http://a.com/disallowed.html", "http://b.com"},
       {false, true} /* throttled */,
       0 /* creative_index */,
       false /* should_paint */,
       OriginStatusWithThrottling::kUnknownAndThrottled},
      {"http://a.com",
       {"http://b.com/disallowed.html", "http://b.com"},
       {true, true} /* throttled */,
       0 /* creative_index */,
       false /* should_paint */,
       OriginStatusWithThrottling::kUnknownAndThrottled},
      {"http://a.com",
       {"http://a.com/disallowed.html", "http://b.com"},
       {false, true} /* throttled */,
       1 /* creative_index */,
       false /* should_paint */,
       OriginStatusWithThrottling::kUnknownAndUnthrottled},
      {"http://a.com",
       {"http://a.com/disallowed.html", "http://b.com"},
       {true, true} /* throttled */,
       1 /* creative_index */,
       false /* should_paint */,
       OriginStatusWithThrottling::kUnknownAndThrottled},
      {"http://a.com",
       {"http://a.com/disallowed.html", "http://b.com"},
       {true, false} /* throttled */,
       1 /* creative_index */,
       false /* should_paint */,
       OriginStatusWithThrottling::kUnknownAndThrottled},
      {"http://a.com",
       {"http://b.com/disallowed.html", "http://b.com"},
       {true, false} /* throttled */,
       1 /* creative_index */,
       false /* should_paint */,
       OriginStatusWithThrottling::kUnknownAndThrottled},
      {"http://a.com",
       {"http://b.com/disallowed.html", "http://a.com"},
       {true, false} /* throttled */,
       1 /* creative_index */,
       true /* should_paint */,
       OriginStatusWithThrottling::kSameAndUnthrottled},
      {"http://a.com",
       {"http://a.com/disallowed.html", "http://b.com"},
       {true, false} /* throttled */,
       1 /* creative_index */,
       true /* should_paint */,
       OriginStatusWithThrottling::kCrossAndUnthrottled}};

  for (const auto& creative_origin_test : test_cases) {
    TestCreativeOriginStatusWithThrottling(creative_origin_test);
  }
}

// Tests that even when the intervention is not enabled, we still record the
// computed heavy ad types for ad frames
TEST_P(AdsPageLoadMetricsObserverTest, HeavyAdFeatureOff_UMARecorded) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {heavy_ad_intervention::features::kHeavyAdIntervention,
           heavy_ad_intervention::features::kHeavyAdInterventionWarning});
  OverrideWithMockClock();

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame_none =
      CreateAndNavigateSubFrame(kAdUrl, main_frame);
  RenderFrameHost* ad_frame_net = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  content::RenderFrameHostTester* rfh_tester_net =
      content::RenderFrameHostTester::For(ad_frame_net);
  RenderFrameHost* ad_frame_cpu = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  content::RenderFrameHostTester* rfh_tester_cpu =
      content::RenderFrameHostTester::For(ad_frame_cpu);
  RenderFrameHost* ad_frame_total_cpu =
      CreateAndNavigateSubFrame(kAdUrl, main_frame);
  content::RenderFrameHostTester* rfh_tester_total_cpu =
      content::RenderFrameHostTester::For(ad_frame_total_cpu);

  // Load some bytes in each frame so they are considered ad iframes.
  ResourceDataUpdate(ad_frame_none, ResourceCached::kNotCached, 1);
  ResourceDataUpdate(ad_frame_net, ResourceCached::kNotCached, 1);
  ResourceDataUpdate(ad_frame_cpu, ResourceCached::kNotCached, 1);
  ResourceDataUpdate(ad_frame_total_cpu, ResourceCached::kNotCached, 1);

  // Make three of the ad frames hit thresholds for heavy ads.
  ResourceDataUpdate(ad_frame_net, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024));
  OnCpuTimingUpdate(
      ad_frame_cpu,
      base::Milliseconds(heavy_ad_thresholds::kMaxPeakWindowedPercent * 30000 /
                         100));
  UseCpuTimeUnderThreshold(
      ad_frame_total_cpu, base::Milliseconds(heavy_ad_thresholds::kMaxCpuTime));

  // Check the intervention issues
  EXPECT_EQ(rfh_tester_net->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kAll),
            0);
  EXPECT_EQ(rfh_tester_cpu->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kAll),
            0);
  EXPECT_EQ(rfh_tester_total_cpu->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kAll),
            0);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  using HeavyAdStatus = HeavyAdStatus;
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"), 4);
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"),
      HeavyAdStatus::kNone, 1);
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"),
      HeavyAdStatus::kNetwork, 1);
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"),
      HeavyAdStatus::kPeakCpu, 1);
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"),
      HeavyAdStatus::kTotalCpu, 1);

  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.InterventionType2"), 0);

  // Histogram is not logged when no frames are unloaded.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.NetworkBytesAtFrameUnload"), 0);
}

TEST_P(AdsPageLoadMetricsObserverTest, HeavyAdNetworkUsage_InterventionFired) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      heavy_ad_intervention::features::kHeavyAdIntervention);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(ad_frame);

  // Load just under the threshold amount of bytes.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) - 1);

  // Verify we did not trigger the intervention.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));

  // Verify that prior to an intervention is triggered we do not log
  // NetworkBytesAtFrameUnload.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.NetworkBytesAtFrameUnload"), 0);

  ErrorPageWaiter waiter(web_contents());

  // Load enough bytes to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 2);

  const char kInterventionMessage[] =
      "Ad was removed because its network usage exceeded the limit. "
      "See "
      "https://www.chromestatus.com/feature/"
      "4800491902992384?utm_source=devtools";
  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));
  EXPECT_EQ(kInterventionMessage, PopLastInterventionReportMessage());

  waiter.WaitForError();
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.InterventionType2"), HeavyAdStatus::kNetwork,
      1);
  EXPECT_EQ(rfh_tester->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kNetworkTotal),
            1);
  EXPECT_EQ(rfh_tester->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kAll),
            1);

  // Verify that unloading a heavy ad due to network usage logs the network
  // bytes to UMA.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.NetworkBytesAtFrameUnload"),
      heavy_ad_thresholds::kMaxNetworkBytes / 1024, 1);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.NetworkBytesAtFrameUnload"), 1);
}

// Test that when the page is hidden and the app enters the background, that we
// record histograms, but continue to monitor for CPU heavy ad interventions.
TEST_P(AdsPageLoadMetricsObserverTest, HeavyAdCpuInterventionInBackground) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      heavy_ad_intervention::features::kHeavyAdIntervention);
  OverrideWithMockClock();

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add some data to the ad frame so it get reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 1);

  // Use just under the peak threshold amount of CPU.
  OnCpuTimingUpdate(
      ad_frame,
      base::Milliseconds(
          heavy_ad_thresholds::kMaxPeakWindowedPercent * 30000 / 100 - 1));

  // Verify we did not trigger the intervention.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));

  // Verify no reporting happened prior to backgrounding.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Bytes.FullPage.Total2"), 0);

  // Background the page.
  tester()->SimulateAppEnterBackground();

  // Verify reporting happened.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Bytes.FullPage.Total2"), 1);

  // Use enough CPU to trigger the intervention.
  ErrorPageWaiter waiter(web_contents());
  AdvancePageDuration(base::Seconds(10));
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(1));

  // Wait for an error page and then check there's an intervention on the frame.
  waiter.WaitForError();
  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));

  // Navigate away to trigger histograms. Check they didn't fire again.
  NavigateFrame(kNonAdUrl, main_frame);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Bytes.FullPage.Total2"), 1);
}

// Test that when the page is hidden and the app enters the background, that we
// record histograms, but continue to monitor for network heavy ad
// interventions.
TEST_P(AdsPageLoadMetricsObserverTest,
       HeavyAdNetworkInterventionInBackgrounded) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      heavy_ad_intervention::features::kHeavyAdIntervention);
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Load just under the threshold amount of bytes.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) - 1);

  // Verify we did not trigger the intervention.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));

  // Verify that prior to an intervention is triggered we do not log
  // NetworkBytesAtFrameUnload.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.NetworkBytesAtFrameUnload"), 0);

  // Verify no reporting happened prior to backgrounding.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Cpu.FullPage.TotalUsage2"), 0);

  // Background the page.
  tester()->SimulateAppEnterBackground();

  // Verify reporting happened.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Cpu.FullPage.TotalUsage2"), 1);

  // Load enough bytes to trigger the intervention.
  ErrorPageWaiter waiter(web_contents());
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 2);

  // Wait for an error page and then check there's an intervention on the frame.
  waiter.WaitForError();
  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));

  // Navigate away to trigger histograms. Check they didn't fire again.
  NavigateFrame(kNonAdUrl, main_frame);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Cpu.FullPage.TotalUsage2"), 1);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       HeavyAdNetworkUsageWithNoise_InterventionFired) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      heavy_ad_intervention::features::kHeavyAdIntervention);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);

  OverrideHeavyAdNoiseProvider(
      std::make_unique<MockNoiseProvider>(2048 /* network noise */));
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(ad_frame);

  // Load just under the threshold amount of bytes with noise included.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.InterventionType2"), 0);
  EXPECT_EQ(rfh_tester->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kAll),
            0);

  // Histogram is not logged before the intervention is fired.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.NetworkBytesAtFrameUnload"), 0);

  ErrorPageWaiter waiter(web_contents());

  // Load enough bytes to meet the noised threshold criteria.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 1);

  waiter.WaitForError();
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.InterventionType2"), HeavyAdStatus::kNetwork,
      1);
  EXPECT_EQ(rfh_tester->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kNetworkTotal),
            1);
  EXPECT_EQ(rfh_tester->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kAll),
            1);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.DisallowedByBlocklist"), false, 1);

  // Verify that unloading a heavy ad due to network usage logs the bytes to
  // UMA.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.NetworkBytesAtFrameUnload"),
      heavy_ad_thresholds::kMaxNetworkBytes / 1024, 1);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.NetworkBytesAtFrameUnload"), 1);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       HeavyAdNetworkUsageLessThanNoisedThreshold_NotFired) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      heavy_ad_intervention::features::kHeavyAdIntervention);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);

  OverrideHeavyAdNoiseProvider(
      std::make_unique<MockNoiseProvider>(2048 /* network noise */));
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Load network bytes that trip the heavy ad threshold without noise.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     heavy_ad_thresholds::kMaxNetworkBytes / 1024 + 1);

  // Verify we did not trigger the intervention.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"),
      HeavyAdStatus::kNone, 1);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       HeavyAdNetworkUsageLessThanNoisedThreshold_CpuTriggers) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      heavy_ad_intervention::features::kHeavyAdIntervention);
  OverrideWithMockClock();

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);

  OverrideHeavyAdNoiseProvider(
      std::make_unique<MockNoiseProvider>(2048 /* network noise */));
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(ad_frame);

  // Load network bytes that trip the heavy ad threshold without noise.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     heavy_ad_thresholds::kMaxNetworkBytes / 1024 + 1);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.InterventionType2"), 0);
  EXPECT_EQ(rfh_tester->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kAll),
            0);

  // Verify the frame can still trip the CPU threshold.
  UseCpuTimeUnderThreshold(
      ad_frame, base::Milliseconds(heavy_ad_thresholds::kMaxCpuTime + 1));

  // Verify we did trigger the intervention and that the message matches the
  // intervention type with noise.
  const char kReportOnlyMessage[] =
      "Ad was removed because its "
      "total CPU usage exceeded the limit. "
      "See "
      "https://www.chromestatus.com/feature/"
      "4800491902992384?utm_source=devtools";
  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.InterventionType2"), HeavyAdStatus::kTotalCpu,
      1);
  EXPECT_EQ(kReportOnlyMessage, PopLastInterventionReportMessage());
  EXPECT_EQ(rfh_tester->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kCpuTotal),
            1);
  EXPECT_EQ(rfh_tester->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kAll),
            1);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"),
      HeavyAdStatus::kTotalCpu, 1);
}

TEST_P(AdsPageLoadMetricsObserverTest, HeavyAdTotalCpuUsage_InterventionFired) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      heavy_ad_intervention::features::kHeavyAdIntervention);
  OverrideWithMockClock();

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(ad_frame);

  // Add some data to the ad frame so it get reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 1);

  // Use just under the threshold amount of CPU.Needs to spread across enough
  // windows to not trigger peak threshold.
  AdvancePageDuration(base::Seconds(30));
  UseCpuTimeUnderThreshold(
      ad_frame, base::Milliseconds(heavy_ad_thresholds::kMaxCpuTime - 1));

  // Verify we did not trigger the intervention.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));

  AdvancePageDuration(base::Seconds(30));

  // Use enough CPU to trigger the intervention.
  ErrorPageWaiter waiter(web_contents());
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(1));

  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));
  waiter.WaitForError();
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.InterventionType2"), HeavyAdStatus::kTotalCpu,
      1);
  EXPECT_EQ(rfh_tester->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kCpuTotal),
            1);
  EXPECT_EQ(rfh_tester->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kAll),
            1);
}

TEST_P(AdsPageLoadMetricsObserverTest, HeavyAdPeakCpuUsage_InterventionFired) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      heavy_ad_intervention::features::kHeavyAdIntervention);
  OverrideWithMockClock();

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(ad_frame);

  // Add some data to the ad frame so it get reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 1);

  // Use just under the peak threshold amount of CPU.
  OnCpuTimingUpdate(
      ad_frame,
      base::Milliseconds(
          heavy_ad_thresholds::kMaxPeakWindowedPercent * 30000 / 100 - 1));

  // Verify we did not trigger the intervention.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));

  // Use enough CPU to trigger the intervention.
  ErrorPageWaiter waiter(web_contents());
  AdvancePageDuration(base::Seconds(10));
  OnCpuTimingUpdate(ad_frame, base::Milliseconds(1));

  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));
  waiter.WaitForError();
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.InterventionType2"), HeavyAdStatus::kPeakCpu,
      1);
  EXPECT_EQ(rfh_tester->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kCpuPeak),
            1);
  EXPECT_EQ(rfh_tester->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kAll),
            1);

  // Verify we do not record UMA specific to network byte interventions when
  // the intervention triggers for CPU.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.NetworkBytesAtFrameUnload"), 0);
}

TEST_P(AdsPageLoadMetricsObserverTest, HeavyAdFeatureDisabled_NotFired) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {heavy_ad_intervention::features::kHeavyAdIntervention,
           heavy_ad_intervention::features::kHeavyAdInterventionWarning});

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add enough data to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (kMaxHeavyAdNetworkBytes / 1024) + 1);

  // Verify we did not trigger the intervention.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));

  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.DisallowedByBlocklist"), 0);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       HeavyAdWithUserGesture_NotConsideredHeavy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      heavy_ad_intervention::features::kHeavyAdIntervention);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Give the frame a user activation before the threshold would be hit.
  tester()->SimulateFrameReceivedUserActivation(ad_frame);

  // Add enough data to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"),
      HeavyAdStatus::kNone, 1);
}

// Tests that each configurable unload policy allows the intervention to trigger
// on the correct frames.
TEST_P(AdsPageLoadMetricsObserverTest, HeavyAdPolicyProvided) {
  struct {
    // |policy| maps to a HeavyAdUnloadPolicy.
    std::string policy;
    bool exceed_network;
    bool exceed_cpu;
    bool intervention_expected;
  } kTestCases[] = {
      {"0" /* policy */, false /* exceed_network */, false /* exceed_cpu */,
       false /* intervention_expected */},
      {"0" /* policy */, true /* exceed_network */, false /* exceed_cpu */,
       true /* intervention_expected */},
      {"0" /* policy */, false /* exceed_network */, true /* exceed_cpu */,
       false /* intervention_expected */},
      {"0" /* policy */, true /* exceed_network */, true /* exceed_cpu */,
       true /* intervention_expected */},
      {"1" /* policy */, false /* exceed_network */, false /* exceed_cpu */,
       false /* intervention_expected */},
      {"1" /* policy */, true /* exceed_network */, false /* exceed_cpu */,
       false /* intervention_expected */},
      {"1" /* policy */, false /* exceed_network */, true /* exceed_cpu */,
       true /* intervention_expected */},
      {"1" /* policy */, true /* exceed_network */, true /* exceed_cpu */,
       true /* intervention_expected */},
      {"2" /* policy */, false /* exceed_network */, false /* exceed_cpu */,
       false /* intervention_expected */},
      {"2" /* policy */, true /* exceed_network */, false /* exceed_cpu */,
       true /* intervention_expected */},
      {"2" /* policy */, false /* exceed_network */, true /* exceed_cpu */,
       true /* intervention_expected */},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(base::StringPrintf(
        "policy: %s, exceed_network: %d, exceed_cpu: %d,  "
        "intervention_expected: %d",
        test_case.policy.c_str(), test_case.exceed_network,
        test_case.exceed_cpu, test_case.intervention_expected));
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        heavy_ad_intervention::features::kHeavyAdIntervention,
        {{"kUnloadPolicy", test_case.policy}});
    RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
    RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);
    // Clear out any pending messages.
    HasInterventionReportsAfterFlush(ad_frame);

    ErrorPageWaiter waiter(web_contents());
    if (test_case.exceed_network) {
      ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                         (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);
    }
    if (test_case.exceed_cpu) {
      OnCpuTimingUpdate(
          ad_frame, base::Milliseconds(heavy_ad_thresholds::kMaxCpuTime + 1));
    }

    // We should either see an error page if the intervention happened, or not
    // see any reports.
    if (test_case.intervention_expected) {
      waiter.WaitForError();
    } else {
      EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));
    }

    blocklist()->ClearBlockList(base::Time::Min(), base::Time::Max());
  }
}

// Verifies when a user reloads a page with a heavy ad we log it to metrics.
TEST_P(AdsPageLoadMetricsObserverTest, HeavyAdPageReload_MetricsRecorded) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      heavy_ad_intervention::features::kHeavyAdIntervention);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add enough data to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  // Reload the page.
  NavigationSimulator::Reload(web_contents());

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"),
      HeavyAdStatus::kNetwork, 1);
}

// Verifies when a user reloads a page we do not trigger the heavy ad
// intevention.
TEST_P(AdsPageLoadMetricsObserverTest, HeavyAdPageReload_InterventionIgnored) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      heavy_ad_intervention::features::kHeavyAdIntervention);

  NavigateMainFrame(kNonAdUrl);
  // Reload the page.
  NavigationSimulator::Reload(web_contents());

  RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add enough data to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  // Verify we did not trigger the intervention.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));
}

TEST_P(AdsPageLoadMetricsObserverTest,
       HeavyAdPageReloadPrivacyMitigationsDisabled_InterventionAllowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {heavy_ad_intervention::features::kHeavyAdIntervention},
      {heavy_ad_intervention::features::kHeavyAdPrivacyMitigations});

  NavigateMainFrame(kNonAdUrl);
  // Reload the page.
  NavigationSimulator::Reload(web_contents());

  RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add enough data to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  // Verify we trigger the intervention.
  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));

  // The histogram should not be recorded when the reload logic is ignored by
  // the privacy mitigations flag.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.IgnoredByReload"), 0);
}

TEST_P(AdsPageLoadMetricsObserverTest, HeavyAdBlocklistFull_NotFired) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      heavy_ad_intervention::features::kHeavyAdIntervention);

  // Five interventions are allowed to occur, per origin per day. Add five
  // entries to the blocklist.
  for (int i = 0; i < 5; i++)
    blocklist()->AddEntry(GURL(kNonAdUrl).host(), true, 0);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add enough data to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  // Verify we did not trigger the intervention.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));
}

TEST_P(AdsPageLoadMetricsObserverTest,
       HeavyAdBlocklistDisabled_InterventionNotBlocked) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {heavy_ad_intervention::features::kHeavyAdIntervention},
      {heavy_ad_intervention::features::kHeavyAdPrivacyMitigations});

  // Fill up the blocklist to verify the blocklist logic is correctly ignored
  // when disabled.
  for (int i = 0; i < 5; i++)
    blocklist()->AddEntry(GURL(kNonAdUrl).host(), true, 0);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(ad_frame);

  // Add enough data to trigger the intervention.
  ErrorPageWaiter waiter(web_contents());
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));
  waiter.WaitForError();
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.InterventionType2"), HeavyAdStatus::kNetwork,
      1);
  EXPECT_EQ(rfh_tester->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kNetworkTotal),
            1);
  EXPECT_EQ(rfh_tester->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kAll),
            1);

  // This histogram should not be recorded when the blocklist is disabled.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.DisallowedByBlocklist"), 0);
}

TEST_P(AdsPageLoadMetricsObserverTest, HeavyAdBlocklist_InterventionReported) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      heavy_ad_intervention::features::kHeavyAdIntervention);

  // Five interventions are allowed to occur, per origin per day. Add four
  // entries to the blocklist.
  for (int i = 0; i < 4; i++)
    blocklist()->AddEntry(GURL(kNonAdUrl).host(), true, 0);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add enough data to trigger the intervention.
  ErrorPageWaiter waiter(web_contents());
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  // Verify the intervention triggered.
  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));
  waiter.WaitForError();
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.InterventionType2"), HeavyAdStatus::kNetwork,
      1);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.DisallowedByBlocklist"), false, 1);

  // Verify the blocklist blocks the next intervention.
  ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add enough data to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  // Verify the intervention did not occur again.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram("HeavyAds.DisallowedByBlocklist"), true, 1);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       HeavyAdReportingOnly_ReportSentNoUnload) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {heavy_ad_intervention::features::kHeavyAdInterventionWarning},
      {heavy_ad_intervention::features::kHeavyAdIntervention});

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(ad_frame);

  ErrorPageWaiter waiter(web_contents());

  // Load enough bytes to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  const char kReportOnlyMessage[] =
      "A future version of Chrome may remove this ad because its network "
      "usage exceeded the limit. "
      "See "
      "https://www.chromestatus.com/feature/"
      "4800491902992384?utm_source=devtools";

  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));
  EXPECT_EQ(kReportOnlyMessage, PopLastInterventionReportMessage());

  // It is not ideal to check the last loaded page here as it requires relying
  // on mojo timings after flushing the interface above. But the ordering is
  // deterministic as intervention reports and navigation use the same mojo
  // pipe.
  EXPECT_FALSE(waiter.LastPageWasErrorPage());
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.InterventionType2"), HeavyAdStatus::kNetwork,
      1);
  EXPECT_EQ(rfh_tester->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kNetworkTotal),
            1);
  EXPECT_EQ(rfh_tester->GetHeavyAdIssueCount(
                RenderFrameHostTester::HeavyAdIssueType::kAll),
            1);
}

TEST_P(AdsPageLoadMetricsObserverTest, NoFirstContentfulPaint_NotRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Load some bytes so that the frame is recorded.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 100);

  // Navigate away and check the histogram.
  NavigateFrame(kNonAdUrl, main_frame);

  histogram_tester().ExpectTotalCount(
      "AdPaintTiming.NavigationToFirstContentfulPaint3", 0);
  histogram_tester().ExpectTotalCount(
      "AdPaintTiming.TopFrameNavigationToFirstAdFirstContentfulPaint", 0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming.TopFrameNavigationToFirstContentfulPaint"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterDeviceAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningDeviceAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningDeviceAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterServerAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningServerAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterNoAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfter"
                        "ServerAndDeviceAuctions"),
      0);
}

TEST_P(AdsPageLoadMetricsObserverTest, FirstContentfulPaint_Recorded) {
  base::TimeTicks start = base::TimeTicks::Now();
  RenderFrameHost* main_frame =
      NavigateFrame(kNonAdUrl, web_contents()->GetPrimaryMainFrame(), start);

  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(
      kAdUrl, main_frame, start + base::Milliseconds(100));

  // Load some bytes so that the frame is recorded.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 100);

  // Set FirstContentfulPaint.
  SimulateFirstContentfulPaint(ad_frame, base::Milliseconds(100));

  // Navigate away and check the histogram.
  NavigateFrame(kNonAdUrl, main_frame);

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("AdPaintTiming.NavigationToFirstContentfulPaint3"), 100,
      1);

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram(
          "AdPaintTiming.TopFrameNavigationToFirstAdFirstContentfulPaint"),
      200, 1);

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram(
          "AdPaintTiming.TopFrameNavigationToFirstContentfulPaint"),
      200, 1);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterDeviceAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningDeviceAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterServerAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningServerAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterNoAuction"),
      1);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfter"
                        "ServerAndDeviceAuctions"),
      0);

  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kTiming_FirstContentfulPaintName, 100);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       FirstContentfulPaintPostDeviceAuctionNoWinner_Recorded) {
  SimulateFCPPostAuctions(
      {CompleteAuctionResult(/*is_server_auction=*/false,
                             /*is_on_device_auction=*/true,
                             content::AuctionResult::kNoInterestGroups)});
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterDeviceAuction"),
      1);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningDeviceAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterServerAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningServerAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterNoAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfter"
                        "ServerAndDeviceAuctions"),
      0);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       FirstContentfulPaintPostDeviceAuctionWinner_Recorded) {
  SimulateFCPPostAuctions({CompleteAuctionResult(
      /*is_server_auction=*/false,
      /*is_on_device_auction=*/true, content::AuctionResult::kSuccess)});
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterDeviceAuction"),
      1);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningDeviceAuction"),
      1);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterServerAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningServerAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterNoAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfter"
                        "ServerAndDeviceAuctions"),
      0);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       FirstContentfulPaintPostServerAuctionNoWinner_Recorded) {
  SimulateFCPPostAuctions({CompleteAuctionResult(
      /*is_server_auction=*/true,
      /*is_on_device_auction=*/false, content::AuctionResult::kNoBids)});

  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterDeviceAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningDeviceAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterServerAuction"),
      1);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningServerAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterNoAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfter"
                        "ServerAndDeviceAuctions"),
      0);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       FirstContentfulPaintPostServerAuctionWinner_Recorded) {
  SimulateFCPPostAuctions({CompleteAuctionResult(
      /*is_server_auction=*/true,
      /*is_on_device_auction=*/false, content::AuctionResult::kSuccess)});

  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterDeviceAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningDeviceAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterServerAuction"),
      1);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningServerAuction"),
      1);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterNoAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfter"
                        "ServerAndDeviceAuctions"),
      0);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       FirstContentfulPaintPostServerAuctionsWithWinnerAndNoWinner_Recorded) {
  SimulateFCPPostAuctions(
      {CompleteAuctionResult(
           /*is_server_auction=*/true,
           /*is_on_device_auction=*/false, content::AuctionResult::kSuccess),
       CompleteAuctionResult(
           /*is_server_auction=*/true,
           /*is_on_device_auction=*/false, content::AuctionResult::kNoBids)});

  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterDeviceAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningDeviceAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterServerAuction"),
      1);
  // Only record
  // TopFrameNavigationToFirstAdFirstContentfulPaintAfterWinningServerAuction
  // if *only* winning auctions happened.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningServerAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterNoAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfter"
                        "ServerAndDeviceAuctions"),
      0);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       FirstContentfulPaintPostAbortedOnDeviceAuction_NotRecorded) {
  SimulateFCPPostAuctions({CompleteAuctionResult(
      /*is_server_auction=*/false,
      /*is_on_device_auction=*/true, content::AuctionResult::kAborted)});

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterDeviceAuction"),
      200, 0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterServerAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningServerAuction"),
      0);
  // No auction *completed* so the following metric is still recorded.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterNoAuction"),
      1);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfter"
                        "ServerAndDeviceAuctions"),
      0);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       FirstContentfulPaintPostServerAndDeviceAuction_Recorded) {
  SimulateFCPPostAuctions(
      {CompleteAuctionResult(/*is_server_auction=*/true,
                             /*is_on_device_auction=*/true,
                             content::AuctionResult::kNoInterestGroups)});

  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterDeviceAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningDeviceAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterServerAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningServerAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterNoAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfter"
                        "ServerAndDeviceAuctions"),
      1);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       FirstContentfulPaintOneServerAuctionAndOneDeviceAuction_Recorded) {
  SimulateFCPPostAuctions(
      {CompleteAuctionResult(/*is_server_auction=*/true,
                             /*is_on_device_auction=*/false,
                             content::AuctionResult::kNoInterestGroups),
       CompleteAuctionResult(/*is_server_auction=*/false,
                             /*is_on_device_auction=*/true,
                             content::AuctionResult::kSuccess)});

  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterDeviceAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningDeviceAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterServerAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWi"
                        "nningServerAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterNoAuction"),
      0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming."
                        "TopFrameNavigationToFirstAdFirstContentfulPaintAfter"
                        "ServerAndDeviceAuctions"),
      1);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       MultipleFirstContentfulPaintsInAdWithInOrderIPCs_EarliestUsed) {
  base::TimeTicks start = base::TimeTicks::Now();

  RenderFrameHost* main_frame =
      NavigateFrame(kNonAdUrl, web_contents()->GetPrimaryMainFrame(), start);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(
      kAdUrl, main_frame, start + base::Milliseconds(100));
  RenderFrameHost* sub_frame = CreateAndNavigateSubFrame(
      kAdUrl, ad_frame, start + base::Milliseconds(200));

  // Load some bytes so that the frame is recorded.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 100);

  // Set FirstContentfulPaint for nested subframe. Assume that it paints first.
  SimulateFirstContentfulPaint(sub_frame, base::Milliseconds(90));

  // Set FirstContentfulPaint for root ad frame.
  SimulateFirstContentfulPaint(ad_frame, base::Milliseconds(300));

  // Navigate away and check the histogram.
  NavigateFrame(kNonAdUrl, main_frame);

  // The histogram value should be that of the earliest FCP recorded.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("AdPaintTiming.NavigationToFirstContentfulPaint3"), 90,
      1);

  // The subframe's navigation started at time 200, and the FCP was 90, so 290
  // is expected.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram(
          "AdPaintTiming.TopFrameNavigationToFirstAdFirstContentfulPaint"),
      290, 1);

  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kTiming_FirstContentfulPaintName, 90);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       MultipleFirstContentfulPaintsInAdWithOutOfOrderIPCs_EarliestUsed) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  RenderFrameHost* sub_frame = CreateAndNavigateSubFrame(kAdUrl, ad_frame);

  // Load some bytes so that the frame is recorded.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 100);

  // Set FirstContentfulPaint for root ad frame.
  SimulateFirstContentfulPaint(ad_frame, base::Milliseconds(100));

  // Set FirstContentfulPaint for inner subframe. Simulate the nested
  // frame painting first but having its IPCs received second.
  SimulateFirstContentfulPaint(sub_frame, base::Milliseconds(90));

  // Navigate away and check the histogram.
  NavigateFrame(kNonAdUrl, main_frame);

  // The histogram value should be that of the earliest FCP recorded.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("AdPaintTiming.NavigationToFirstContentfulPaint3"), 90,
      1);

  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kTiming_FirstContentfulPaintName, 90);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       MultipleAdFramesWithFirstContentfulPaint) {
  base::TimeTicks start = base::TimeTicks::Now();

  RenderFrameHost* main_frame =
      NavigateFrame(kNonAdUrl, web_contents()->GetPrimaryMainFrame(), start);

  // Each ad frame loads 100ms after the previous load.
  RenderFrameHost* ad_frame_at_100ms = CreateAndNavigateSubFrame(
      kAdUrl, main_frame, start + base::Milliseconds(100));
  RenderFrameHost* ad_frame_at_200ms = CreateAndNavigateSubFrame(
      kAdUrl, main_frame, start + base::Milliseconds(200));
  RenderFrameHost* ad_frame_at_300ms = CreateAndNavigateSubFrame(
      kAdUrl, main_frame, start + base::Milliseconds(300));

  // Load some bytes so that the frame is recorded.
  ResourceDataUpdate(ad_frame_at_100ms, ResourceCached::kNotCached, 100);
  ResourceDataUpdate(ad_frame_at_200ms, ResourceCached::kNotCached, 100);
  ResourceDataUpdate(ad_frame_at_300ms, ResourceCached::kNotCached, 100);

  SimulateFirstContentfulPaint(ad_frame_at_100ms,
                               base::Milliseconds(300));  // @400ms
  SimulateFirstContentfulPaint(ad_frame_at_200ms,
                               base::Milliseconds(90));  // @290ms
  SimulateFirstContentfulPaint(ad_frame_at_300ms,
                               base::Milliseconds(1290));  // @1590ms

  // Navigate away and check the histogram.
  NavigateFrame(kNonAdUrl, main_frame);

  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("AdPaintTiming.NavigationToFirstContentfulPaint3"), 3);

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram(
          "AdPaintTiming.TopFrameNavigationToFirstAdFirstContentfulPaint"),
      290, 1);
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram(
          "AdPaintTiming.TopFrameNavigationToFirstContentfulPaint"),
      400, 1);
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram(
          "AdPaintTiming.TopFrameNavigationToFirstContentfulPaint"),
      290, 1);
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram(
          "AdPaintTiming.TopFrameNavigationToFirstContentfulPaint"),
      1590, 1);

  histogram_tester().ExpectTotalCount(
      SuffixedHistogram(
          "AdPaintTiming.TopFrameNavigationToFirstContentfulPaint"),
      3);
}

TEST_P(AdsPageLoadMetricsObserverTest,
       FirstContentfulPaintNoAdRootPainted_Recorded) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  RenderFrameHost* sub_frame = CreateAndNavigateSubFrame(kAdUrl, ad_frame);

  // Load some bytes so that the frame is recorded.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 100);

  // Set FirstContentfulPaint for nested subframe. It is the only frame painted.
  SimulateFirstContentfulPaint(sub_frame, base::Milliseconds(90));

  // Navigate away and check the histogram.
  NavigateFrame(kNonAdUrl, main_frame);

  // The histogram value should be that of the earliest FCP recorded.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("AdPaintTiming.NavigationToFirstContentfulPaint3"), 90,
      1);

  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kTiming_FirstContentfulPaintName, 90);
}

class AdsMemoryMeasurementTest : public AdsPageLoadMetricsObserverTest {
 private:
  void SetUpScopedFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {blink::features::kFencedFrames,
             {{"implementation_type", "mparch"}}},
            {::features::kV8PerFrameMemoryMonitoring, {}},
        },
        {});
  }
};

INSTANTIATE_TEST_SUITE_P(All, AdsMemoryMeasurementTest, testing::Bool());

TEST_P(AdsMemoryMeasurementTest, SingleAdFrame_MaxMemoryBytesRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Load kilobytes in frame so that aggregates are recorded.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Notify that memory measurement is available.
  SimulateV8MemoryChange(ad_frame, 10 * 1024);

  // Update memory usage. The max will change, as 30 is positive.
  SimulateV8MemoryChange(ad_frame, 30 * 1024);

  // Update memory usage. The max will remain the same, as -20 is negative.
  SimulateV8MemoryChange(ad_frame, -20 * 1024);

  // Navigate main frame to record histograms.
  NavigateMainFrame(kNonAdUrl);

  histogram_tester().ExpectUniqueSample(kMemoryUpdateCountHistogramId, 3, 1);
}

TEST_P(AdsMemoryMeasurementTest, MultiAdFramesNested_MaxMemoryBytesRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame1 = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Create a nested subframe with the same origin as its parent.
  RenderFrameHost* ad_frame2 = CreateAndNavigateSubFrame(kAdUrl, ad_frame1);

  // Load kilobytes in each frame so that aggregates are recorded.
  ResourceDataUpdate(ad_frame1, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(ad_frame2, ResourceCached::kNotCached, 10);

  // Notify that memory measurement is available.
  SimulateV8MemoryChange(ad_frame1, 10 * 1024);
  SimulateV8MemoryChange(ad_frame2, 10 * 1024);

  // Update memory usage. The max will change, as these values are both
  // positive.
  SimulateV8MemoryChange(ad_frame1, 30 * 1024);
  SimulateV8MemoryChange(ad_frame2, 10 * 1024);

  // Update memory usage. The max will remain the same, as these values
  // are both negative.
  SimulateV8MemoryChange(ad_frame1, -25 * 1024);
  SimulateV8MemoryChange(ad_frame2, -5 * 1024);

  // Navigate main frame to record histograms.
  NavigateMainFrame(kNonAdUrl);

  histogram_tester().ExpectUniqueSample(kMemoryUpdateCountHistogramId, 6, 1);
}

TEST_P(AdsMemoryMeasurementTest,
       MultiAdFramesNonNested_MaxMemoryBytesRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame1 = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Create another ad subframe with a different origin.
  RenderFrameHost* ad_frame2 =
      CreateAndNavigateSubFrame(kOtherAdUrl, main_frame);

  // Load kilobytes in each frame so that aggregates are recorded.
  ResourceDataUpdate(ad_frame1, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(ad_frame2, ResourceCached::kNotCached, 10);

  // Notify that memory measurement is available.
  SimulateV8MemoryChange(ad_frame1, 10 * 1024);
  SimulateV8MemoryChange(ad_frame2, 10 * 1024);

  // Update memory usage. The second max and aggregate max
  // will change.
  SimulateV8MemoryChange(ad_frame1, -9 * 1024);
  SimulateV8MemoryChange(ad_frame2, 100 * 1024);

  // Update memory usage. The aggregate max will change
  // again after the first update.
  SimulateV8MemoryChange(ad_frame1, 1 * 1024);
  SimulateV8MemoryChange(ad_frame2, -90 * 1024);

  // Update memory usage. The first max will change.
  SimulateV8MemoryChange(ad_frame1, 50 * 1024);
  SimulateV8MemoryChange(ad_frame2, -5 * 1024);

  // Navigate main frame to record histograms.
  NavigateMainFrame(kNonAdUrl);

  histogram_tester().ExpectUniqueSample(kMemoryUpdateCountHistogramId, 8, 1);
}

TEST_P(AdsMemoryMeasurementTest, MainFrame_MaxMemoryBytesRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Load kilobytes in each frame. |ad_frame| must exist for ad metrics to be
  // tracked.
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 1000);
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Notify that memory measurement is available.
  SimulateV8MemoryChange(main_frame, 1000 * 1024);

  // Update memory usage. The max will also change, as this value is
  // positive.
  SimulateV8MemoryChange(main_frame, 1000 * 1024);

  // Update memory usage. The max will remain the same, as this value is
  // negative.
  SimulateV8MemoryChange(main_frame, -1980 * 1024);

  // Navigate to record histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  histogram_tester().ExpectUniqueSample(kMemoryMainFrameMaxHistogramId, 2000,
                                        1);
  histogram_tester().ExpectUniqueSample(kMemoryUpdateCountHistogramId, 3, 1);
}

}  // namespace page_load_metrics

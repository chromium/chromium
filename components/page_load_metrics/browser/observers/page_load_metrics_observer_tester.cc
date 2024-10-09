// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_tester.h"
#include "base/memory/raw_ptr.h"

#include <memory>
#include <string>
#include <utility>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_interface.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace page_load_metrics {

class PageLoadMetricsMemoryTracker;

namespace {

class TestPageLoadMetricsEmbedderInterface
    : public PageLoadMetricsEmbedderInterface {
 public:
  explicit TestPageLoadMetricsEmbedderInterface(
      PageLoadMetricsObserverTester* test)
      : test_(test) {}

  TestPageLoadMetricsEmbedderInterface(
      const TestPageLoadMetricsEmbedderInterface&) = delete;
  TestPageLoadMetricsEmbedderInterface& operator=(
      const TestPageLoadMetricsEmbedderInterface&) = delete;

  bool IsNewTabPageUrl(const GURL& url) override { return false; }

  // Forward the registration logic to the test class so that derived classes
  // can override the logic there without depending on the embedder interface.
  void RegisterObservers(
      PageLoadTracker* tracker,
      content::NavigationHandle* navigation_handle) override {
    test_->RegisterObservers(tracker);
  }

  std::unique_ptr<base::OneShotTimer> CreateTimer() override {
    auto timer = std::make_unique<test::WeakMockTimer>();
    test_->SetMockTimer(timer->AsWeakPtr());
    return std::move(timer);
  }

  bool IsNoStatePrefetch(content::WebContents* web_contents) override {
    return false;
  }

  bool IsExtensionUrl(const GURL& url) override { return false; }

  bool IsSidePanel(content::WebContents* web_contents) override {
    return false;
  }

  bool IsNonTabWebUI() override { return test_->is_non_tab_webui(); }

  page_load_metrics::PageLoadMetricsMemoryTracker*
  GetMemoryTrackerForBrowserContext(
      content::BrowserContext* browser_context) override {
    return nullptr;
  }

 private:
  raw_ptr<PageLoadMetricsObserverTester> test_;
};

}  // namespace

PageLoadMetricsObserverTester::PageLoadMetricsObserverTester(
    content::WebContents* web_contents,
    content::RenderViewHostTestHarness* rfh_test_harness,
    const RegisterObserversCallback& callback,
    bool is_non_tab_webui)
    : register_callback_(callback),
      web_contents_(web_contents),
      rfh_test_harness_(rfh_test_harness),
      metrics_web_contents_observer_(
          MetricsWebContentsObserver::CreateForWebContents(
              web_contents,
              std::make_unique<TestPageLoadMetricsEmbedderInterface>(this))),
      is_non_tab_webui_(is_non_tab_webui) {}

PageLoadMetricsObserverTester::~PageLoadMetricsObserverTester() = default;

void PageLoadMetricsObserverTester::StartNavigation(const GURL& gurl) {
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateBrowserInitiated(gurl,
                                                           web_contents());
  navigation->Start();
}

void PageLoadMetricsObserverTester::NavigateWithPageTransitionAndCommit(
    const GURL& url,
    ui::PageTransition transition) {
  auto simulator = PageTransitionIsWebTriggerable(transition)
                       ? content::NavigationSimulator::CreateRendererInitiated(
                             url, rfh_test_harness_->main_rfh())
                       : content::NavigationSimulator::CreateBrowserInitiated(
                             url, web_contents());
  simulator->SetTransition(transition);
  simulator->Commit();
}

void PageLoadMetricsObserverTester::NavigateToUntrackedUrl() {
  rfh_test_harness_->NavigateAndCommit(GURL(url::kAboutBlankURL));
}

void PageLoadMetricsObserverTester::SimulateTimingUpdate(
    const mojom::PageLoadTiming& timing) {
  SimulateTimingUpdate(timing, web_contents()->GetPrimaryMainFrame());
}

void PageLoadMetricsObserverTester::SimulateTimingUpdate(
    const mojom::PageLoadTiming& timing,
    content::RenderFrameHost* rfh) {
  SimulatePageLoadTimingUpdate(
      timing, mojom::FrameMetadata(), /* new_features= */ {},
      mojom::FrameRenderDataUpdate(), mojom::CpuTiming(), mojom::InputTiming(),
      std::nullopt, rfh, *CreateSoftNavigationMetrics());
}

void PageLoadMetricsObserverTester::SimulateCpuTimingUpdate(
    const mojom::CpuTiming& cpu_timing) {
  SimulateCpuTimingUpdate(cpu_timing, web_contents()->GetPrimaryMainFrame());
}

void PageLoadMetricsObserverTester::SimulateCpuTimingUpdate(
    const mojom::CpuTiming& cpu_timing,
    content::RenderFrameHost* rfh) {
  auto timing = page_load_metrics::mojom::PageLoadTimingPtr(std::in_place);
  page_load_metrics::InitPageLoadTimingForTest(timing.get());
  SimulatePageLoadTimingUpdate(
      *timing, mojom::FrameMetadata(),
      /* new_features= */ {}, mojom::FrameRenderDataUpdate(), cpu_timing,
      mojom::InputTiming(), std::nullopt, rfh, *CreateSoftNavigationMetrics());
}

void PageLoadMetricsObserverTester::SimulateInputTimingUpdate(
    const mojom::InputTiming& input_timing) {
  SimulateInputTimingUpdate(input_timing,
                            web_contents()->GetPrimaryMainFrame());
}

void PageLoadMetricsObserverTester::SimulateInputTimingUpdate(
    const mojom::InputTiming& input_timing,
    content::RenderFrameHost* rfh) {
  auto timing = page_load_metrics::mojom::PageLoadTimingPtr(std::in_place);
  page_load_metrics::InitPageLoadTimingForTest(timing.get());
  SimulatePageLoadTimingUpdate(
      *timing, mojom::FrameMetadata(), /* new_features= */ {},
      mojom::FrameRenderDataUpdate(), mojom::CpuTiming(), input_timing,
      std::nullopt, rfh, *CreateSoftNavigationMetrics());
}

void PageLoadMetricsObserverTester::SimulateTimingAndMetadataUpdate(
    const mojom::PageLoadTiming& timing,
    const mojom::FrameMetadata& metadata) {
  SimulatePageLoadTimingUpdate(
      timing, metadata, /* new_features= */ {}, mojom::FrameRenderDataUpdate(),
      mojom::CpuTiming(), mojom::InputTiming(), std::nullopt,
      web_contents()->GetPrimaryMainFrame(), *CreateSoftNavigationMetrics());
}

void PageLoadMetricsObserverTester::SimulateMetadataUpdate(
    const mojom::FrameMetadata& metadata,
    content::RenderFrameHost* rfh) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  SimulatePageLoadTimingUpdate(
      timing, metadata, /* new_features= */ {}, mojom::FrameRenderDataUpdate(),
      mojom::CpuTiming(), mojom::InputTiming(), std::nullopt, rfh,
      *CreateSoftNavigationMetrics());
}

void PageLoadMetricsObserverTester::SimulateFeaturesUpdate(
    const std::vector<blink::UseCounterFeature>& new_features) {
  SimulatePageLoadTimingUpdate(
      mojom::PageLoadTiming(), mojom::FrameMetadata(), new_features,
      mojom::FrameRenderDataUpdate(), mojom::CpuTiming(), mojom::InputTiming(),
      std::nullopt, web_contents()->GetPrimaryMainFrame(),
      *CreateSoftNavigationMetrics());
}

void PageLoadMetricsObserverTester::SimulateRenderDataUpdate(
    const mojom::FrameRenderDataUpdate& render_data) {
  SimulateRenderDataUpdate(render_data, web_contents()->GetPrimaryMainFrame());
}

void PageLoadMetricsObserverTester::SimulateRenderDataUpdate(
    const mojom::FrameRenderDataUpdate& render_data,
    content::RenderFrameHost* rfh) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  SimulatePageLoadTimingUpdate(
      timing, mojom::FrameMetadata(),
      /* new_features= */ {}, render_data, mojom::CpuTiming(),
      mojom::InputTiming(), std::nullopt, rfh, *CreateSoftNavigationMetrics());
}

void PageLoadMetricsObserverTester::SimulateSoftNavigation(
    content::NavigationHandle* navigation_handle) {
  SimulateDidFinishNavigation(navigation_handle);
}

void PageLoadMetricsObserverTester::SimulateDidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  metrics_web_contents_observer_->DidFinishNavigation(navigation_handle);
}

void PageLoadMetricsObserverTester::SimulateSoftNavigationCountUpdate(
    const mojom::SoftNavigationMetrics& soft_navigation_metrics) {
  SimulatePageLoadTimingUpdate(
      mojom::PageLoadTiming(), mojom::FrameMetadata(),
      /* new_features= */ {}, mojom::FrameRenderDataUpdate(),
      mojom::CpuTiming(), mojom::InputTiming(), std::nullopt,
      web_contents()->GetPrimaryMainFrame(), soft_navigation_metrics);
}

void PageLoadMetricsObserverTester::SimulatePageLoadTimingUpdate(
    const mojom::PageLoadTiming& timing,
    const mojom::FrameMetadata& metadata,
    const std::vector<blink::UseCounterFeature>& new_features,
    const mojom::FrameRenderDataUpdate& render_data,
    const mojom::CpuTiming& cpu_timing,
    const mojom::InputTiming& input_timing,
    const std::optional<blink::SubresourceLoadMetrics>&
        subresource_load_metrics,
    content::RenderFrameHost* rfh,
    const mojom::SoftNavigationMetrics& soft_navigation_metrics) {
  metrics_web_contents_observer_->OnTimingUpdated(
      rfh, timing.Clone(), metadata.Clone(), new_features,
      std::vector<mojom::ResourceDataUpdatePtr>(), render_data.Clone(),
      cpu_timing.Clone(), input_timing.Clone(), subresource_load_metrics,
      soft_navigation_metrics.Clone());
  // If sending the timing update caused the PageLoadMetricsUpdateDispatcher to
  // schedule a buffering timer, then fire it now so metrics are dispatched to
  // observers.
  base::MockOneShotTimer* mock_timer = GetMockTimer();
  if (mock_timer && mock_timer->IsRunning())
    mock_timer->Fire();
}

void PageLoadMetricsObserverTester::SimulateResourceDataUseUpdate(
    const std::vector<mojom::ResourceDataUpdatePtr>& resources) {
  SimulateResourceDataUseUpdate(resources,
                                web_contents()->GetPrimaryMainFrame());
}

void PageLoadMetricsObserverTester::SimulateResourceDataUseUpdate(
    const std::vector<mojom::ResourceDataUpdatePtr>& resources,
    content::RenderFrameHost* render_frame_host) {
  auto timing = mojom::PageLoadTimingPtr(std::in_place);
  InitPageLoadTimingForTest(timing.get());
  metrics_web_contents_observer_->OnTimingUpdated(
      render_frame_host, std::move(timing),
      mojom::FrameMetadataPtr(std::in_place),
      std::vector<blink::UseCounterFeature>(), resources,
      mojom::FrameRenderDataUpdatePtr(std::in_place),
      mojom::CpuTimingPtr(std::in_place), mojom::InputTimingPtr(std::in_place),
      std::nullopt, CreateSoftNavigationMetrics());
}

void PageLoadMetricsObserverTester::SimulateLoadedResource(
    const ExtraRequestCompleteInfo& info) {
  SimulateLoadedResource(info, content::GlobalRequestID());
}

void PageLoadMetricsObserverTester::SimulateLoadedResource(
    const ExtraRequestCompleteInfo& info,
    const content::GlobalRequestID& request_id) {
  if (info.request_destination ==
      network::mojom::RequestDestination::kDocument) {
    ASSERT_NE(content::GlobalRequestID(), request_id)
        << "Main frame resources must have a GlobalRequestID.";
  }

  blink::mojom::ResourceLoadInfo resource_load_info;
  resource_load_info.final_url = info.final_url.GetURL();
  resource_load_info.was_cached = info.was_cached;
  resource_load_info.raw_body_bytes = info.raw_body_bytes;
  resource_load_info.total_received_bytes =
      info.original_network_content_length;
  resource_load_info.request_destination = info.request_destination;
  resource_load_info.net_error = info.net_error;
  resource_load_info.network_info = blink::mojom::CommonNetworkInfo::New();
  resource_load_info.network_info->remote_endpoint = info.remote_endpoint;
  if (info.load_timing_info)
    resource_load_info.load_timing_info = *info.load_timing_info;
  else
    resource_load_info.load_timing_info.request_start = base::TimeTicks::Now();

  metrics_web_contents_observer_->ResourceLoadComplete(
      web_contents()->GetPrimaryMainFrame(), request_id, resource_load_info);
}

void PageLoadMetricsObserverTester::SimulateFrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  metrics_web_contents_observer_->FrameReceivedUserActivation(
      render_frame_host);
}

void PageLoadMetricsObserverTester::SimulateInputEvent(
    const blink::WebInputEvent& event) {
  metrics_web_contents_observer_->OnInputEvent(event);
}

void PageLoadMetricsObserverTester::SimulateAppEnterBackground() {
  metrics_web_contents_observer_->FlushMetricsOnAppEnterBackground();
}

void PageLoadMetricsObserverTester::SimulateMediaPlayed() {
  SimulateMediaPlayed(web_contents()->GetPrimaryMainFrame());
}

void PageLoadMetricsObserverTester::SimulateMediaPlayed(
    content::RenderFrameHost* rfh) {
  content::WebContentsObserver::MediaPlayerInfo video_type(
      /*has_video=*/true, /*has_audio=*/true);
  metrics_web_contents_observer_->MediaStartedPlaying(
      video_type, content::MediaPlayerId(rfh->GetGlobalId(), 0));
}

void PageLoadMetricsObserverTester::SimulateCookieAccess(
    const content::CookieAccessDetails& details) {
  metrics_web_contents_observer_->OnCookiesAccessed(
      metrics_web_contents_observer_->web_contents()->GetPrimaryMainFrame(),
      details);
}

void PageLoadMetricsObserverTester::SimulateStorageAccess(
    const GURL& url,
    const GURL& first_party_url,
    bool blocked_by_policy,
    StorageType storage_type) {
  metrics_web_contents_observer_->OnStorageAccessed(
      metrics_web_contents_observer_->web_contents()->GetPrimaryMainFrame(),
      url, first_party_url, blocked_by_policy, storage_type);
}

void PageLoadMetricsObserverTester::SimulateCustomUserTimingUpdate(
    mojom::CustomUserTimingMarkPtr custom_timing) {
  metrics_web_contents_observer_->OnCustomUserTimingUpdated(
      web_contents()->GetPrimaryMainFrame(), std::move(custom_timing));
}

const PageLoadMetricsObserverDelegate&
PageLoadMetricsObserverTester::GetDelegateForCommittedLoad() const {
  return metrics_web_contents_observer_->GetDelegateForCommittedLoad();
}

void PageLoadMetricsObserverTester::RegisterObservers(
    PageLoadTracker* tracker) {
  if (!register_callback_.is_null())
    register_callback_.Run(tracker);
}

void PageLoadMetricsObserverTester::SimulateMemoryUpdate(
    content::RenderFrameHost* render_frame_host,
    int64_t delta_bytes) {
  DCHECK(render_frame_host);
  if (delta_bytes != 0) {
    std::vector<MemoryUpdate> update(
        {MemoryUpdate(render_frame_host->GetGlobalId(), delta_bytes)});
    metrics_web_contents_observer_->OnV8MemoryChanged(update);
  }
}

}  // namespace page_load_metrics

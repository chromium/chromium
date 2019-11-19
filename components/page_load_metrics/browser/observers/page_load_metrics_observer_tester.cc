// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_tester.h"

#include <memory>
#include <string>
#include <utility>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_interface.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/resource_load_info.mojom.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace {

class TestPageLoadMetricsEmbedderInterface
    : public PageLoadMetricsEmbedderInterface {
 public:
  explicit TestPageLoadMetricsEmbedderInterface(
      PageLoadMetricsObserverTester* test)
      : test_(test) {}

  bool IsNewTabPageUrl(const GURL& url) override { return false; }

  // Forward the registration logic to the test class so that derived classes
  // can override the logic there without depending on the embedder interface.
  void RegisterObservers(PageLoadTracker* tracker) override {
    test_->RegisterObservers(tracker);
  }

  std::unique_ptr<base::OneShotTimer> CreateTimer() override {
    auto timer = std::make_unique<test::WeakMockTimer>();
    test_->SetMockTimer(timer->AsWeakPtr());
    return std::move(timer);
  }

  bool IsPrerender(content::WebContents* web_contents) override {
    return false;
  }

  bool IsExtensionUrl(const GURL& url) override { return false; }

 private:
  PageLoadMetricsObserverTester* test_;

  DISALLOW_COPY_AND_ASSIGN(TestPageLoadMetricsEmbedderInterface);
};

}  // namespace

PageLoadMetricsObserverTester::PageLoadMetricsObserverTester(
    content::WebContents* web_contents,
    content::RenderViewHostTestHarness* rfh_test_harness,
    const RegisterObserversCallback& callback)
    : register_callback_(callback),
      web_contents_(web_contents),
      rfh_test_harness_(rfh_test_harness),
      metrics_web_contents_observer_(
          MetricsWebContentsObserver::CreateForWebContents(
              web_contents,
              std::make_unique<TestPageLoadMetricsEmbedderInterface>(this))) {}

PageLoadMetricsObserverTester::~PageLoadMetricsObserverTester() {}

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
  SimulateTimingUpdate(timing, web_contents()->GetMainFrame());
}

void PageLoadMetricsObserverTester::SimulateTimingUpdate(
    const mojom::PageLoadTiming& timing,
    content::RenderFrameHost* rfh) {
  SimulatePageLoadTimingUpdate(
      timing, mojom::PageLoadMetadata(), mojom::PageLoadFeatures(),
      mojom::FrameRenderDataUpdate(), mojom::CpuTiming(),
      mojom::DeferredResourceCounts(), rfh);
}

void PageLoadMetricsObserverTester::SimulateCpuTimingUpdate(
    const mojom::CpuTiming& cpu_timing) {
  SimulateCpuTimingUpdate(cpu_timing, web_contents()->GetMainFrame());
}

void PageLoadMetricsObserverTester::SimulateCpuTimingUpdate(
    const mojom::CpuTiming& cpu_timing,
    content::RenderFrameHost* rfh) {
  auto timing = page_load_metrics::mojom::PageLoadTimingPtr(base::in_place);
  page_load_metrics::InitPageLoadTimingForTest(timing.get());
  SimulatePageLoadTimingUpdate(*timing, mojom::PageLoadMetadata(),
                               mojom::PageLoadFeatures(),
                               mojom::FrameRenderDataUpdate(), cpu_timing,
                               mojom::DeferredResourceCounts(), rfh);
}

void PageLoadMetricsObserverTester::SimulateTimingAndMetadataUpdate(
    const mojom::PageLoadTiming& timing,
    const mojom::PageLoadMetadata& metadata) {
  SimulatePageLoadTimingUpdate(
      timing, metadata, mojom::PageLoadFeatures(),
      mojom::FrameRenderDataUpdate(), mojom::CpuTiming(),
      mojom::DeferredResourceCounts(), web_contents()->GetMainFrame());
}

void PageLoadMetricsObserverTester::SimulateMetadataUpdate(
    const mojom::PageLoadMetadata& metadata,
    content::RenderFrameHost* rfh) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  SimulatePageLoadTimingUpdate(timing, metadata, mojom::PageLoadFeatures(),
                               mojom::FrameRenderDataUpdate(),
                               mojom::CpuTiming(),
                               mojom::DeferredResourceCounts(), rfh);
}

void PageLoadMetricsObserverTester::SimulateFeaturesUpdate(
    const mojom::PageLoadFeatures& new_features) {
  SimulatePageLoadTimingUpdate(
      mojom::PageLoadTiming(), mojom::PageLoadMetadata(), new_features,
      mojom::FrameRenderDataUpdate(), mojom::CpuTiming(),
      mojom::DeferredResourceCounts(), web_contents()->GetMainFrame());
}

void PageLoadMetricsObserverTester::SimulateRenderDataUpdate(
    const mojom::FrameRenderDataUpdate& render_data) {
  SimulateRenderDataUpdate(render_data, web_contents()->GetMainFrame());
}

void PageLoadMetricsObserverTester::SimulateRenderDataUpdate(
    const mojom::FrameRenderDataUpdate& render_data,
    content::RenderFrameHost* rfh) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  SimulatePageLoadTimingUpdate(
      timing, mojom::PageLoadMetadata(), mojom::PageLoadFeatures(), render_data,
      mojom::CpuTiming(), mojom::DeferredResourceCounts(), rfh);
}

void PageLoadMetricsObserverTester::SimulatePageLoadTimingUpdate(
    const mojom::PageLoadTiming& timing,
    const mojom::PageLoadMetadata& metadata,
    const mojom::PageLoadFeatures& new_features,
    const mojom::FrameRenderDataUpdate& render_data,
    const mojom::CpuTiming& cpu_timing,
    const mojom::DeferredResourceCounts& new_deferred_resource_data,
    content::RenderFrameHost* rfh) {
  metrics_web_contents_observer_->OnTimingUpdated(
      rfh, timing.Clone(), metadata.Clone(), new_features.Clone(),
      std::vector<mojom::ResourceDataUpdatePtr>(), render_data.Clone(),
      cpu_timing.Clone(), new_deferred_resource_data.Clone());
  // If sending the timing update caused the PageLoadMetricsUpdateDispatcher to
  // schedule a buffering timer, then fire it now so metrics are dispatched to
  // observers.
  base::MockOneShotTimer* mock_timer = GetMockTimer();
  if (mock_timer && mock_timer->IsRunning())
    mock_timer->Fire();
}

void PageLoadMetricsObserverTester::SimulateResourceDataUseUpdate(
    const std::vector<mojom::ResourceDataUpdatePtr>& resources) {
  SimulateResourceDataUseUpdate(resources, web_contents()->GetMainFrame());
}

void PageLoadMetricsObserverTester::SimulateResourceDataUseUpdate(
    const std::vector<mojom::ResourceDataUpdatePtr>& resources,
    content::RenderFrameHost* render_frame_host) {
  auto timing = mojom::PageLoadTimingPtr(base::in_place);
  InitPageLoadTimingForTest(timing.get());
  metrics_web_contents_observer_->OnTimingUpdated(
      render_frame_host, std::move(timing),
      mojom::PageLoadMetadataPtr(base::in_place),
      mojom::PageLoadFeaturesPtr(base::in_place), resources,
      mojom::FrameRenderDataUpdatePtr(base::in_place),
      mojom::CpuTimingPtr(base::in_place),
      mojom::DeferredResourceCountsPtr(base::in_place));
}

void PageLoadMetricsObserverTester::SimulateLoadedResource(
    const ExtraRequestCompleteInfo& info) {
  SimulateLoadedResource(info, content::GlobalRequestID());
}

void PageLoadMetricsObserverTester::SimulateLoadedResource(
    const ExtraRequestCompleteInfo& info,
    const content::GlobalRequestID& request_id) {
  if (info.resource_type == content::ResourceType::kMainFrame) {
    ASSERT_NE(content::GlobalRequestID(), request_id)
        << "Main frame resources must have a GlobalRequestID.";
  }

  content::mojom::ResourceLoadInfo resource_load_info;
  resource_load_info.url = info.origin_of_final_url.GetURL();
  resource_load_info.was_cached = info.was_cached;
  resource_load_info.raw_body_bytes = info.raw_body_bytes;
  resource_load_info.total_received_bytes =
      info.original_network_content_length;
  resource_load_info.resource_type = info.resource_type;
  resource_load_info.net_error = info.net_error;
  resource_load_info.network_info = content::mojom::CommonNetworkInfo::New();
  resource_load_info.network_info->remote_endpoint = info.remote_endpoint;
  if (info.load_timing_info)
    resource_load_info.load_timing_info = *info.load_timing_info;
  else
    resource_load_info.load_timing_info.request_start = base::TimeTicks::Now();

  metrics_web_contents_observer_->ResourceLoadComplete(
      web_contents()->GetMainFrame(), request_id, resource_load_info);
}

void PageLoadMetricsObserverTester::SimulateFrameReceivedFirstUserActivation(
    content::RenderFrameHost* render_frame_host) {
  metrics_web_contents_observer_->FrameReceivedFirstUserActivation(
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
  content::WebContentsObserver::MediaPlayerInfo video_type(
      true /* has_video*/, true /* has_audio */);
  content::RenderFrameHost* render_frame_host = web_contents()->GetMainFrame();
  metrics_web_contents_observer_->MediaStartedPlaying(
      video_type, content::MediaPlayerId(render_frame_host, 0));
}

void PageLoadMetricsObserverTester::SimulateCookiesRead(
    const GURL& url,
    const GURL& first_party_url,
    const net::CookieList& cookie_list,
    bool blocked_by_policy) {
  metrics_web_contents_observer_->OnCookiesRead(url, first_party_url,
                                                cookie_list, blocked_by_policy);
}

void PageLoadMetricsObserverTester::SimulateCookieChange(
    const GURL& url,
    const GURL& first_party_url,
    const net::CanonicalCookie& cookie,
    bool blocked_by_policy) {
  metrics_web_contents_observer_->OnCookieChange(url, first_party_url, cookie,
                                                 blocked_by_policy);
}

void PageLoadMetricsObserverTester::SimulateDomStorageAccess(
    const GURL& url,
    const GURL& first_party_url,
    bool local,
    bool blocked_by_policy) {
  metrics_web_contents_observer_->OnDomStorageAccessed(
      url, first_party_url, local, blocked_by_policy);
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

}  // namespace page_load_metrics

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_FORWARD_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_FORWARD_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_interface.h"
#include "content/public/browser/auction_result.h"

namespace page_load_metrics {

// This class is used to wrap another observer instance and forward metrics for
// the instance toward the another relevant instance running in the parent
// PageLoadTracker.
// Note: This class should override all virtual methods so to forward the
// callback correctly.
class PageLoadMetricsForwardObserver final
    : public PageLoadMetricsObserverInterface {
 public:
  // Refers `parent_observer` as a weak pointer because it may be destructed
  // at anytime when it returns STOP_OBSERVING in the callbacks for the events
  // happening in the parent page.
  explicit PageLoadMetricsForwardObserver(
      base::WeakPtr<PageLoadMetricsObserverInterface> parent_observer);

  PageLoadMetricsForwardObserver(const PageLoadMetricsForwardObserver&) =
      delete;
  PageLoadMetricsForwardObserver& operator=(
      const PageLoadMetricsForwardObserver&) = delete;

  ~PageLoadMetricsForwardObserver() override;

 private:
  // PageLoadMetricsObserverInterface implementation:
  const char* GetObserverName() const override;
  const PageLoadMetricsObserverDelegate& GetDelegate() const override;
  void SetDelegate(PageLoadMetricsObserverDelegate*) override;
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnPreviewStart(content::NavigationHandle* navigation_handle,
                               const GURL& currently_committed_url) override;
  ObservePolicy OnNavigationHandleTimingUpdated(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnDidInternalNavigationAbort(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNextNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnCommitSameDocumentNavigation(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnHidden(const mojom::PageLoadTiming& timing) override;
  ObservePolicy OnShown() override;
  ObservePolicy OnEnterBackForwardCache(
      const mojom::PageLoadTiming& timing) override;
  void OnRestoreFromBackForwardCache(
      const mojom::PageLoadTiming& timing,
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy ShouldObserveMimeType(
      const std::string& mime_type) const override;
  ObservePolicy ShouldObserveScheme(const GURL& url) const override;
  void OnTimingUpdate(content::RenderFrameHost* subframe_rfh,
                      const mojom::PageLoadTiming& timing) override;
  void OnSoftNavigationUpdated(const mojom::SoftNavigationMetrics&) override;
  void OnInputTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const mojom::InputTiming& input_timing_delta) override;
  void OnPageInputTimingUpdate(uint64_t num_interactions) override;
  void OnPageRenderDataUpdate(const mojom::FrameRenderDataUpdate& render_data,
                              bool is_main_frame) override;
  void OnSubFrameRenderDataUpdate(
      content::RenderFrameHost* subframe_rfh,
      const mojom::FrameRenderDataUpdate& render_data) override;
  void OnCpuTimingUpdate(content::RenderFrameHost* subframe_rfh,
                         const mojom::CpuTiming& timing) override;
  void OnUserInput(const blink::WebInputEvent& event,
                   const mojom::PageLoadTiming& timing) override;
  void OnDomContentLoadedEventStart(
      const mojom::PageLoadTiming& timing) override;
  void OnLoadEventStart(const mojom::PageLoadTiming& timing) override;
  void OnParseStart(const mojom::PageLoadTiming& timing) override;
  void OnParseStop(const mojom::PageLoadTiming& timing) override;
  void OnConnectStart(const mojom::PageLoadTiming& timing) override;
  void OnDomainLookupStart(const mojom::PageLoadTiming& timing) override;
  void OnDomainLookupEnd(const mojom::PageLoadTiming& timing) override;
  void OnFirstPaintInPage(const mojom::PageLoadTiming& timing) override;
  void OnFirstImagePaintInPage(const mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const mojom::PageLoadTiming& timing) override;
  void OnFirstPaintAfterBackForwardCacheRestoreInPage(
      const mojom::BackForwardCacheTiming& timing,
      size_t index) override;
  void OnFirstInputAfterBackForwardCacheRestoreInPage(
      const mojom::BackForwardCacheTiming& timing,
      size_t index) override;
  void OnRequestAnimationFramesAfterBackForwardCacheRestoreInPage(
      const mojom::BackForwardCacheTiming& timing,
      size_t index) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const mojom::PageLoadTiming& timing) override;
  void OnFirstInputInPage(const mojom::PageLoadTiming& timing) override;
  void OnLoadingBehaviorObserved(content::RenderFrameHost* rfh,
                                 int behavior_flags) override;
  void OnJavaScriptFrameworksObserved(
      content::RenderFrameHost* rfh,
      const blink::JavaScriptFrameworkDetectionResult&) override;
  void OnFeaturesUsageObserved(
      content::RenderFrameHost* rfh,
      const std::vector<blink::UseCounterFeature>& features) override;
  void SetUpSharedMemoryForSmoothness(
      const base::ReadOnlySharedMemoryRegion& shared_memory) override;
  void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<mojom::ResourceDataUpdatePtr>& resources) override;
  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      content::RenderFrameHost* render_frame_host) override;
  void OnMainFrameIntersectionRectChanged(
      content::RenderFrameHost* rfh,
      const gfx::Rect& main_frame_intersection_rect) override;
  void OnMainFrameViewportRectChanged(
      const gfx::Rect& main_frame_viewport_rect) override;
  void OnMainFrameImageAdRectsChanged(
      const base::flat_map<int, gfx::Rect>& main_frame_image_ad_rects) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const mojom::PageLoadTiming& timing) override;
  void OnComplete(const mojom::PageLoadTiming& timing) override;
  void OnFailedProvisionalLoad(
      const FailedProvisionalLoadInfo& failed_provisional_load_info) override;
  void OnLoadedResource(
      const ExtraRequestCompleteInfo& extra_request_complete_info) override;
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override;
  void FrameDisplayStateChanged(content::RenderFrameHost* render_frame_host,
                                bool is_display_none) override;
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override;
  void OnRenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override;
  void OnSubFrameDeleted(content::FrameTreeNodeId frame_tree_node_id) override;
  void OnCookiesRead(
      const GURL& url,
      const GURL& first_party_url,
      bool blocked_by_policy,
      bool is_ad_tagged,
      const net::CookieSettingOverrides& cookie_setting_overrides,
      bool is_partitioned_access) override;
  void OnCookieChange(
      const GURL& url,
      const GURL& first_party_url,
      const net::CanonicalCookie& cookie,
      bool blocked_by_policy,
      bool is_ad_tagged,
      const net::CookieSettingOverrides& cookie_setting_overrides,
      bool is_partitioned_access) override;
  void OnStorageAccessed(const GURL& url,
                         const GURL& first_party_url,
                         bool blocked_by_policy,
                         StorageType access_type) override;
  void OnPrefetchLikely() override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;
  void DidActivatePreviewedPage(base::TimeTicks activation_time) override;
  void OnV8MemoryChanged(
      const std::vector<MemoryUpdate>& memory_updates) override;
  void OnSharedStorageWorkletHostCreated() override;
  void OnSharedStorageSelectURLCalled() override;
  void OnCustomUserTimingMarkObserved(
      const std::vector<mojom::CustomUserTimingMarkPtr>& timings) override;
  void OnAdAuctionComplete(bool is_server_auction,
                           bool is_on_device_auction,
                           content::AuctionResult result) override;

  // Holds the forward target observer running in the parent PageLoadTracker.
  base::WeakPtr<PageLoadMetricsObserverInterface> parent_observer_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_FORWARD_OBSERVER_H_

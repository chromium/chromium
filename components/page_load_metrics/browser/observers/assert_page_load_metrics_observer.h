// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_ASSERT_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_ASSERT_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer_interface.h"

// Asserts the constraints of methods of PageLoadMetricsObserver using
// ovserver-level forwarding (i.e. PageLoadMetricsForwardObserver) as code and
// checks the behavior in browsertests of PageLoadMetricsObservers.
//
// This class will be added iff `DCHECK_IS_ON()`.
//
// The list of methods are not complete. For most ones among missing ones, we
// have no (non trivial) assumption on callback timings.
//
// Note that this inherits PageLoadMetricsObserverInterface rather than
// PageLoadMetricsObserver to encourage to write assertions for newly added
// methods.
class AssertPageLoadMetricsObserver final
    : public page_load_metrics::PageLoadMetricsObserverInterface {
 public:
  AssertPageLoadMetricsObserver();
  ~AssertPageLoadMetricsObserver() override;

  // PageLoadMetricsObserverInterface implementation:
  const char* GetObserverName() const override;

  const page_load_metrics::PageLoadMetricsObserverDelegate& GetDelegate()
      const override;
  void SetDelegate(
      page_load_metrics::PageLoadMetricsObserverDelegate*) override;

  // Initialization and redirect
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

  // Commit and activation
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;
  void DidActivatePreviewedPage(base::TimeTicks activation_time) override;

  // Termination-like events
  void OnFailedProvisionalLoad(
      const page_load_metrics::FailedProvisionalLoadInfo&
          failed_provisional_load_info) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnEnterBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  // Override to inspect
  ObservePolicy ShouldObserveMimeType(
      const std::string& mime_type) const override;
  ObservePolicy ShouldObserveScheme(const GURL& url) const override;

  // Events for navigations that are not related to PageLoadMetricsObserver's
  // lifetime
  void OnDidInternalNavigationAbort(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNextNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnCommitSameDocumentNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Visibility changes
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnShown() override;

  // Timing updates
  //
  // For more detailed event order, see page_load_metrics_update_dispatcher.cc.
  void OnTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnParseStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnParseStop(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnConnectStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnDomainLookupStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnDomainLookupEnd(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstImagePaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstPaintAfterBackForwardCacheRestoreInPage(
      const page_load_metrics::mojom::BackForwardCacheTiming& timing,
      size_t index) override;
  void OnFirstInputAfterBackForwardCacheRestoreInPage(
      const page_load_metrics::mojom::BackForwardCacheTiming& timing,
      size_t index) override;
  void OnRequestAnimationFramesAfterBackForwardCacheRestoreInPage(
      const page_load_metrics::mojom::BackForwardCacheTiming& timing,
      size_t index) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstInputInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  // Input events and input timing events
  void OnUserInput(
      const blink::WebInputEvent& event,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnPageInputTimingUpdate(uint64_t num_interactions) override;
  void OnInputTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::InputTiming& input_timing_delta) override;

  // Page render data update
  void OnPageRenderDataUpdate(
      const page_load_metrics::mojom::FrameRenderDataUpdate& render_data,
      bool is_main_frame) override;

  // Subframe events
  void OnSubFrameRenderDataUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::FrameRenderDataUpdate& render_data)
      override;

  // RenderFrameHost and FrameTreeNode deletion
  void OnRenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override;
  void OnSubFrameDeleted(content::FrameTreeNodeId frame_tree_node_id) override;

  // The method below are not well investigated.
  //
  // TODO(crbug.com/40856776): Add more assertions.
  void OnRestoreFromBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      content::NavigationHandle* navigation_handle) override {}
  void OnSoftNavigationUpdated(
      const page_load_metrics::mojom::SoftNavigationMetrics&) override {}
  void OnCpuTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::CpuTiming& timing) override {}
  void OnLoadingBehaviorObserved(content::RenderFrameHost* rfh,
                                 int behavior_flags) override {}
  void OnJavaScriptFrameworksObserved(
      content::RenderFrameHost* rfh,
      const blink::JavaScriptFrameworkDetectionResult&) override {}
  void OnFeaturesUsageObserved(
      content::RenderFrameHost* rfh,
      const std::vector<blink::UseCounterFeature>& features) override {}
  void SetUpSharedMemoryForSmoothness(
      const base::ReadOnlySharedMemoryRegion& shared_memory) override {}
  void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override {}
  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      content::RenderFrameHost* render_frame_host) override {}
  void OnMainFrameIntersectionRectChanged(
      content::RenderFrameHost* rfh,
      const gfx::Rect& main_frame_intersection_rect) override {}
  void OnMainFrameViewportRectChanged(
      const gfx::Rect& main_frame_viewport_rect) override {}
  void OnMainFrameImageAdRectsChanged(const base::flat_map<int, gfx::Rect>&
                                          main_frame_image_ad_rects) override {}
  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_complete_info) override {}
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override {}
  void FrameDisplayStateChanged(content::RenderFrameHost* render_frame_host,
                                bool is_display_none) override {}
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override {}
  void OnCookiesRead(
      const GURL& url,
      const GURL& first_party_url,
      bool blocked_by_policy,
      bool is_ad_tagged,
      const net::CookieSettingOverrides& cookie_setting_overrides,
      bool is_partitioned_access) override {}
  void OnCookieChange(
      const GURL& url,
      const GURL& first_party_url,
      const net::CanonicalCookie& cookie,
      bool blocked_by_policy,
      bool is_ad_tagged,
      const net::CookieSettingOverrides& cookie_setting_overrides,
      bool is_partitioned_access) override {}
  void OnStorageAccessed(const GURL& url,
                         const GURL& first_party_url,
                         bool blocked_by_policy,
                         page_load_metrics::StorageType access_type) override {}
  void OnPrefetchLikely() override {}
  void OnV8MemoryChanged(const std::vector<page_load_metrics::MemoryUpdate>&
                             memory_updates) override {}
  void OnSharedStorageWorkletHostCreated() override {}
  void OnSharedStorageSelectURLCalled() override {}
  void OnCustomUserTimingMarkObserved(
      const std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr>&
          timings) override {}
  void OnAdAuctionComplete(bool is_server_auction,
                           bool is_on_device_auction,
                           content::AuctionResult result) override {}

  // Reference implementations duplicated from PageLoadMetricsObserver
  ObservePolicy ShouldObserveMimeTypeByDefault(
      const std::string& mime_type) const;
  ObservePolicy OnEnterBackForwardCacheByDefault(
      const page_load_metrics::mojom::PageLoadTiming& timing);

 private:
  bool IsPrerendered() const;

  raw_ptr<page_load_metrics::PageLoadMetricsObserverDelegate> delegate_;

  bool started_ = false;
  // Same to `GetDelegate().DidCommit()`
  bool committed_ = false;
  // Same to `GetDelegate().GetPrerenderingData()` is one of
  // `kActivatedNoActivationStart` and `kActivated`.
  bool activated_ = false;
  mutable bool destructing_ = false;
  bool backforwardcache_entering_ = false;
  bool backforwardcache_entered_ = false;
  bool in_prerendering_ = false;
  bool in_preview_ = false;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_ASSERT_PAGE_LOAD_METRICS_OBSERVER_H_

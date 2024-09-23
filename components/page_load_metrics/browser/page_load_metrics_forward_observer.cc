// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_forward_observer.h"

#include "content/public/browser/auction_result.h"

namespace page_load_metrics {

PageLoadMetricsForwardObserver::PageLoadMetricsForwardObserver(
    base::WeakPtr<PageLoadMetricsObserverInterface> parent_observer)
    : parent_observer_(std::move(parent_observer)) {
  DCHECK(parent_observer_);
}

PageLoadMetricsForwardObserver::~PageLoadMetricsForwardObserver() = default;

const char* PageLoadMetricsForwardObserver::GetObserverName() const {
  // Returns the target ovserver's name so that it works even in cascaded cases,
  // i.e. an instance in the child page decides to forward to the page this
  // forward observer is tracking. Metrics from such child page should be also
  // forwarded to the parent page.
  if (parent_observer_)
    return parent_observer_->GetObserverName();
  return nullptr;
}

const PageLoadMetricsObserverDelegate&
PageLoadMetricsForwardObserver::GetDelegate() const {
  // TODO(crbug.com/40895492): Investigate whether this should truly be
  // unreachable. Note that all NOTREACHED()s were made non-fatal in this file,
  // they are not all necessarily hit.
  DUMP_WILL_BE_NOTREACHED();
  const PageLoadMetricsObserverDelegate* null_value = nullptr;
  return *null_value;
}

void PageLoadMetricsForwardObserver::SetDelegate(
    PageLoadMetricsObserverDelegate* delegate) {
  // No need to set. Ignore.
}

// Registration and initialization of PageLoadMetricsForwardObserver is
// different from ones of other PageLoadMetricsObserver subclasses.
// PageLoadMetricsForwardObserver is registered in
// `components/page_load_metrics/browser/page_load_tracker.cc` and methods
// OnStart, OnFencedFramesStart, and OnPrerenderingStart are never called.
PageLoadMetricsObserverInterface::ObservePolicy
PageLoadMetricsForwardObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  // TODO(crbug.com/40895492): Investigate whether this should truly be
  // unreachable. Note that all NOTREACHED()s were made non-fatal in this file,
  // they are not all necessarily hit.
  DUMP_WILL_BE_NOTREACHED();
  return STOP_OBSERVING;
}

PageLoadMetricsObserverInterface::ObservePolicy
PageLoadMetricsForwardObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // TODO(crbug.com/40895492): Investigate whether this should truly be
  // unreachable. Note that all NOTREACHED()s were made non-fatal in this file,
  // they are not all necessarily hit.
  DUMP_WILL_BE_NOTREACHED();
  return STOP_OBSERVING;
}

PageLoadMetricsObserverInterface::ObservePolicy
PageLoadMetricsForwardObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // TODO(crbug.com/40895492): Investigate whether this should truly be
  // unreachable. Note that all NOTREACHED()s were made non-fatal in this file,
  // they are not all necessarily hit.
  DUMP_WILL_BE_NOTREACHED();
  return STOP_OBSERVING;
}

PageLoadMetricsObserverInterface::ObservePolicy
PageLoadMetricsForwardObserver::OnPreviewStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // TODO(crbug.com/40895492): Investigate whether this should truly be
  // unreachable. Note that all NOTREACHED()s were made non-fatal in this file,
  // they are not all necessarily hit.
  DUMP_WILL_BE_NOTREACHED();
  return STOP_OBSERVING;
}

PageLoadMetricsObserverInterface::ObservePolicy
PageLoadMetricsForwardObserver::OnNavigationHandleTimingUpdated(
    content::NavigationHandle* navigation_handle) {
  // New events don't support forward observers.
  return CONTINUE_OBSERVING;
}

// Main frame events will be converted as sub-frame events on forwarding, and
// OnRedirect is an event only for the main frame. We just mask it here.
PageLoadMetricsObserverInterface::ObservePolicy
PageLoadMetricsForwardObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  return CONTINUE_OBSERVING;
}

// OnCommit and OnDidInternalNavigationAbort are handled at PageLoadTracker to
// forward events as a sub-frame navigation regardless of each observer's
// policy.
PageLoadMetricsObserverInterface::ObservePolicy
PageLoadMetricsForwardObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  return CONTINUE_OBSERVING;
}

void PageLoadMetricsForwardObserver::OnDidInternalNavigationAbort(
    content::NavigationHandle* navigation_handle) {}

// ReadyToCommitNextNavigation is an event only for main frames. As main frame
// events are converted to sub-frames events on forwarding, this event is just
// masked here.
void PageLoadMetricsForwardObserver::ReadyToCommitNextNavigation(
    content::NavigationHandle* navigation_handle) {}

// OnDidFinishSubFrameNavigation is handled at PageLoadTracker to forward events
// regardless of each observer's policy.
void PageLoadMetricsForwardObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {}

// OnCommitSameDocumentNavigation is handled at PageLoadTracker to forward
// events as a sub-frame navigation regardless of each observer's policy.
void PageLoadMetricsForwardObserver::OnCommitSameDocumentNavigation(
    content::NavigationHandle* navigation_handle) {}

// Inner pages' OnHidden and OnShown are ignored to avoid duplicated calls in
// the parent observer.
PageLoadMetricsObserverInterface::ObservePolicy
PageLoadMetricsForwardObserver::OnHidden(const mojom::PageLoadTiming& timing) {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserverInterface::ObservePolicy
PageLoadMetricsForwardObserver::OnShown() {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserverInterface::ObservePolicy
PageLoadMetricsForwardObserver::OnEnterBackForwardCache(
    const mojom::PageLoadTiming& timing) {
  return CONTINUE_OBSERVING;
}

void PageLoadMetricsForwardObserver::OnRestoreFromBackForwardCache(
    const mojom::PageLoadTiming& timing,
    content::NavigationHandle* navigation_handle) {}

PageLoadMetricsObserverInterface::ObservePolicy
PageLoadMetricsForwardObserver::ShouldObserveMimeType(
    const std::string& mime_type) const {
  if (!parent_observer_ ||
      parent_observer_->ShouldObserveMimeType(mime_type) == STOP_OBSERVING) {
    return STOP_OBSERVING;
  }
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserverInterface::ObservePolicy
PageLoadMetricsForwardObserver::ShouldObserveScheme(const GURL& url) const {
  if (!parent_observer_ ||
      parent_observer_->ShouldObserveScheme(url) == STOP_OBSERVING) {
    return STOP_OBSERVING;
  }
  return CONTINUE_OBSERVING;
}

// As PageLoadTracker handles OnTimingUpdate to dispatch also for the parent
// page, do not forward the event to the target here.
void PageLoadMetricsForwardObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const mojom::PageLoadTiming& timing) {}

// Soft navigations only happen in outermost top-level documents.
void PageLoadMetricsForwardObserver::OnSoftNavigationUpdated(
    const mojom::SoftNavigationMetrics&) {}

void PageLoadMetricsForwardObserver::OnInputTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const mojom::InputTiming& input_timing_delta) {}

void PageLoadMetricsForwardObserver::OnPageInputTimingUpdate(
    uint64_t num_interactions) {}

void PageLoadMetricsForwardObserver::OnPageRenderDataUpdate(
    const mojom::FrameRenderDataUpdate& render_data,
    bool is_main_frame) {}

void PageLoadMetricsForwardObserver::OnSubFrameRenderDataUpdate(
    content::RenderFrameHost* subframe_rfh,
    const mojom::FrameRenderDataUpdate& render_data) {
  if (!parent_observer_)
    return;
  parent_observer_->OnSubFrameRenderDataUpdate(subframe_rfh, render_data);
}

// As PageLoadTracker handles OnCpuTimingUpdate to dispatch also for the parent
// page, do not forward the event to the target here.
void PageLoadMetricsForwardObserver::OnCpuTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const mojom::CpuTiming& timing) {}

// OnUserInput is always dispatched only to the primary page.
void PageLoadMetricsForwardObserver::OnUserInput(
    const blink::WebInputEvent& event,
    const mojom::PageLoadTiming& timing) {
  // TODO(crbug.com/40895492): Investigate whether this should truly be
  // unreachable. Note that all NOTREACHED()s were made non-fatal in this file,
  // they are not all necessarily hit.
  DUMP_WILL_BE_NOTREACHED();
}

// Following events should be ignored as they are controlled at
// DispatchObserverTimingCallbacks in PageLoadTracker to be called once per
// observer. Relevant event sources are forwarded at PageLoadTracker layer.
void PageLoadMetricsForwardObserver::OnDomContentLoadedEventStart(
    const mojom::PageLoadTiming& timing) {}

void PageLoadMetricsForwardObserver::OnLoadEventStart(
    const mojom::PageLoadTiming& timing) {}

void PageLoadMetricsForwardObserver::OnParseStart(
    const mojom::PageLoadTiming& timing) {}

void PageLoadMetricsForwardObserver::OnParseStop(
    const mojom::PageLoadTiming& timing) {}

void PageLoadMetricsForwardObserver::OnConnectStart(
    const mojom::PageLoadTiming& timing) {}
void PageLoadMetricsForwardObserver::OnDomainLookupStart(
    const mojom::PageLoadTiming& timing) {}
void PageLoadMetricsForwardObserver::OnDomainLookupEnd(
    const mojom::PageLoadTiming& timing) {}

void PageLoadMetricsForwardObserver::OnFirstPaintInPage(
    const mojom::PageLoadTiming& timing) {}

void PageLoadMetricsForwardObserver::OnFirstImagePaintInPage(
    const mojom::PageLoadTiming& timing) {}

void PageLoadMetricsForwardObserver::OnFirstContentfulPaintInPage(
    const mojom::PageLoadTiming& timing) {}

void PageLoadMetricsForwardObserver::
    OnFirstPaintAfterBackForwardCacheRestoreInPage(
        const mojom::BackForwardCacheTiming& timing,
        size_t index) {
  // TODO(crbug.com/40895492): Investigate whether this should truly be
  // unreachable. Note that all NOTREACHED()s were made non-fatal in this file,
  // they are not all necessarily hit.
  DUMP_WILL_BE_NOTREACHED() << "Not supported.";
}

void PageLoadMetricsForwardObserver::
    OnFirstInputAfterBackForwardCacheRestoreInPage(
        const mojom::BackForwardCacheTiming& timing,
        size_t index) {
  // TODO(crbug.com/40895492): Investigate whether this should truly be
  // unreachable. Note that all NOTREACHED()s were made non-fatal in this file,
  // they are not all necessarily hit.
  DUMP_WILL_BE_NOTREACHED() << "Not supported.";
}

void PageLoadMetricsForwardObserver::
    OnRequestAnimationFramesAfterBackForwardCacheRestoreInPage(
        const mojom::BackForwardCacheTiming& timing,
        size_t index) {
  // TODO(crbug.com/40895492): Investigate whether this should truly be
  // unreachable. Note that all NOTREACHED()s were made non-fatal in this file,
  // they are not all necessarily hit.
  DUMP_WILL_BE_NOTREACHED() << "Not supported.";
}

void PageLoadMetricsForwardObserver::OnFirstMeaningfulPaintInMainFrameDocument(
    const mojom::PageLoadTiming& timing) {}

void PageLoadMetricsForwardObserver::OnFirstInputInPage(
    const mojom::PageLoadTiming& timing) {}

// OnLoadingBehaviorObserved and OnJavaScriptFrameworksObserved are called
// through PageLoadTracker::UpdateMetrics. So, the event is always forwarded at
// the PageLoadTracker layer.
void PageLoadMetricsForwardObserver::OnLoadingBehaviorObserved(
    content::RenderFrameHost* rfh,
    int behavior_flags) {}
void PageLoadMetricsForwardObserver::OnJavaScriptFrameworksObserved(
    content::RenderFrameHost* rfh,
    const blink::JavaScriptFrameworkDetectionResult&) {}

void PageLoadMetricsForwardObserver::OnFeaturesUsageObserved(
    content::RenderFrameHost* rfh,
    const std::vector<blink::UseCounterFeature>& features) {
  if (!parent_observer_)
    return;
  parent_observer_->OnFeaturesUsageObserved(rfh, features);
}

// SetUpSharedMemoryForSmoothness is called only for the outermost page.
void PageLoadMetricsForwardObserver::SetUpSharedMemoryForSmoothness(
    const base::ReadOnlySharedMemoryRegion& shared_memory) {
  // See also MetricsWebContentsObserver::SetUpSharedMemoryForSmoothness and
  // the relevant TODO. Currently, information from OOPIFs and FencedFrames are
  // not handled.
  // TODO(crbug.com/40895492): Investigate whether this should truly be
  // unreachable. Note that all NOTREACHED()s were made non-fatal in this file,
  // they are not all necessarily hit.
  DUMP_WILL_BE_NOTREACHED();
}

// PageLoadTracker already aggregates inter-pages data and processes it via
// PageLoadMetricsUpdateDispatcher to dispatch OnResourceDataUseObserved with
// the aggregated data. So, we don't need to forward here.
void PageLoadMetricsForwardObserver::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<mojom::ResourceDataUpdatePtr>& resources) {}

void PageLoadMetricsForwardObserver::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    content::RenderFrameHost* render_frame_host) {
  if (!parent_observer_)
    return;
  parent_observer_->MediaStartedPlaying(video_type, render_frame_host);
}

void PageLoadMetricsForwardObserver::OnMainFrameIntersectionRectChanged(
    content::RenderFrameHost* rfh,
    const gfx::Rect& main_frame_intersection_rect) {
  if (!parent_observer_)
    return;
  parent_observer_->OnMainFrameIntersectionRectChanged(
      rfh, main_frame_intersection_rect);
}

void PageLoadMetricsForwardObserver::OnMainFrameViewportRectChanged(
    const gfx::Rect& main_frame_viewport_rect) {
  if (!parent_observer_)
    return;
  parent_observer_->OnMainFrameViewportRectChanged(main_frame_viewport_rect);
}

void PageLoadMetricsForwardObserver::OnMainFrameImageAdRectsChanged(
    const base::flat_map<int, gfx::Rect>& main_frame_image_ad_rects) {
  if (!parent_observer_) {
    return;
  }
  parent_observer_->OnMainFrameImageAdRectsChanged(main_frame_image_ad_rects);
}

// Don't need to forward FlushMetricsOnAppEnterBackground and OnComplete as they
// are dispatched to all trackers.
PageLoadMetricsObserverInterface::ObservePolicy
PageLoadMetricsForwardObserver::FlushMetricsOnAppEnterBackground(
    const mojom::PageLoadTiming& timing) {
  return CONTINUE_OBSERVING;
}

void PageLoadMetricsForwardObserver::OnComplete(
    const mojom::PageLoadTiming& timing) {}

// OnFailedProvisionalLoad are handled at PageLoadTracker to forward events as a
// sub-frame navigation regardless of each observer's policy.
void PageLoadMetricsForwardObserver::OnFailedProvisionalLoad(
    const FailedProvisionalLoadInfo& failed_provisional_load_info) {}

void PageLoadMetricsForwardObserver::OnLoadedResource(
    const ExtraRequestCompleteInfo& extra_request_complete_info) {
  if (!parent_observer_)
    return;
  parent_observer_->OnLoadedResource(extra_request_complete_info);
}

void PageLoadMetricsForwardObserver::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  if (!parent_observer_)
    return;
  parent_observer_->FrameReceivedUserActivation(render_frame_host);
}

void PageLoadMetricsForwardObserver::FrameDisplayStateChanged(
    content::RenderFrameHost* render_frame_host,
    bool is_display_none) {
  if (!parent_observer_)
    return;
  parent_observer_->FrameDisplayStateChanged(render_frame_host,
                                             is_display_none);
}

void PageLoadMetricsForwardObserver::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  if (!parent_observer_)
    return;
  parent_observer_->FrameSizeChanged(render_frame_host, frame_size);
}

// OnRenderFrameDeleted and OnSubFrameDeleted are handled at PageLoadTracker to
// forward events as sub-frames deletion regardless of each observer's policy.
void PageLoadMetricsForwardObserver::OnRenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {}

void PageLoadMetricsForwardObserver::OnSubFrameDeleted(
    content::FrameTreeNodeId frame_tree_node_id) {}

void PageLoadMetricsForwardObserver::OnCookiesRead(
    const GURL& url,
    const GURL& first_party_url,
    bool blocked_by_policy,
    bool is_ad_tagged,
    const net::CookieSettingOverrides& cookie_setting_overrides,
    bool is_partitioned_access) {
  if (!parent_observer_)
    return;
  parent_observer_->OnCookiesRead(url, first_party_url, blocked_by_policy,
                                  is_ad_tagged, cookie_setting_overrides,
                                  is_partitioned_access);
}

void PageLoadMetricsForwardObserver::OnCookieChange(
    const GURL& url,
    const GURL& first_party_url,
    const net::CanonicalCookie& cookie,
    bool blocked_by_policy,
    bool is_ad_tagged,
    const net::CookieSettingOverrides& cookie_setting_overrides,
    bool is_partitioned_access) {
  if (!parent_observer_)
    return;
  parent_observer_->OnCookieChange(
      url, first_party_url, cookie, blocked_by_policy, is_ad_tagged,
      cookie_setting_overrides, is_partitioned_access);
}

void PageLoadMetricsForwardObserver::OnStorageAccessed(
    const GURL& url,
    const GURL& first_party_url,
    bool blocked_by_policy,
    StorageType access_type) {
  if (!parent_observer_)
    return;
  parent_observer_->OnStorageAccessed(url, first_party_url, blocked_by_policy,
                                      access_type);
}

void PageLoadMetricsForwardObserver::OnPrefetchLikely() {
  // This event is delivered only for the primary page.
  // TODO(crbug.com/40895492): Investigate whether this should truly be
  // unreachable. Note that all NOTREACHED()s were made non-fatal in this file,
  // they are not all necessarily hit.
  DUMP_WILL_BE_NOTREACHED();
}

void PageLoadMetricsForwardObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {}

void PageLoadMetricsForwardObserver::DidActivatePreviewedPage(
    base::TimeTicks activation_time) {}

void PageLoadMetricsForwardObserver::OnV8MemoryChanged(
    const std::vector<MemoryUpdate>& memory_updates) {
  if (!parent_observer_)
    return;
  parent_observer_->OnV8MemoryChanged(memory_updates);
}

void PageLoadMetricsForwardObserver::OnSharedStorageWorkletHostCreated() {
  if (!parent_observer_)
    return;
  parent_observer_->OnSharedStorageWorkletHostCreated();
}

void PageLoadMetricsForwardObserver::OnSharedStorageSelectURLCalled() {
  if (!parent_observer_) {
    return;
  }
  parent_observer_->OnSharedStorageSelectURLCalled();
}

void PageLoadMetricsForwardObserver::OnCustomUserTimingMarkObserved(
    const std::vector<mojom::CustomUserTimingMarkPtr>& timings) {
  // This new API doesn't support FORWARD_OBSERVING that is discouraged for new
  // observers.
}

void PageLoadMetricsForwardObserver::OnAdAuctionComplete(
    bool is_server_auction,
    bool is_on_device_auction,
    content::AuctionResult result) {
  if (!parent_observer_) {
    return;
  }
  parent_observer_->OnAdAuctionComplete(is_server_auction, is_on_device_auction,
                                        result);
}

}  // namespace page_load_metrics

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_forward_observer.h"

namespace page_load_metrics {

PageLoadMetricsForwardObserver::PageLoadMetricsForwardObserver(
    base::WeakPtr<PageLoadMetricsObserver> parent_observer)
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
  return PageLoadMetricsObserver::GetObserverName();
}

PageLoadMetricsObserver::ObservePolicy PageLoadMetricsForwardObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  // If the target observer is already destructed, we don't need to continue
  // observing. Also, even if the target observer decides forwarding, this
  // forwarding instance just needs to continue observing.
  if (!parent_observer_ ||
      parent_observer_->OnStart(navigation_handle, currently_committed_url,
                                started_in_foreground) == STOP_OBSERVING) {
    return STOP_OBSERVING;
  }
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
PageLoadMetricsForwardObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  if (!parent_observer_ ||
      parent_observer_->OnFencedFramesStart(
          navigation_handle, currently_committed_url) == STOP_OBSERVING) {
    return STOP_OBSERVING;
  }
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
PageLoadMetricsForwardObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  if (!parent_observer_ ||
      parent_observer_->OnPrerenderStart(
          navigation_handle, currently_committed_url) == STOP_OBSERVING) {
    return STOP_OBSERVING;
  }
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
PageLoadMetricsForwardObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  if (!parent_observer_ ||
      parent_observer_->OnRedirect(navigation_handle) == STOP_OBSERVING) {
    return STOP_OBSERVING;
  }
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy PageLoadMetricsForwardObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  if (!parent_observer_ ||
      parent_observer_->OnCommit(navigation_handle) == STOP_OBSERVING) {
    return STOP_OBSERVING;
  }
  return CONTINUE_OBSERVING;
}

void PageLoadMetricsForwardObserver::OnDidInternalNavigationAbort(
    content::NavigationHandle* navigation_handle) {
  if (!parent_observer_)
    return;
  parent_observer_->OnDidInternalNavigationAbort(navigation_handle);
}

void PageLoadMetricsForwardObserver::ReadyToCommitNextNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!parent_observer_)
    return;
  parent_observer_->ReadyToCommitNextNavigation(navigation_handle);
}

void PageLoadMetricsForwardObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!parent_observer_)
    return;
  parent_observer_->OnDidFinishSubFrameNavigation(navigation_handle);
}

void PageLoadMetricsForwardObserver::OnCommitSameDocumentNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!parent_observer_)
    return;
  parent_observer_->OnCommitSameDocumentNavigation(navigation_handle);
}

PageLoadMetricsObserver::ObservePolicy PageLoadMetricsForwardObserver::OnHidden(
    const mojom::PageLoadTiming& timing) {
  if (!parent_observer_ || parent_observer_->OnHidden(timing) == STOP_OBSERVING)
    return STOP_OBSERVING;
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
PageLoadMetricsForwardObserver::OnShown() {
  if (!parent_observer_ || parent_observer_->OnShown() == STOP_OBSERVING)
    return STOP_OBSERVING;
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
PageLoadMetricsForwardObserver::OnEnterBackForwardCache(
    const mojom::PageLoadTiming& timing) {
  if (!parent_observer_ ||
      parent_observer_->OnEnterBackForwardCache(timing) == STOP_OBSERVING) {
    return STOP_OBSERVING;
  }
  return CONTINUE_OBSERVING;
}

void PageLoadMetricsForwardObserver::OnRestoreFromBackForwardCache(
    const mojom::PageLoadTiming& timing,
    content::NavigationHandle* navigation_handle) {
  if (!parent_observer_)
    return;
  parent_observer_->OnRestoreFromBackForwardCache(timing, navigation_handle);
}

PageLoadMetricsObserver::ObservePolicy
PageLoadMetricsForwardObserver::ShouldObserveMimeType(
    const std::string& mime_type) const {
  if (!parent_observer_ ||
      parent_observer_->ShouldObserveMimeType(mime_type) == STOP_OBSERVING) {
    return STOP_OBSERVING;
  }
  return CONTINUE_OBSERVING;
}

void PageLoadMetricsForwardObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const mojom::PageLoadTiming& timing) {
  if (!parent_observer_)
    return;
  parent_observer_->OnTimingUpdate(subframe_rfh, timing);
}

void PageLoadMetricsForwardObserver::OnMobileFriendlinessUpdate(
    const blink::MobileFriendliness& mobile_friendliness) {
  if (!parent_observer_)
    return;
  parent_observer_->OnMobileFriendlinessUpdate(mobile_friendliness);
}

void PageLoadMetricsForwardObserver::OnInputTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const mojom::InputTiming& input_timing_delta) {
  if (!parent_observer_)
    return;
  parent_observer_->OnInputTimingUpdate(subframe_rfh, input_timing_delta);
}

void PageLoadMetricsForwardObserver::OnSubFrameRenderDataUpdate(
    content::RenderFrameHost* subframe_rfh,
    const mojom::FrameRenderDataUpdate& render_data) {
  if (!parent_observer_)
    return;
  parent_observer_->OnSubFrameRenderDataUpdate(subframe_rfh, render_data);
}

void PageLoadMetricsForwardObserver::OnCpuTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const mojom::CpuTiming& timing) {
  if (!parent_observer_)
    return;
  parent_observer_->OnCpuTimingUpdate(subframe_rfh, timing);
}

void PageLoadMetricsForwardObserver::OnUserInput(
    const blink::WebInputEvent& event,
    const mojom::PageLoadTiming& timing) {
  if (!parent_observer_)
    return;
  parent_observer_->OnUserInput(event, timing);
}

void PageLoadMetricsForwardObserver::OnDomContentLoadedEventStart(
    const mojom::PageLoadTiming& timing) {
  if (!parent_observer_)
    return;
  parent_observer_->OnDomContentLoadedEventStart(timing);
}

void PageLoadMetricsForwardObserver::OnLoadEventStart(
    const mojom::PageLoadTiming& timing) {
  if (!parent_observer_)
    return;
  parent_observer_->OnLoadEventStart(timing);
}

void PageLoadMetricsForwardObserver::OnFirstLayout(
    const mojom::PageLoadTiming& timing) {
  if (!parent_observer_)
    return;
  parent_observer_->OnFirstLayout(timing);
}

void PageLoadMetricsForwardObserver::OnParseStart(
    const mojom::PageLoadTiming& timing) {
  if (!parent_observer_)
    return;
  parent_observer_->OnParseStart(timing);
}

void PageLoadMetricsForwardObserver::OnParseStop(
    const mojom::PageLoadTiming& timing) {
  if (!parent_observer_)
    return;
  parent_observer_->OnParseStop(timing);
}

void PageLoadMetricsForwardObserver::OnFirstPaintInPage(
    const mojom::PageLoadTiming& timing) {
  if (!parent_observer_)
    return;
  parent_observer_->OnFirstPaintInPage(timing);
}

void PageLoadMetricsForwardObserver::OnFirstImagePaintInPage(
    const mojom::PageLoadTiming& timing) {
  if (!parent_observer_)
    return;
  parent_observer_->OnFirstImagePaintInPage(timing);
}

void PageLoadMetricsForwardObserver::OnFirstContentfulPaintInPage(
    const mojom::PageLoadTiming& timing) {
  if (!parent_observer_)
    return;
  parent_observer_->OnFirstContentfulPaintInPage(timing);
}

void PageLoadMetricsForwardObserver::
    OnFirstPaintAfterBackForwardCacheRestoreInPage(
        const mojom::BackForwardCacheTiming& timing,
        size_t index) {
  if (!parent_observer_)
    return;
  parent_observer_->OnFirstPaintAfterBackForwardCacheRestoreInPage(timing,
                                                                   index);
}

void PageLoadMetricsForwardObserver::
    OnFirstInputAfterBackForwardCacheRestoreInPage(
        const mojom::BackForwardCacheTiming& timing,
        size_t index) {
  if (!parent_observer_)
    return;
  parent_observer_->OnFirstInputAfterBackForwardCacheRestoreInPage(timing,
                                                                   index);
}

void PageLoadMetricsForwardObserver::
    OnRequestAnimationFramesAfterBackForwardCacheRestoreInPage(
        const mojom::BackForwardCacheTiming& timing,
        size_t index) {
  if (!parent_observer_)
    return;
  parent_observer_->OnRequestAnimationFramesAfterBackForwardCacheRestoreInPage(
      timing, index);
}

void PageLoadMetricsForwardObserver::OnFirstMeaningfulPaintInMainFrameDocument(
    const mojom::PageLoadTiming& timing) {
  if (!parent_observer_)
    return;
  parent_observer_->OnFirstMeaningfulPaintInMainFrameDocument(timing);
}

void PageLoadMetricsForwardObserver::OnFirstInputInPage(
    const mojom::PageLoadTiming& timing) {
  if (!parent_observer_)
    return;
  parent_observer_->OnFirstInputInPage(timing);
}

void PageLoadMetricsForwardObserver::OnLoadingBehaviorObserved(
    content::RenderFrameHost* rfh,
    int behavior_flags) {
  if (!parent_observer_)
    return;
  parent_observer_->OnLoadingBehaviorObserved(rfh, behavior_flags);
}

void PageLoadMetricsForwardObserver::OnFeaturesUsageObserved(
    content::RenderFrameHost* rfh,
    const std::vector<blink::UseCounterFeature>& features) {
  if (!parent_observer_)
    return;
  parent_observer_->OnFeaturesUsageObserved(rfh, features);
}

void PageLoadMetricsForwardObserver::SetUpSharedMemoryForSmoothness(
    const base::ReadOnlySharedMemoryRegion& shared_memory) {
  if (!parent_observer_)
    return;
  parent_observer_->SetUpSharedMemoryForSmoothness(shared_memory);
}

void PageLoadMetricsForwardObserver::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<mojom::ResourceDataUpdatePtr>& resources) {
  if (!parent_observer_)
    return;
  parent_observer_->OnResourceDataUseObserved(rfh, resources);
}

void PageLoadMetricsForwardObserver::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    content::RenderFrameHost* render_frame_host) {
  if (!parent_observer_)
    return;
  parent_observer_->MediaStartedPlaying(video_type, render_frame_host);
}

void PageLoadMetricsForwardObserver::OnFrameIntersectionUpdate(
    content::RenderFrameHost* rfh,
    const mojom::FrameIntersectionUpdate& intersection_update) {
  if (!parent_observer_)
    return;
  parent_observer_->OnFrameIntersectionUpdate(rfh, intersection_update);
}

PageLoadMetricsObserver::ObservePolicy
PageLoadMetricsForwardObserver::FlushMetricsOnAppEnterBackground(
    const mojom::PageLoadTiming& timing) {
  if (!parent_observer_ || parent_observer_->FlushMetricsOnAppEnterBackground(
                               timing) == STOP_OBSERVING) {
    return STOP_OBSERVING;
  }
  return CONTINUE_OBSERVING;
}

void PageLoadMetricsForwardObserver::OnComplete(
    const mojom::PageLoadTiming& timing) {
  if (!parent_observer_)
    return;
  parent_observer_->OnComplete(timing);
}

void PageLoadMetricsForwardObserver::OnFailedProvisionalLoad(
    const FailedProvisionalLoadInfo& failed_provisional_load_info) {
  if (!parent_observer_)
    return;
  parent_observer_->OnFailedProvisionalLoad(failed_provisional_load_info);
}

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

void PageLoadMetricsForwardObserver::OnRenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (!parent_observer_)
    return;
  parent_observer_->OnRenderFrameDeleted(render_frame_host);
}

void PageLoadMetricsForwardObserver::OnSubFrameDeleted(int frame_tree_node_id) {
  if (!parent_observer_)
    return;
  parent_observer_->OnSubFrameDeleted(frame_tree_node_id);
}

void PageLoadMetricsForwardObserver::OnCookiesRead(
    const GURL& url,
    const GURL& first_party_url,
    const net::CookieList& cookie_list,
    bool blocked_by_policy) {
  if (!parent_observer_)
    return;
  parent_observer_->OnCookiesRead(url, first_party_url, cookie_list,
                                  blocked_by_policy);
}

void PageLoadMetricsForwardObserver::OnCookieChange(
    const GURL& url,
    const GURL& first_party_url,
    const net::CanonicalCookie& cookie,
    bool blocked_by_policy) {
  if (!parent_observer_)
    return;
  parent_observer_->OnCookieChange(url, first_party_url, cookie,
                                   blocked_by_policy);
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
  if (!parent_observer_)
    return;
  parent_observer_->OnPrefetchLikely();
}

void PageLoadMetricsForwardObserver::DidActivatePortal(
    base::TimeTicks activation_time) {
  if (!parent_observer_)
    return;
  parent_observer_->DidActivatePortal(activation_time);
}

void PageLoadMetricsForwardObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  if (!parent_observer_)
    return;
  parent_observer_->DidActivatePrerenderedPage(navigation_handle);
}

void PageLoadMetricsForwardObserver::OnV8MemoryChanged(
    const std::vector<MemoryUpdate>& memory_updates) {
  if (!parent_observer_)
    return;
  parent_observer_->OnV8MemoryChanged(memory_updates);
}

}  // namespace page_load_metrics

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/assert_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"

using page_load_metrics::PageLoadMetricsObserver;
using page_load_metrics::PrerenderingState;

AssertPageLoadMetricsObserver::AssertPageLoadMetricsObserver() = default;

AssertPageLoadMetricsObserver::~AssertPageLoadMetricsObserver() {
  // Restrict the check because this class can't detect
  // PageLoadTracker::StopTracking case.
  if (!committed_) {
    return;
  }

  CHECK(destructing_);
}

const page_load_metrics::PageLoadMetricsObserverDelegate&
AssertPageLoadMetricsObserver::GetDelegate() const {
  // The delegate must exist and outlive the page load metrics observer.
  CHECK(delegate_);
  return *delegate_;
}

void AssertPageLoadMetricsObserver::SetDelegate(
    page_load_metrics::PageLoadMetricsObserverDelegate* delegate) {
  delegate_ = delegate;
}

const char* AssertPageLoadMetricsObserver::GetObserverName() const {
  return "AssertPageLoadMetricsObserver";
}

PageLoadMetricsObserver::ObservePolicy AssertPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  CHECK(!started_);
  started_ = true;

  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
AssertPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  CHECK(!started_);
  started_ = true;

  // This class uses observer-level forwarding.
  return FORWARD_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
AssertPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  CHECK(!started_);
  started_ = true;
  in_prerendering_ = true;

  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
AssertPageLoadMetricsObserver::OnPreviewStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  CHECK(!started_);
  started_ = true;
  in_preview_ = true;

  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
AssertPageLoadMetricsObserver::OnNavigationHandleTimingUpdated(
    content::NavigationHandle* navigation_handle) {
  CHECK(started_);
  CHECK(!committed_);

  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
AssertPageLoadMetricsObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  CHECK(started_);
  CHECK(!committed_);

  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy AssertPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  CHECK(started_);
  CHECK(!committed_);
  CHECK(!activated_);
  committed_ = true;

  CHECK(navigation_handle->IsInPrimaryMainFrame() ||
        navigation_handle->IsInPrerenderedMainFrame());
  CHECK(!navigation_handle->IsPrerenderedPageActivation());

  return CONTINUE_OBSERVING;
}

void AssertPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  CHECK(started_);
  CHECK(committed_);
  CHECK(!activated_);
  CHECK(in_prerendering_);
  CHECK(!in_preview_);
  activated_ = true;
  in_prerendering_ = false;
}

void AssertPageLoadMetricsObserver::DidActivatePreviewedPage(
    base::TimeTicks activation_time) {
  CHECK(started_);
  CHECK(committed_);
  CHECK(!activated_);
  CHECK(!in_prerendering_);
  CHECK(in_preview_);
  activated_ = true;
  in_preview_ = false;
}

void AssertPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo&
        failed_provisional_load_info) {
  CHECK(started_);
  CHECK(!committed_);
  CHECK(!activated_);
  CHECK(!backforwardcache_entered_);
  CHECK(!destructing_);
  destructing_ = true;
}

void AssertPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // Note that the default implementation of
  // PageLoadMetricsObserver::OnEnterBackForwardCache calls OnComplete and
  // returns STOP_OBSERVING.

  CHECK(committed_);
  CHECK(!destructing_);

  if (!backforwardcache_entering_) {
    destructing_ = true;
  }
}

PageLoadMetricsObserver::ObservePolicy
AssertPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // This is called even for provisional loads.
  CHECK(started_);

  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
AssertPageLoadMetricsObserver::ShouldObserveMimeType(
    const std::string& mime_type) const {
  // Sets a flag for destructor's assertion.

  ObservePolicy policy = ShouldObserveMimeTypeByDefault(mime_type);

  if (policy == STOP_OBSERVING) {
    destructing_ = true;
  }

  return policy;
}

PageLoadMetricsObserver::ObservePolicy
AssertPageLoadMetricsObserver::ShouldObserveMimeTypeByDefault(
    const std::string& mime_type) const {
  return PageLoadMetricsObserver::IsStandardWebPageMimeType(mime_type)
             ? CONTINUE_OBSERVING
             : STOP_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
AssertPageLoadMetricsObserver::ShouldObserveScheme(const GURL& url) const {
  return url.SchemeIsHTTPOrHTTPS() ? CONTINUE_OBSERVING : STOP_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
AssertPageLoadMetricsObserver::OnEnterBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(committed_);

  // If ovserver doesn't override OnEnterBackForwardCache, the observer will be
  // stopped with OnComplete called.
  backforwardcache_entering_ = true;
  PageLoadMetricsObserver::ObservePolicy policy =
      OnEnterBackForwardCacheByDefault(timing);
  backforwardcache_entering_ = false;
  CHECK_EQ(policy, STOP_OBSERVING);

  backforwardcache_entered_ = true;

  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
AssertPageLoadMetricsObserver::OnEnterBackForwardCacheByDefault(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // Invoke OnComplete to ensure that recorded data is dumped.
  OnComplete(timing);
  return STOP_OBSERVING;
}

void AssertPageLoadMetricsObserver::ReadyToCommitNextNavigation(
    content::NavigationHandle* navigation_handle) {
  CHECK(committed_);
}

void AssertPageLoadMetricsObserver::OnCommitSameDocumentNavigation(
    content::NavigationHandle* navigation_handle) {
  CHECK(committed_);
  // TODO(kenoss): I can't explain why this doesn't hold now. Write description.
  // if (IsPrerendered()) {
  //   (CHECK(activated_));
  // }
}

void AssertPageLoadMetricsObserver::OnDidInternalNavigationAbort(
    content::NavigationHandle* navigation_handle) {
  CHECK(started_);
}

void AssertPageLoadMetricsObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  // Subframe navigations are triggered after the main frame is committed.
  CHECK(committed_);
}

PageLoadMetricsObserver::ObservePolicy
AssertPageLoadMetricsObserver::OnShown() {
  CHECK(started_);
  // If prerendered, this is called even for provisional loads.
  // After https://crrev.com/c/3767770, we can assume that `activated_`.
  // TODO(kenoss): Add CHECK.

  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy AssertPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(started_);
  // If prerendered, this is called even for provisional loads.
  // After https://crrev.com/c/3767770, we can assume that `activated_`.
  // TODO(kenoss): Add CHECK.

  return CONTINUE_OBSERVING;
}

void AssertPageLoadMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(started_);
}

void AssertPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(started_);
  CHECK(timing.parse_timing->parse_start.has_value());
}

void AssertPageLoadMetricsObserver::OnParseStop(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(started_);
  CHECK(timing.parse_timing->parse_stop.has_value());
}

void AssertPageLoadMetricsObserver::OnConnectStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {}
void AssertPageLoadMetricsObserver::OnDomainLookupStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {}
void AssertPageLoadMetricsObserver::OnDomainLookupEnd(
    const page_load_metrics::mojom::PageLoadTiming& timing) {}

void AssertPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(started_);
  CHECK(timing.document_timing->dom_content_loaded_event_start.has_value());
}

void AssertPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(started_);
  CHECK(timing.document_timing->load_event_start.has_value());
}

void AssertPageLoadMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(committed_);
  if (IsPrerendered()) {
    CHECK(activated_);
  }
  CHECK(timing.paint_timing->first_paint.has_value());
}

void AssertPageLoadMetricsObserver::OnFirstImagePaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(committed_);
  if (IsPrerendered()) {
    CHECK(activated_);
  }
  CHECK(timing.paint_timing->first_image_paint.has_value());
}

void AssertPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(committed_);
  if (IsPrerendered()) {
    CHECK(activated_);
  }
  CHECK(timing.paint_timing->first_contentful_paint.has_value());
}

void AssertPageLoadMetricsObserver::OnFirstMeaningfulPaintInMainFrameDocument(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(committed_);
  if (IsPrerendered()) {
    CHECK(activated_);
  }
  CHECK(timing.paint_timing->first_meaningful_paint.has_value());
}

void AssertPageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(committed_);
  if (IsPrerendered()) {
    CHECK(activated_);
  }
  CHECK(timing.interactive_timing->first_input_delay.has_value());
}

void AssertPageLoadMetricsObserver::
    OnFirstPaintAfterBackForwardCacheRestoreInPage(
        const page_load_metrics::mojom::BackForwardCacheTiming& timing,
        size_t index) {
  CHECK(backforwardcache_entered_);
  CHECK(!timing.first_paint_after_back_forward_cache_restore.is_zero());
}

void AssertPageLoadMetricsObserver::
    OnFirstInputAfterBackForwardCacheRestoreInPage(
        const page_load_metrics::mojom::BackForwardCacheTiming& timing,
        size_t index) {
  CHECK(backforwardcache_entered_);
  CHECK(timing.first_input_delay_after_back_forward_cache_restore.has_value());
}

void AssertPageLoadMetricsObserver::
    OnRequestAnimationFramesAfterBackForwardCacheRestoreInPage(
        const page_load_metrics::mojom::BackForwardCacheTiming& timing,
        size_t index) {
  CHECK(backforwardcache_entered_);
  CHECK_EQ(
      timing.request_animation_frames_after_back_forward_cache_restore.size(),
      3u);
}

void AssertPageLoadMetricsObserver::OnUserInput(
    const blink::WebInputEvent& event,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(committed_);
  // If prerendered, input events are triggered after activation.
  if (IsPrerendered()) {
    CHECK(activated_);
  }
}

void AssertPageLoadMetricsObserver::OnPageInputTimingUpdate(
    uint64_t num_interactions) {
  CHECK(committed_);
  // If prerendered, input events are triggered after activation.
  if (IsPrerendered()) {
    CHECK(activated_);
  }
}

void AssertPageLoadMetricsObserver::OnInputTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::InputTiming& input_timing_delta) {
  // This callback is triggered even if there's no input, even before
  // activation.
  CHECK(committed_);
}

void AssertPageLoadMetricsObserver::OnRenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  CHECK(started_);
}

void AssertPageLoadMetricsObserver::OnSubFrameDeleted(
    content::FrameTreeNodeId frame_tree_node_id) {
  CHECK(started_);
}

void AssertPageLoadMetricsObserver::OnPageRenderDataUpdate(
    const page_load_metrics::mojom::FrameRenderDataUpdate& render_data,
    bool is_main_frame) {
  CHECK(committed_);
}

void AssertPageLoadMetricsObserver::OnSubFrameRenderDataUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::FrameRenderDataUpdate& render_data) {
  CHECK(committed_);
}

bool AssertPageLoadMetricsObserver::IsPrerendered() const {
  return (GetDelegate().GetPrerenderingState() !=
          PrerenderingState::kNoPrerendering);
}

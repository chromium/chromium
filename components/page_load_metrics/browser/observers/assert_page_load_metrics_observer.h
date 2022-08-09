// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_ASSERT_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_ASSERT_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

// Asserts the constraints of methods of PageLoadMetricsObserver using
// ovserver-level forwarding (i.e. PageLoadMetricsForwardObserver) as code and
// checks the behavior in browsertests of PageLoadMetricsObservers.
//
// This class will be added iff `DCHECK_IS_ON()`.
//
// The list of methods are not complete. For most ones among missing ones, we
// have no (non trivial) assumption on callback timings.
class AssertPageLoadMetricsObserver final
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  AssertPageLoadMetricsObserver();
  ~AssertPageLoadMetricsObserver() override;

  // PageLoadMetricsObserverInterface implementation:
  const char* GetObserverName() const override;

  // Initialization and redirect
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle) override;

  // Commit and activation
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;

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
  void OnPageInputTimingUpdate(uint64_t num_input_events) override;
  void OnInputTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::InputTiming& input_timing_delta) override;

  // Subframe events
  void OnSubFrameRenderDataUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::FrameRenderDataUpdate& render_data)
      override;

  // RenderFrameHost and FrameTreeNode deletion
  void OnRenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override;
  void OnSubFrameDeleted(int frame_tree_node_id) override;

 private:
  bool IsPrerendered() const;

  bool started_ = false;
  // Same to `GetDelegate().DidCommit()`
  bool committed_ = false;
  // Same to `GetDelegate().GetPrerenderingData()` is one of
  // `kActivatedNoActivationStart` and `kActivated`.
  bool activated_ = false;
  mutable bool destructing_ = false;
  bool backforwardcache_entering_ = false;
  bool backforwardcache_entered_ = false;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_ASSERT_PAGE_LOAD_METRICS_OBSERVER_H_

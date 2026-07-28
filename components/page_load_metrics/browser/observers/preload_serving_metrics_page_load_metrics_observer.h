// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRELOAD_SERVING_METRICS_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRELOAD_SERVING_METRICS_PAGE_LOAD_METRICS_OBSERVER_H_

#include <optional>
#include <string>

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/preload_serving_metrics_capsule.h"

// Records FirstContentfulPaint for `PreloadServingMetrics`
//
// See `PreloadServingMetrics` for more details.
class PreloadServingMetricsPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  PreloadServingMetricsPageLoadMetricsObserver();
  ~PreloadServingMetricsPageLoadMetricsObserver() override;

  // Not movable nor copyable.
  PreloadServingMetricsPageLoadMetricsObserver(
      PreloadServingMetricsPageLoadMetricsObserver&& other) = delete;
  PreloadServingMetricsPageLoadMetricsObserver& operator=(
      PreloadServingMetricsPageLoadMetricsObserver&& other) = delete;
  PreloadServingMetricsPageLoadMetricsObserver(
      const PreloadServingMetricsPageLoadMetricsObserver&) = delete;
  PreloadServingMetricsPageLoadMetricsObserver& operator=(
      const PreloadServingMetricsPageLoadMetricsObserver&) = delete;

 private:
  // PageLoadMetricsObserver implementation:
  const char* GetObserverName() const override;
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  PageLoadMetricsObserver::ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnEnterBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnRestoreFromBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      content::NavigationHandle* navigation_handle) override;

  void MaybeRecord();

  void RetrieveNavigationInitiatorLocationAndSrp(
      content::NavigationHandle* navigation_handle);

  std::unique_ptr<content::PreloadServingMetricsCapsule>
      preload_serving_metrics_capsule_;

  // TODO(https://crbug.com/517725655): There is a long term refactoring plan
  // for the PageLoadMetricsObserver. Please refer to the document fore more
  // details
  // https://docs.google.com/document/d/1d9k-YDEdT35LDVN3BkILyDVqZKlv-b6F0yV_OZRVIQk/edit?resourcekey=0-Jr0Dysk9Cabb0vZlG-ESXg&tab=t.0#heading=h.dygbqkif9aw5
  std::optional<std::string> navigation_initiator_string_;
  bool is_url_srp_ = false;
  // TODO(https://crbug.com/539388005): `PLMO::OnFirstContentfulPaintInPage()`
  // is not expected to be called between `PLMO::OnEnterBackForwardCache()` and
  // `PLMO::OnRestoreFromBackForwardCache()`, but currently it is happening. To
  // avoid this issue, `has_entered_bfcache_` is introduced and used in FCP
  // recording to avoid crash. Remove this variable once the problem is
  // resolved.
  bool has_entered_bfcache_ = false;
  bool has_restored_from_bfcache_ = false;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRELOAD_SERVING_METRICS_PAGE_LOAD_METRICS_OBSERVER_H_

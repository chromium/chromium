// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_SHARED_STORAGE_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_SHARED_STORAGE_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/numerics/checked_math.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

class SharedStoragePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  SharedStoragePageLoadMetricsObserver() = default;
  SharedStoragePageLoadMetricsObserver(
      const SharedStoragePageLoadMetricsObserver&) = delete;
  ~SharedStoragePageLoadMetricsObserver() override = default;

  SharedStoragePageLoadMetricsObserver& operator=(
      const SharedStoragePageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  const char* GetObserverName() const override;
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnSharedStorageWorkletHostCreated() override;
  void OnSharedStorageSelectURLCalled() override;

 private:
  void RecordSessionEndHistograms(
      const page_load_metrics::mojom::PageLoadTiming& main_frame_timing);

  base::CheckedNumeric<int> worklet_hosts_created_count_ = 0;
  base::CheckedNumeric<int> select_url_call_count_ = 0;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_SHARED_STORAGE_PAGE_LOAD_METRICS_OBSERVER_H_

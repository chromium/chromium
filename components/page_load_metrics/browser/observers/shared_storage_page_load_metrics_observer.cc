// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "components/page_load_metrics/browser/observers/shared_storage_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"

const char* SharedStoragePageLoadMetricsObserver::GetObserverName() const {
  static const char kName[] = "SharedStoragePageLoadMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SharedStoragePageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  // Receive events for primary pages.
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SharedStoragePageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // TODO(crbug.com/40222513): Handle Prerendering cases.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SharedStoragePageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Receive events for FencedFrames, as these will soon support shared storage.
  return CONTINUE_OBSERVING;
}

void SharedStoragePageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms(timing);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SharedStoragePageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms(timing);
  return STOP_OBSERVING;
}

void SharedStoragePageLoadMetricsObserver::OnSharedStorageWorkletHostCreated() {
  worklet_hosts_created_count_++;
}

void SharedStoragePageLoadMetricsObserver::OnSharedStorageSelectURLCalled() {
  select_url_call_count_++;
}

void SharedStoragePageLoadMetricsObserver::RecordSessionEndHistograms(
    const page_load_metrics::mojom::PageLoadTiming& main_frame_timing) {
  base::UmaHistogramCounts10000("Storage.SharedStorage.Worklet.NumPerPage",
                                worklet_hosts_created_count_.ValueOrDefault(
                                    std::numeric_limits<int>::max()));
  base::UmaHistogramCounts10000(
      "Storage.SharedStorage.Worklet.SelectURL.CallsPerPage",
      select_url_call_count_.ValueOrDefault(std::numeric_limits<int>::max()));
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_UMA_FILE_AND_DATA_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_UMA_FILE_AND_DATA_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

// Since UmaPageLoadMetricsObserver intentionally only records metrics for the
// http/https schemes, this observer was added to record metrics for the file
// and data schemes, used in some WebView apps.
class UmaFileAndDataPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // page_load_metrics::PageLoadMetricsObserver:
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy ShouldObserveScheme(const GURL& url) const override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  void RecordTimingHistograms();
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_UMA_FILE_AND_DATA_PAGE_LOAD_METRICS_OBSERVER_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_ZSTD_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_ZSTD_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {
// Exposed for tests.
extern const char kHistogramZstdFirstContentfulPaint[];
extern const char kHistogramZstdLargestContentfulPaint[];
extern const char kHistogramZstdParseStart[];

}  // namespace internal

class ZstdPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  ZstdPageLoadMetricsObserver();

  ZstdPageLoadMetricsObserver(const ZstdPageLoadMetricsObserver&) = delete;
  ZstdPageLoadMetricsObserver& operator=(const ZstdPageLoadMetricsObserver&) =
      delete;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;

  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;

  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;

  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnParseStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  void LogMetricsOnComplete();
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_ZSTD_PAGE_LOAD_METRICS_OBSERVER_H_

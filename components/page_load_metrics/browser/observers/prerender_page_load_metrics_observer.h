// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRERENDER_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRERENDER_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace blink {
namespace mojom {
enum class WebFeature : int32_t;
}  // namespace mojom
}  // namespace blink

// Records metrics relevant to prerendering. Currently it logs feature usage in
// normal page loads which, when if used during prerendering, may result in
// cancelling or freezing the prerender, to help estimate the effect on
// coverage.
class PrerenderPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  PrerenderPageLoadMetricsObserver() = default;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnStorageAccessed(const GURL& url,
                         const GURL& first_party_url,
                         bool blocked_by_policy,
                         page_load_metrics::StorageType access_type) override;

 private:
  void RecordFeatureUse(blink::mojom::WebFeature feature);

  bool did_fcp_ = false;
  bool did_local_storage_ = false;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRERENDER_PAGE_LOAD_METRICS_OBSERVER_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/declarative_performance_observer.h"

#include "base/feature_list.h"
#include "content/public/browser/navigation_handle.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/declarative_performance_observer.mojom.h"

namespace page_load_metrics {

DeclarativePerformanceObserver::DeclarativePerformanceObserver() = default;
DeclarativePerformanceObserver::~DeclarativePerformanceObserver() = default;

const char* DeclarativePerformanceObserver::GetObserverName() const {
  return "DeclarativePerformanceObserver";
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DeclarativePerformanceObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (!base::FeatureList::IsEnabled(
          network::features::kDeclarativePerformanceObserver)) {
    return STOP_OBSERVING;
  }

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DeclarativePerformanceObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DeclarativePerformanceObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DeclarativePerformanceObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  const network::mojom::DeclarativePerformanceObserverPolicy* policy =
      navigation_handle->GetDeclarativePerformanceObserverPolicy();

  if (!policy) {
    return STOP_OBSERVING;
  }

  // If reporting_endpoint is null, we cannot report events, so stop observing.
  // TODO(crbug.com/505208781): Consider using a default endpoint if
  // appropriate.
  if (!policy->reporting_endpoint) {
    return STOP_OBSERVING;
  }

  // If no entry types are specified, there is nothing to observe.
  if (policy->entry_types.empty()) {
    return STOP_OBSERVING;
  }

  return CONTINUE_OBSERVING;
}

}  // namespace page_load_metrics

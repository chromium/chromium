// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_DECLARATIVE_PERFORMANCE_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_DECLARATIVE_PERFORMANCE_OBSERVER_H_

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "services/network/public/mojom/declarative_performance_observer.mojom.h"

namespace page_load_metrics {

class DeclarativePerformanceObserver : public PageLoadMetricsObserver {
 public:
  DeclarativePerformanceObserver();
  ~DeclarativePerformanceObserver() override;

  DeclarativePerformanceObserver(const DeclarativePerformanceObserver&) =
      delete;
  DeclarativePerformanceObserver& operator=(
      const DeclarativePerformanceObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver implementation:
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

  const std::string& reporting_endpoint_for_testing() const {
    return reporting_endpoint_;
  }
  const base::flat_set<network::mojom::PerformanceEntryType>&
  enabled_types_for_testing() const {
    return enabled_types_;
  }
  const base::ListValue& buffered_entries_for_testing() const {
    return buffered_entries_;
  }

 private:
  void AddEntryToBuffer(base::DictValue entry);

  std::string reporting_endpoint_;
  base::flat_set<network::mojom::PerformanceEntryType> enabled_types_;
  base::ListValue buffered_entries_;
  bool started_in_foreground_ = false;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_DECLARATIVE_PERFORMANCE_OBSERVER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PERFORMANCE_MANAGER_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PERFORMANCE_MANAGER_METRICS_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom-forward.h"

class GURL;

namespace content {
class NavigationHandle;
}  // namespace content

namespace performance_manager {
class PageNode;
}

// Logs UMA about the timing of PerformanceManager's LoadingState transitions.
class PerformanceManagerMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // Whether the page was visible during the load.
  enum class Visibility {
    kUnknown,
    kBackground,
    kForeground,
    kMixed,
  };

  PerformanceManagerMetricsObserver();
  ~PerformanceManagerMetricsObserver() override;

  PerformanceManagerMetricsObserver(const PerformanceManagerMetricsObserver&) =
      delete;
  PerformanceManagerMetricsObserver& operator=(
      const PerformanceManagerMetricsObserver&) = delete;

  // Called from PerformanceManager's PageNodeObserver when the loading state
  // becomes kLoadedIdle.
  void OnPageNodeLoadedIdle(base::TimeTicks loaded_idle_time);

  // page_load_metrics::PageLoadMetricsObserver:

  // Methods that start a navigation.
  ObservePolicy OnStart(content::NavigationHandle*,
                        const GURL&,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(content::NavigationHandle*,
                                    const GURL&) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle*,
                                 const GURL&) override;

  // Methods that track visibility.
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming&) override;
  ObservePolicy OnShown() override;

  // Methods that log metrics.
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming&) override;
  // Called before destruction if the navigation committed.
  void OnComplete(const page_load_metrics::mojom::PageLoadTiming&) override;
  // Called before destruction if the navigation did not commit.
  void OnFailedProvisionalLoad(
      const page_load_metrics::FailedProvisionalLoadInfo&) override;

 private:
  // Logs UMA metrics if the load times are available. If `is_final` is true,
  // this will not be called again so it logs all metrics. Otherwise it only
  // logs metrics if both FirstContentfulPaint and LoadedIdle were observed.
  // Returns STOP_OBSERVING if it logs metrics since there's no need to
  // continue observing.
  ObservePolicy LogMetricsIfLoaded(bool is_final);

  // Starts watching for `page_node`, the PerformanceManager node for this page
  // load, to reach LoadedIdle.
  void WatchForLoadedIdle(
      base::WeakPtr<performance_manager::PageNode> page_node);

  SEQUENCE_CHECKER(sequence_checker_);

  // The visibility state of the page during this load.
  Visibility visibility_ GUARDED_BY_CONTEXT(sequence_checker_) =
      Visibility::kUnknown;

  // True if metrics were already logged, in which case further calls to
  // LogMetricsIfLoaded() will do nothing.
  bool logged_metrics_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // The time that the page load reached LoadedIdle, or 0 if this wasn't
  // observed yet.
  base::TimeTicks loaded_idle_time_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<PerformanceManagerMetricsObserver> weak_factory_{this};
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PERFORMANCE_MANAGER_METRICS_OBSERVER_H_

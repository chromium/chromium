// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PERFORMANCE_MANAGER_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PERFORMANCE_MANAGER_METRICS_OBSERVER_H_

#include <optional>

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
  // Returns the delta between the navigation start (which is reported from
  // renderers) and `time`, which should be a timestamp taken in this process
  // after starting navigation. Since timestamps from different processes can't
  // be guaranteed to be monotonically increasing, returns nullopt if the delta
  // would be negative.
  std::optional<base::TimeDelta> DeltaFromNavigationStartTime(
      base::TimeTicks time) const;

  // Logs NavigationToLoadedIdle and LCPToLoadedIdle UMA metrics if the load
  // times are available. Returns STOP_OBSERVING if all metrics are logged,
  // since there's no need to continue observing, or CONTINUE_OBSERVING to keep
  // waiting for load times.
  ObservePolicy LogMetricsIfAvailable();

  // Logs UMA metrics when a load is finished or abandoned. This logs the same
  // metrics as LogMetricsIfLoaded(), plus some of NavigationWithoutLoadedIdle,
  // LCPWithoutLoadedIdle, and LoadedIdleWithoutLCP for any load times that
  // aren't available.
  void LogFinalMetrics();

  // Starts watching for `page_node`, the PerformanceManager node for this page
  // load, to reach LoadedIdle.
  void WatchForLoadedIdle(
      base::WeakPtr<performance_manager::PageNode> page_node);

  SEQUENCE_CHECKER(sequence_checker_);

  // The visibility state of the page during this load.
  Visibility visibility_ GUARDED_BY_CONTEXT(sequence_checker_) =
      Visibility::kUnknown;

  // True if various metrics were already logged, to prevent logging them twice.
  bool logged_load_metrics_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool logged_lcp_metrics_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // The time that the page load reached LoadedIdle, or 0 if this wasn't
  // observed yet.
  base::TimeTicks loaded_idle_time_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<PerformanceManagerMetricsObserver> weak_factory_{this};
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PERFORMANCE_MANAGER_METRICS_OBSERVER_H_

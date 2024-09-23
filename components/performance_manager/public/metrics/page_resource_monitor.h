// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_PAGE_RESOURCE_MONITOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_PAGE_RESOURCE_MONITOR_H_

#include <map>
#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/resource_attribution/queries.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/system_cpu/cpu_sample.h"

namespace performance_manager::metrics {

// Periodically reports tab resource usage via UKM.
class PageResourceMonitor : public resource_attribution::QueryResultObserver,
                            public GraphOwnedDefaultImpl {
 public:
  // These values are logged to UKM. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // PageMeasurementAlgorithm in enums.xml.
  enum class PageMeasurementAlgorithm {
    kLegacy = 0,
    kEvenSplitAndAggregate = 1,
    kMaxValue = kEvenSplitAndAggregate,
  };

  // These values are logged to UKM. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // PageMeasurementBackgroundState in enums.xml.
  enum class PageMeasurementBackgroundState {
    kForeground = 0,
    kBackground = 1,
    kAudibleInBackground = 2,
    kBackgroundMixedAudible = 3,
    kMixedForegroundBackground = 4,
    kMaxValue = kMixedForegroundBackground,
  };

  // If `enable_system_cpu_probe` is false, `system_cpu_probe_` will be left
  // null. This is mainly intended for tests.
  explicit PageResourceMonitor(bool enable_system_cpu_probe = true);

  ~PageResourceMonitor() override;
  PageResourceMonitor(const PageResourceMonitor& other) = delete;
  PageResourceMonitor& operator=(const PageResourceMonitor&) = delete;

  // QueryResultObserver:
  void OnResourceUsageUpdated(
      const resource_attribution::QueryResultMap& results) override;

  // Returns the time between calls to OnResourceUsageUpdated(). Tests can
  // advance the mock clock by this amount to trigger metrics collection.
  base::TimeDelta GetCollectionDelayForTesting() const;

 private:
  // The percent CPU usage for each PageNode that was measured. This stores a
  // ResourceContext instead of a node pointer in case the PageNode is deleted
  // while taking asynchronous system CPU measurements.
  using PageCPUUsageMap =
      std::map<resource_attribution::ResourceContext, double>;

  // Helper class that converts CPUTimeResult to proportion of CPU used over a
  // fixed interval, and adds system CPU to the result.
  class CPUResultConverter;

  // Invoked asynchronously from OnResourceUsageUpdate() when both page and
  // system CPU measurements are ready. `results` contains the original query
  // results, with both CPU and memory measurements. `page_cpu_usage` is the
  // proportion of CPU usage calculated for each page from the original results,
  // and `system_cpu` is the overall system CPU usage.
  void OnPageResourceUsageResult(
      const resource_attribution::QueryResultMap& results,
      const PageCPUUsageMap& page_cpu_usage,
      std::optional<system_cpu::CpuSample> system_cpu);

  SEQUENCE_CHECKER(sequence_checker_);

  // Repeating query that triggers OnResourceUsageUpdated on a timer.
  resource_attribution::ScopedResourceUsageQuery resource_query_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Manages notificatoin subscriptions to `resource_query_`.
  resource_attribution::ScopedQueryObservation query_observation_{this};

  // Time of last PageResourceUsage collection.
  base::TimeTicks time_of_last_resource_usage_
      GUARDED_BY_CONTEXT(sequence_checker_) = base::TimeTicks::Now();

  // Helpers to convert CPU measurements for UMA.
  std::unique_ptr<CPUResultConverter> cpu_result_converter_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<CPUResultConverter> delayed_cpu_result_converter_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Graph being monitored.
  raw_ptr<Graph> graph_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  base::WeakPtrFactory<PageResourceMonitor> weak_factory_{this};
};

}  // namespace performance_manager::metrics

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_PAGE_RESOURCE_MONITOR_H_

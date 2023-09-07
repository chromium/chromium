// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_MONITOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_MONITOR_H_

#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"

namespace performance_manager {
class Graph;
class PageNode;
}  // namespace performance_manager

namespace performance_manager::resource_attribution {

// The Resource Attribution result and metadata structs described in
// https://bit.ly/resource-attribution-api#heading=h.k8fjwkwxxdj6.
// TODO(crbug.com/1471683): Implement the rest of the ResourceAttribution query
// API, make it the interface to CPUMeasurementMonitor.

struct ResourceUsageResultMetadata {
  // The time this measurement was taken.
  base::TimeTicks measurement_time;
};

struct CPUTimeResult {
  ResourceUsageResultMetadata metadata;

  // The time that Resource Attribution started monitoring the CPU usage of this
  // context.
  base::TimeTicks start_time;

  // Total time the context spent on CPU between `start_time` and
  // `metadata.measurement_time`.
  //
  // `cumulative_cpu` / (`metadata.measurement_time` - `start_time`)
  // gives percentage of CPU used as a fraction in the range 0% to 100% *
  // SysInfo::NumberOfProcessors(), the same as
  // ProcessMetrics::GetPlatformIndependentCPUUsage().
  base::TimeDelta cumulative_cpu;
};

// Periodically collect CPU usage from process nodes.
//
// Note: this can't store measurements in NodeAttachedData because the final CPU
// measurement for a node may not arrive until after it's removed from the
// graph. So this is not a decorator as defined in
// components/performance_manager/README.md.
class CPUMeasurementMonitor : public ProcessNode::ObserverDefaultImpl {
 public:
  // A shim to request CPU measurements for a process. A new
  // CPUMeasurementDelegate object will be created for each ProcessNode to be
  // measured. Can be overridden for testing by passing a factory callback to
  // SetCPUMeasurementDelegateFactoryForTesting().
  class CPUMeasurementDelegate {
   public:
    using FactoryCallback =
        base::RepeatingCallback<std::unique_ptr<CPUMeasurementDelegate>(
            const ProcessNode*)>;

    CPUMeasurementDelegate() = default;
    virtual ~CPUMeasurementDelegate() = default;

    // Requests CPU usage for the process. This is [[nodiscard]] to match the
    // semantics of ProcessMetrics::GetCumulativeCPUUsage().
    [[nodiscard]] virtual base::TimeDelta GetCumulativeCPUUsage() = 0;
  };

  CPUMeasurementMonitor();
  ~CPUMeasurementMonitor() override;

  CPUMeasurementMonitor(const CPUMeasurementMonitor& other) = delete;
  CPUMeasurementMonitor& operator=(const CPUMeasurementMonitor&) = delete;

  // The given `factory_callback` will be called to create a
  // CPUMeasurementDelegate for each ProcessNode to be measured.
  void SetCPUMeasurementDelegateFactoryForTesting(
      CPUMeasurementDelegate::FactoryCallback factory_callback);

  // Starts monitoring CPU usage for all renderer ProcessNode's in `graph`.
  void StartMonitoring(Graph* graph);

  // Stops monitoring ProcessNode's in `graph`.
  void StopMonitoring(Graph* graph);

  // Updates the CPU measurements for each ProcessNode being tracked and returns
  // the estimated CPU usage of each frame and worker in those processes.
  // TODO(crbug.com/1471683): Also store PageContext's in this map, replacing
  // EstimatePageCPUUsage().
  std::map<ResourceContext, CPUTimeResult> UpdateAndGetCPUMeasurements();

  // Helper to estimate the CPU usage of a PageNode given the estimates for all
  // frames and workers.
  static base::TimeDelta EstimatePageCPUUsage(
      const PageNode* page_node,
      const std::map<ResourceContext, CPUTimeResult>& cpu_usage_map);

  // ProcessNode::Observer:
  void OnProcessLifetimeChange(const ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;

 private:
  friend class CPUMeasurementMonitorTest;

  // Holds a CPUMeasurementDelegate object to measure CPU usage and metadata
  // about the measurements. One CPUMeasurement will be created for each
  // ProcessNode being measured.
  class CPUMeasurement {
   public:
    explicit CPUMeasurement(std::unique_ptr<CPUMeasurementDelegate> delegate);
    ~CPUMeasurement();

    // Move-only type.
    CPUMeasurement(const CPUMeasurement& other) = delete;
    CPUMeasurement& operator=(const CPUMeasurement& other) = delete;
    CPUMeasurement(CPUMeasurement&& other);
    CPUMeasurement& operator=(CPUMeasurement&& other);

    // Returns the most recent measurement that was taken during
    // MeasureAndDistributeCPUUsage().
    base::TimeDelta most_recent_measurement() const {
      return most_recent_measurement_;
    }

    // Measures the CPU usage of `process_node`, calculates the change in CPU
    // usage over the period (`last_measurement_time_` ... now], and allocates
    // the results to frames and workers in the process.
    void MeasureAndDistributeCPUUsage(
        const ProcessNode* process_node,
        std::map<ResourceContext, CPUTimeResult>& cpu_usage_map);

   private:
    std::unique_ptr<CPUMeasurementDelegate> delegate_;

    // The most recent CPU measurement that was taken.
    base::TimeDelta most_recent_measurement_;

    // Last time CPU measurements were taken (for calculating the total length
    // of a measurement interval).
    base::TimeTicks last_measurement_time_;
  };

  // Creates a CPUMeasurement tracker for `process_node` and adds it to
  // `cpu_measurement_map_`.
  void MonitorCPUUsage(const ProcessNode* process_node);

  // Updates the CPU measurements for each ProcessNode being tracked, adding
  // the estimated CPU usage of each frame and worker in those processes since
  // the last time UpdateCPUMeasurements() was called to `measurement_results_`.
  void UpdateCPUMeasurements();

  SEQUENCE_CHECKER(sequence_checker_);

  // Map of process nodes to ProcessMetrics used to measure CPU usage.
  std::map<const ProcessNode*, CPUMeasurement> cpu_measurement_map_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // A map from resource contexts to the estimated CPU usage of each.
  std::map<ResourceContext, CPUTimeResult> measurement_results_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Callback that will be invoked to create CPUMeasurementDelegate objects for
  // each ProcessNode being measured.
  CPUMeasurementDelegate::FactoryCallback cpu_measurement_delegate_factory_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // True if StartMonitoring() was called and StopMonitoring() was not.
  bool is_monitoring_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_MONITOR_H_

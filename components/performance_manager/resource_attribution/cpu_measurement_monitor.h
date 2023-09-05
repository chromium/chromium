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

  // A map from FrameNode's or WorkerNode's to the estimated CPU usage of each.
  // The estimate is a fraction in the range 0% to 100% *
  // SysInfo::NumberOfProcessors(), the same as the return value of
  // ProcessMetrics::GetPlatformIndependentCPUUsage().
  // TODO(crbug.com/1471683): Also store PageContext's in this map, replacing
  // EstimatePageCPUUsage().
  using CPUUsageMap = std::map<ResourceContext, double>;

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
  // the estimated CPU usage of each frame and worker in those processes since
  // the last time UpdateCPUMeasurements() was called.
  CPUUsageMap UpdateCPUMeasurements();

  // Helper to estimate the CPU usage of a PageNode given the estimates for all
  // frames and workers.
  static double EstimatePageCPUUsage(const PageNode* page_node,
                                     const CPUUsageMap& cpu_usage_map);

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

    // Measures the CPU usage of `process_node`, calculates the proportion of
    // usage over the period `measurement_interval_end` -
    // `measurement_interval_start`, and allocates the results to frames and
    // workers in the process.
    void MeasureAndDistributeCPUUsage(
        const ProcessNode* process_node,
        base::TimeTicks measurement_interval_start,
        base::TimeTicks measurement_interval_end,
        CPUUsageMap& cpu_usage_map);

   private:
    std::unique_ptr<CPUMeasurementDelegate> delegate_;
    base::TimeDelta most_recent_measurement_;
  };

  // Creates a CPUMeasurement tracker for `process_node` and adds it to
  // `cpu_measurement_map_`.
  void MonitorCPUUsage(const ProcessNode* process_node);

  SEQUENCE_CHECKER(sequence_checker_);

  // Map of process nodes to ProcessMetrics used to measure CPU usage.
  std::map<const ProcessNode*, CPUMeasurement> cpu_measurement_map_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Last time CPU measurements were taken (for calculating the total length of
  // a measurement interval).
  base::TimeTicks last_measurement_time_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Callback that will be invoked to create CPUMeasurementDelegate objects for
  // each ProcessNode being measured.
  CPUMeasurementDelegate::FactoryCallback cpu_measurement_delegate_factory_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_MONITOR_H_

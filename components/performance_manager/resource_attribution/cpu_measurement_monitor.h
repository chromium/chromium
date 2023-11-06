// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_MONITOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_MONITOR_H_

#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/resource_attribution/attribution_helpers.h"
#include "components/performance_manager/public/resource_attribution/cpu_measurement_delegate.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/resource_attribution/graph_change.h"

namespace performance_manager {
class Graph;
}

namespace performance_manager::resource_attribution {

// Periodically collect CPU usage from process nodes.
//
// Note: this can't store measurements in NodeAttachedData because the final CPU
// measurement for a node may not arrive until after it's removed from the
// graph. So this is not a decorator as defined in
// components/performance_manager/README.md
class CPUMeasurementMonitor : public FrameNode::ObserverDefaultImpl,
                              public ProcessNode::ObserverDefaultImpl,
                              public WorkerNode::ObserverDefaultImpl {
 public:
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

  // Stops monitoring the graph.
  void StopMonitoring();

  // Returns true if currently monitoring.
  bool IsMonitoring() const;

  // Updates the CPU measurements for each ProcessNode being tracked and returns
  // the estimated CPU usage of each frame and worker in those processes, and
  // all pages containing them.
  std::map<ResourceContext, CPUTimeResult> UpdateAndGetCPUMeasurements();

  // FrameNode::Observer:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;

  // ProcessNode::Observer:
  void OnProcessLifetimeChange(const ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;

  // WorkerNode::Observer:
  void OnWorkerNodeAdded(const WorkerNode* worker_node) override;
  void OnBeforeWorkerNodeRemoved(const WorkerNode* worker_node) override;
  void OnClientFrameAdded(const WorkerNode* worker_node,
                          const FrameNode* client_frame_node) override;
  void OnBeforeClientFrameRemoved(const WorkerNode* worker_node,
                                  const FrameNode* client_frame_node) override;
  void OnClientWorkerAdded(const WorkerNode* worker_node,
                           const WorkerNode* client_worker_node) override;
  void OnBeforeClientWorkerRemoved(
      const WorkerNode* worker_node,
      const WorkerNode* client_worker_node) override;

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
    // the results to frames and workers in the process using
    // SplitResourceAmongFramesAndWorkers(). The new CPU usage in this
    // measurement is added to `measurement_deltas`.
    void MeasureAndDistributeCPUUsage(
        const ProcessNode* process_node,
        const NodeSplitSet& extra_nodes,
        const NodeSplitSet& nodes_to_skip,
        std::map<ResourceContext, CPUTimeResult>& measurement_deltas);

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

  // Updates the CPU measurement for all ProcessNodes in `cpu_measurement_map_`.
  // Adds the estimated CPU usage of each frame and worker since the last time
  // the process was measured to `measurement_results_`.
  void UpdateAllCPUMeasurements();

  // Updates the CPU measurement for `process_node`. Adds the estimated CPU
  // usage of each frame and worker since the last time the process was measured
  // to `measurement_results_`. `graph_change` is the event that triggered the
  // measurement or NoGraphChange if it wasn't triggered due to a graph topology
  // change.
  void UpdateCPUMeasurements(const ProcessNode* process_node,
                             GraphChange graph_change = NoGraphChange());

  // Adds the new measurements in `measurement_deltas` to
  // `measurement_results_`. `graph_change` is the event that triggered the
  // measurement or NoGraphChange if it wasn't triggered due to a graph topology
  // change.
  void ApplyMeasurementDeltas(
      const std::map<ResourceContext, CPUTimeResult>& measurement_deltas,
      GraphChange graph_change = NoGraphChange());

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

  // Graph being monitored. This will be only be set if StartMonitoring() was
  // called and StopMonitoring() was not.
  raw_ptr<Graph> graph_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_MONITOR_H_

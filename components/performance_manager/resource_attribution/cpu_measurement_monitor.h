// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_MONITOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_MONITOR_H_

#include <map>
#include <optional>
#include <set>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/resource_attribution/cpu_measurement_delegate.h"
#include "components/performance_manager/public/resource_attribution/origin_in_browsing_instance_context.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/resource_attribution/graph_change.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "components/performance_manager/resource_attribution/query_params.h"

namespace resource_attribution {

class ScopedCPUTimeResult;

// Periodically collect CPU usage from process nodes.
//
// Note: this can't store measurements in NodeAttachedData because the final CPU
// measurement for a node may not arrive until after it's removed from the
// graph. So this is not a decorator as defined in
// components/performance_manager/README.md
class CPUMeasurementMonitor
    : public FrameNodeObserver,
      public PageNodeObserver,
      public ProcessNodeObserver,
      public WorkerNodeObserver,
      public performance_manager::NodeDataDescriberDefaultImpl {
 public:
  CPUMeasurementMonitor();
  ~CPUMeasurementMonitor() override;

  CPUMeasurementMonitor(const CPUMeasurementMonitor& other) = delete;
  CPUMeasurementMonitor& operator=(const CPUMeasurementMonitor&) = delete;

  // The given `factory` will be used to create a CPUMeasurementDelegate for
  // each ProcessNode to be measured.
  void SetDelegateFactoryForTesting(CPUMeasurementDelegate::Factory* factory);

  // Starts monitoring CPU usage for all renderer ProcessNode's in `graph`.
  void StartMonitoring(Graph* graph);

  // Stops monitoring the graph.
  void StopMonitoring();

  // Returns true if currently monitoring.
  bool IsMonitoring() const;

  // Creates an empty list in `dead_context_results_` to store results from
  // deleted nodes for `query_id`.
  void RepeatingQueryStarted(internal::QueryId query_id);

  // Removes all `dead_context_results_` that are waiting for `query_id`.
  void RepeatingQueryStopped(internal::QueryId query_id);

  // Returns true if `dead_context_results_` contains `query_id`.
  bool IsTrackingQueryForTesting(internal::QueryId query_id) const;

  // Returns the total number of ResourceContexts tracked in
  // `dead_context_results_`. Contexts can be tracked more than once.
  size_t GetDeadContextCountForTesting() const;

  // Updates the CPU measurements for each ProcessNode being tracked and returns
  // the estimated CPU usage of each frame and worker in those processes, and
  // all other resource contexts containing them. Each QueryResults object will
  // contain a CPUTimeResult. `query_id` is the ID of the
  // ScopedResourceUsageQuery that made the request, or nullopt for a one-shot
  // query.
  //
  // When consecutive measurements for the same `query_id` contain a
  // CPUTimeResult for a given context, the cumulative CPU usage isn't reset
  // between the two measurements, even if the context was transiently dead (a
  // dead OriginInBrowsingInstanceContext can be revived). However, if a
  // measurement doesn't contain a CPUTimeResult for a context, the next
  // measurement may not include CPU usage that was previously reported for that
  // context.
  QueryResultMap UpdateAndGetCPUMeasurements(
      std::optional<internal::QueryId> query_id = std::nullopt);

  // Logs metrics on CPUMeasurementMonitor's memory usage to UMA.
  void RecordMemoryMetrics();

  // FrameNode::Observer:
  void OnBeforeFrameNodeAdded(
      const FrameNode* frame_node,
      const FrameNode* pending_parent_frame_node,
      const PageNode* pending_page_node,
      const ProcessNode* pending_process_node,
      const FrameNode* pending_parent_or_outer_document_or_embedder) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnOriginChanged(
      const FrameNode* frame_node,
      const std::optional<url::Origin>& previous_value) override;

  // PageNode::Observer:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;

  // ProcessNode::Observer:
  void OnProcessNodeAdded(const ProcessNode* process_node) override;
  void OnProcessLifetimeChange(const ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;
  void OnPriorityChanged(const ProcessNode* process_node,
                         base::TaskPriority previous_value) override;

  // WorkerNode::Observer:
  void OnBeforeWorkerNodeAdded(
      const WorkerNode* worker_node,
      const ProcessNode* pending_process_node) override;
  void OnBeforeWorkerNodeRemoved(const WorkerNode* worker_node) override;
  void OnBeforeClientFrameAdded(const WorkerNode* worker_node,
                                const FrameNode* client_frame_node) override;
  void OnBeforeClientFrameRemoved(const WorkerNode* worker_node,
                                  const FrameNode* client_frame_node) override;
  void OnBeforeClientWorkerAdded(const WorkerNode* worker_node,
                                 const WorkerNode* client_worker_node) override;
  void OnBeforeClientWorkerRemoved(
      const WorkerNode* worker_node,
      const WorkerNode* client_worker_node) override;

  // NodeDataDescriber:
  base::Value::Dict DescribeFrameNodeData(const FrameNode* node) const override;
  base::Value::Dict DescribePageNodeData(const PageNode* node) const override;
  base::Value::Dict DescribeProcessNodeData(
      const ProcessNode* node) const override;
  base::Value::Dict DescribeWorkerNodeData(
      const WorkerNode* node) const override;

 private:
  friend class CPUMeasurementMonitorTest;
  friend class ScopedCPUTimeResult;

  // Creates a CPUMeasurementData to track the CPU usage of `process_node`.
  void MonitorCPUUsage(const ProcessNode* process_node);

  // Updates the CPU measurement for all ProcessNodes being monitored using
  // MeasureAndDistributeCPUUsage(). Adds the estimated CPU usage of each frame
  // and worker since the last time the process was measured to a
  // ScopedCPUTimeResult.
  void UpdateAllCPUMeasurements();

  // Updates the CPU measurement for `process_node` using
  // MeasureAndDistributeCPUUsage(). Adds the estimated CPU usage of each frame
  // and worker since the last time the process was measured to a
  // ScopedCPUTimeResult. `graph_change` is the event that triggered the
  // measurement or NoGraphChange if it wasn't triggered due to a graph change.
  void UpdateCPUMeasurements(const ProcessNode* process_node,
                             GraphChange graph_change = NoGraphChange());

  // Retrieves the `CPUTimeResult` for `context`. If no result exists, returns a
  // reference to a null pointer, which the caller must initialize.
  scoped_refptr<ScopedCPUTimeResult>& GetResultPtr(
      const ResourceContext& context);

  // Adds the new measurements in `measurement_deltas` to ScopedCPUTimeResults.
  // `graph_change` is the event that triggered the measurement or NoGraphChange
  // if it wasn't triggered due to a graph change.
  void ApplyMeasurementDeltas(
      const std::map<ResourceContext, CPUTimeResult>& measurement_deltas,
      GraphChange graph_change = NoGraphChange());

  // Adds the measurement in `delta` to the result for `context`. The start time
  // of `delta` must follow the end time of the result. Used for adding
  // successive measurements of process, frame and worker contexts, so the
  // algorithm in the metadata for the result should match that of `delta`.
  // There may be gaps between deltas, such as if a process died and was
  // restarted.
  void ApplySequentialDelta(const ResourceContext& context,
                            const CPUTimeResult& delta);

  // Adds the measurement in `delta` to the result for `context`. Delta may
  // start before the result or end after it. Used for adding frame and worker
  // measurements to page contexts, since the frames and workers can be added in
  // any order.
  void ApplyOverlappingDelta(const ResourceContext& context,
                             const CPUTimeResult& delta);

  // Moves `result_ptr` into `dead_context_results_`. `result_ptr` is passed by
  // move to ensure the caller drops its reference to the live result.
  void SaveFinalMeasurement(scoped_refptr<ScopedCPUTimeResult>&& result_ptr);

  // Returns all `OriginInBrowsingInstanceContext`s associated with live frame
  // or worker contexts.
  std::set<OriginInBrowsingInstanceContext>
  GetLiveOriginInBrowsingInstanceContexts();

  // Measures the CPU usage of `process_node`, calculates the change in CPU
  // usage over the period (`last_measurement_time_` ... now], and allocates
  // the results to frames and workers in the process using
  // SplitResourceAmongFramesAndWorkers(). The new CPU usage in this
  // measurement is added to `measurement_deltas`.
  static void MeasureAndDistributeCPUUsage(
      const ProcessNode* process_node,
      GraphChange graph_change,
      std::map<ResourceContext, CPUTimeResult>& measurement_deltas);

  SEQUENCE_CHECKER(sequence_checker_);

  // A map from `OriginInBrowsingInstanceContext`s to the estimated CPU usage of
  // each, updated whenever UpdateCPUMeasurements() is called. An
  // `OriginInBrowsingInstanceContext` is considered live as long as it's in
  // this map. Results for other context types are held in NodeInlineData in
  // live PM nodes.
  std::map<OriginInBrowsingInstanceContext, scoped_refptr<ScopedCPUTimeResult>>
      origin_results_ GUARDED_BY_CONTEXT(sequence_checker_);

  // A map of non-owning pointers to all `ScopedCPUTimeResult` instances
  // associated with `OriginInBrowsingInstanceContext`.
  std::map<OriginInBrowsingInstanceContext, raw_ptr<ScopedCPUTimeResult>>
      weak_origin_results_ GUARDED_BY_CONTEXT(sequence_checker_);

  // CPU time results for dead contexts retained by ScopedResourceUsageQuery.
  //
  // TODO(crbug.com/333112603): Not every ScopedResourceUsageQuery wants results
  // for each context. Currently all results are reported to every query, and
  // QueryScheduler filters out the ones the query doesn't need. For efficiency
  // CPUMeasurementMonitor should only report the results the query needs.
  struct DeadContextResults {
    DeadContextResults();
    ~DeadContextResults();

    // Move-only.
    DeadContextResults(DeadContextResults&&);
    DeadContextResults& operator=(DeadContextResults&&);
    DeadContextResults(const DeadContextResults&) = delete;
    DeadContextResults& operator=(const DeadContextResults&) = delete;

    // Results for dead contexts to report in the next measurement for this
    // query.
    //
    // When a context dies, its result is added to the `to_report` set of all
    // live queries.
    std::set<scoped_refptr<ScopedCPUTimeResult>> to_report;

    // Results kept alive until the next measurement for this query, in case the
    // associated context is revived. If a context is revived while this set has
    // a reference to its last result, GetResultPtr() will retrieve it instead
    // of creating a new one.
    //
    // When a measurement for a query contains a result for a dead
    // `OriginInBrowsingInstanceContext`, the result is kept in the `kept_alive`
    // set of the query until the next measurement. This ensures that when
    // consecutive measurements for a query contain a result for a given
    // context, the cumulative CPU usage isn't reset between the two
    // measurements, even if the context was transiently dead.
    std::set<scoped_refptr<ScopedCPUTimeResult>> kept_alive;
  };
  std::map<internal::QueryId, DeadContextResults> dead_context_results_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Factory that creates CPUMeasurementDelegate objects for each ProcessNode
  // being measured.
  raw_ptr<CPUMeasurementDelegate::Factory> delegate_factory_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Graph being monitored. This will be only be set if StartMonitoring() was
  // called and StopMonitoring() was not.
  raw_ptr<Graph> graph_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
};

// Ref-counted class which holds a `ResourceContext` and `CPUTimeResult`.
//
// If the context is an `OriginInBrowsingInstanceContext`, the constructor and
// destructor maintain a non-owning pointer to `this` in `weak_origin_results_`,
// allowing GetOriginInBrowsingInstanceResultPtr() to reuse a result that is
// still referenced by `dead_context_results_`.
class ScopedCPUTimeResult : public base::RefCounted<ScopedCPUTimeResult> {
 public:
  ScopedCPUTimeResult(CPUMeasurementMonitor* monitor,
                      const ResourceContext& context,
                      const CPUTimeResult& result);

  ScopedCPUTimeResult(const ScopedCPUTimeResult&) = delete;
  ScopedCPUTimeResult& operator=(const ScopedCPUTimeResult&) = delete;

  CPUTimeResult& result() { return result_; }
  const ResourceContext& context() const { return context_; }
  size_t EstimateMemoryUsage() const;

 private:
  friend class base::RefCounted<ScopedCPUTimeResult>;

  ~ScopedCPUTimeResult();

  const raw_ptr<CPUMeasurementMonitor> monitor_;
  const ResourceContext context_;
  CPUTimeResult result_;
};

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_MONITOR_H_

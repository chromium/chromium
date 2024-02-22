// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/cpu_measurement_monitor.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/sequence_checker.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/node_data_describer_util.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/resource_attribution/attribution_helpers.h"
#include "components/performance_manager/public/resource_attribution/frame_context.h"
#include "components/performance_manager/public/resource_attribution/worker_context.h"
#include "components/performance_manager/resource_attribution/graph_change.h"
#include "components/performance_manager/resource_attribution/worker_client_pages.h"
#include "content/public/common/process_type.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace resource_attribution {

namespace {

// Returns true if `resource_context` refers to a node that's been removed from
// the PM graph.
bool IsDeadContext(const ResourceContext& resource_context) {
  return absl::visit(base::Overloaded{
                         [](const FrameContext& context) {
                           return context.GetFrameNode() == nullptr;
                         },
                         [](const PageContext& context) {
                           return context.GetPageNode() == nullptr;
                         },
                         [](const ProcessContext& context) {
                           return context.GetProcessNode() == nullptr;
                         },
                         [](const WorkerContext& context) {
                           return context.GetWorkerNode() == nullptr;
                         },
                     },
                     resource_context);
}

// CHECK's that `result` obeys all constraints: the start and end timestamps
// form a positive interval and `cumulative_cpu` will fit into that interval.
void ValidateCPUTimeResult(const CPUTimeResult& result) {
  // Start and end must form a valid interval.
  CHECK(!result.metadata.measurement_time.is_null());
  CHECK(!result.start_time.is_null());
  const base::TimeDelta interval =
      result.metadata.measurement_time - result.start_time;
  CHECK(interval.is_positive());

  CHECK(!result.cumulative_cpu.is_negative());
}

}  // namespace

CPUMeasurementMonitor::CPUMeasurementMonitor()
    : delegate_factory_(CPUMeasurementDelegate::GetDefaultFactory()) {}

CPUMeasurementMonitor::~CPUMeasurementMonitor() {
  if (graph_) {
    StopMonitoring();
  }
  CHECK(!graph_);
}

void CPUMeasurementMonitor::SetDelegateFactoryForTesting(
    CPUMeasurementDelegate::Factory* factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ensure that all CPU measurements use the same delegate.
  CHECK(cpu_measurement_map_.empty());
  CHECK(factory);
  delegate_factory_ = factory;
}

void CPUMeasurementMonitor::StartMonitoring(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!graph_);
  graph_ = graph;
  graph_->AddFrameNodeObserver(this);
  graph_->AddProcessNodeObserver(this);
  graph_->AddWorkerNodeObserver(this);

  // Start monitoring CPU usage for all existing processes. Can't read their CPU
  // usage until they have a pid assigned.
  graph_->VisitAllProcessNodes([this](const ProcessNode* process_node) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (delegate_factory_->ShouldMeasureProcess(process_node)) {
      MonitorCPUUsage(process_node);
    }
    return true;
  });
}

void CPUMeasurementMonitor::StopMonitoring() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(graph_);
  cpu_measurement_map_.clear();
  graph_->RemoveFrameNodeObserver(this);
  graph_->RemoveProcessNodeObserver(this);
  graph_->RemoveWorkerNodeObserver(this);
  graph_ = nullptr;
}

bool CPUMeasurementMonitor::IsMonitoring() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return graph_;
}

QueryResultMap CPUMeasurementMonitor::UpdateAndGetCPUMeasurements() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateAllCPUMeasurements();
  QueryResultMap results;
  for (const auto& [context, result] : measurement_results_) {
    ValidateCPUTimeResult(result);
    results.emplace(context, QueryResults{.cpu_time_result = result});
  }

  // After a node is deleted its measurements should only be kept until used
  // for one query result. This was that query.
  std::erase_if(measurement_results_,
                [](const std::pair<ResourceContext, CPUTimeResult>& entry) {
                  return IsDeadContext(entry.first);
                });

  return results;
}

void CPUMeasurementMonitor::OnFrameNodeAdded(const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Take a measurement of the process CPU usage *before* this node was added.
  // This is safe because frames should only be added after their containing
  // process has started.
  UpdateCPUMeasurements(frame_node->GetProcessNode(),
                        GraphChangeAddFrame(frame_node));
}

void CPUMeasurementMonitor::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Take a measurement of the process CPU usage, including this frame, so that
  // its final CPU usage is attributed to it before it's removed.
  UpdateCPUMeasurements(frame_node->GetProcessNode(),
                        GraphChangeRemoveFrame(frame_node));
}

void CPUMeasurementMonitor::OnProcessLifetimeChange(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!graph_) {
    // Not monitoring CPU usage yet.
    CHECK(cpu_measurement_map_.empty());
    return;
  }
  if (delegate_factory_->ShouldMeasureProcess(process_node)) {
    MonitorCPUUsage(process_node);
  }
}

void CPUMeasurementMonitor::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cpu_measurement_map_.erase(process_node);
}

void CPUMeasurementMonitor::OnWorkerNodeAdded(const WorkerNode* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Take a measurement of the process CPU usage *before* this node was added.
  // This is safe because workers should only be added after their containing
  // process has started.
  UpdateCPUMeasurements(worker_node->GetProcessNode(),
                        GraphChangeAddWorker(worker_node));
}

void CPUMeasurementMonitor::OnBeforeWorkerNodeRemoved(
    const WorkerNode* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Take a measurement of the process CPU usage, including this node, so that
  // its final CPU usage is attributed to it before it's removed.
  UpdateCPUMeasurements(worker_node->GetProcessNode(),
                        GraphChangeRemoveWorker(worker_node));
}

void CPUMeasurementMonitor::OnClientFrameAdded(
    const WorkerNode* worker_node,
    const FrameNode* client_frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Take a measurement of the process CPU usage *before* this worker gained a
  // client. The CPU measurement will be distributed to pages that were clients
  // of this worker, not including the new client.
  UpdateCPUMeasurements(
      worker_node->GetProcessNode(),
      GraphChangeAddClientFrameToWorker(worker_node, client_frame_node));
}

void CPUMeasurementMonitor::OnBeforeClientFrameRemoved(
    const WorkerNode* worker_node,
    const FrameNode* client_frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Take a measurement of the process CPU usage *before* this worker lost a
  // client. The CPU measurement will be distributed to pages that were
  // clients of this worker, including the old client.
  UpdateCPUMeasurements(
      worker_node->GetProcessNode(),
      GraphChangeRemoveClientFrameFromWorker(worker_node, client_frame_node));
}

void CPUMeasurementMonitor::OnClientWorkerAdded(
    const WorkerNode* worker_node,
    const WorkerNode* client_worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Take a measurement of the process CPU usage *before* this worker gained a
  // client. The CPU measurement will be distributed to pages that were clients
  // of this worker, not including the new client.
  UpdateCPUMeasurements(
      worker_node->GetProcessNode(),
      GraphChangeAddClientWorkerToWorker(worker_node, client_worker_node));
}

void CPUMeasurementMonitor::OnBeforeClientWorkerRemoved(
    const WorkerNode* worker_node,
    const WorkerNode* client_worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Take a measurement of the process CPU usage *before* this worker lost a
  // client. The CPU measurement will be distributed to pages that were clients
  // of this worker, including the old client.
  UpdateCPUMeasurements(
      worker_node->GetProcessNode(),
      GraphChangeRemoveClientWorkerFromWorker(worker_node, client_worker_node));
}

base::Value::Dict CPUMeasurementMonitor::DescribeFrameNodeData(
    const FrameNode* node) const {
  return DescribeContextData(node->GetResourceContext());
}

base::Value::Dict CPUMeasurementMonitor::DescribePageNodeData(
    const PageNode* node) const {
  return DescribeContextData(node->GetResourceContext());
}

base::Value::Dict CPUMeasurementMonitor::DescribeProcessNodeData(
    const ProcessNode* node) const {
  return DescribeContextData(node->GetResourceContext());
}

base::Value::Dict CPUMeasurementMonitor::DescribeWorkerNodeData(
    const WorkerNode* node) const {
  return DescribeContextData(node->GetResourceContext());
}

void CPUMeasurementMonitor::MonitorCPUUsage(const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If a process crashes and is restarted, a new process can be assigned to the
  // same ProcessNode (and the same ProcessContext). When that happens
  // OnProcessLifetimeChange will call MonitorCPUUsage again for the same node,
  // creating a new CPUMeasurement that starts measuring the new process from 0.
  // ApplyMeasurementDeltas will add the new measurements and the old
  // measurements in the same ProcessContext.
  cpu_measurement_map_.insert_or_assign(
      process_node, CPUMeasurement(delegate_factory_->CreateDelegateForProcess(
                        process_node)));
}

void CPUMeasurementMonitor::UpdateAllCPUMeasurements() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Must call StartMonitoring() before getting measurements.
  CHECK(graph_);

  // Update CPU metrics, attributing the cumulative CPU of each process to its
  // frames and workers.
  std::map<ResourceContext, CPUTimeResult> measurement_deltas;
  for (auto& [node, cpu_measurement] : cpu_measurement_map_) {
    cpu_measurement.MeasureAndDistributeCPUUsage(node, {}, {},
                                                 measurement_deltas);
  }
  ApplyMeasurementDeltas(measurement_deltas);
}

void CPUMeasurementMonitor::UpdateCPUMeasurements(
    const ProcessNode* process_node,
    GraphChange graph_change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Must call StartMonitoring() before getting measurements.
  CHECK(graph_);
  CHECK(process_node);

  // Don't distribute measurements to nodes that are being added to the graph.
  // The current measurement covers the time before the node was added.
  NodeSplitSet nodes_to_skip;

  // Include nodes that are being removed from the graph. They may not be
  // reachable through visitors at this point, but the current measurement
  // covers the time before they were removed.
  // TODO(https://crbug.com/1481676): Make the graph state consistent in
  // OnBefore*NodeRemoved so `extra_nodes` isn't needed.
  NodeSplitSet extra_nodes;

  absl::visit(base::Overloaded{
                  [&nodes_to_skip](GraphChangeAddFrame change) {
                    nodes_to_skip.insert(change.frame_node.get());
                  },
                  [&nodes_to_skip](GraphChangeAddWorker change) {
                    nodes_to_skip.insert(change.worker_node.get());
                  },
                  [&extra_nodes](GraphChangeRemoveFrame change) {
                    extra_nodes.insert(change.frame_node.get());
                  },
                  [&extra_nodes](GraphChangeRemoveWorker change) {
                    extra_nodes.insert(change.worker_node.get());
                  },
                  [](auto change) {
                    // Do nothing.
                  },
              },
              graph_change);

  // Update CPU metrics, attributing the cumulative CPU of the process to its
  // frames and workers.
  std::map<ResourceContext, CPUTimeResult> measurement_deltas;
  const auto it = cpu_measurement_map_.find(process_node);
  if (it == cpu_measurement_map_.end()) {
    // In tests, FrameNode's can be added to mock processes that don't have a
    // PID so aren't being monitored.
    return;
  }
  it->second.MeasureAndDistributeCPUUsage(it->first, extra_nodes, nodes_to_skip,
                                          measurement_deltas);
  ApplyMeasurementDeltas(measurement_deltas, graph_change);
}

void CPUMeasurementMonitor::ApplyMeasurementDeltas(
    const std::map<ResourceContext, CPUTimeResult>& measurement_deltas,
    GraphChange graph_change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [context, delta] : measurement_deltas) {
    CHECK(!ContextIs<PageContext>(context));

    // Add the new process, frame and worker measurements to the existing
    // measurements.
    ApplySequentialDelta(context, delta);

    // Aggregate new frame and worker measurements to pages.
    if (ContextIs<FrameContext>(context)) {
      const FrameNode* frame_node =
          AsContext<FrameContext>(context).GetFrameNode();
      CHECK(frame_node);
      ApplyOverlappingDelta(frame_node->GetPageNode()->GetResourceContext(),
                            delta);
    } else if (ContextIs<WorkerContext>(context)) {
      const WorkerNode* worker_node =
          AsContext<WorkerContext>(context).GetWorkerNode();
      CHECK(worker_node);
      for (const PageNode* page_node :
           GetWorkerClientPages(worker_node, graph_change)) {
        ApplyOverlappingDelta(page_node->GetResourceContext(), delta);
      }
    }
  }
}

void CPUMeasurementMonitor::ApplySequentialDelta(const ResourceContext& context,
                                                 const CPUTimeResult& delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ValidateCPUTimeResult(delta);
  auto [it, inserted] = measurement_results_.try_emplace(context, delta);
  if (inserted) {
    // First result for `context`, use `delta` unchanged.
    return;
  }
  CPUTimeResult& result = it->second;
  ValidateCPUTimeResult(result);
  CHECK_EQ(result.metadata.algorithm, delta.metadata.algorithm);
  CHECK_LE(result.metadata.measurement_time, delta.start_time);
  result.metadata.measurement_time = delta.metadata.measurement_time;
  result.cumulative_cpu += delta.cumulative_cpu;

  // Adding a valid delta to a valid result should produce a valid result.
  ValidateCPUTimeResult(result);
}

void CPUMeasurementMonitor::ApplyOverlappingDelta(const PageContext& context,
                                                  const CPUTimeResult& delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ValidateCPUTimeResult(delta);
  auto [it, inserted] = measurement_results_.try_emplace(context, delta);
  if (inserted) {
    // First result for `context`, use `delta` with correct algorithm for pages.
    it->second.metadata.algorithm = MeasurementAlgorithm::kSum;
    return;
  }
  CPUTimeResult& result = it->second;
  ValidateCPUTimeResult(result);
  CHECK_EQ(result.metadata.algorithm, MeasurementAlgorithm::kSum);
  result.metadata.measurement_time = std::max(result.metadata.measurement_time,
                                              delta.metadata.measurement_time);
  result.start_time = std::min(result.start_time, delta.start_time);
  result.cumulative_cpu += delta.cumulative_cpu;

  // Adding a valid delta to a valid result should produce a valid result.
  ValidateCPUTimeResult(result);
}

base::Value::Dict CPUMeasurementMonitor::DescribeContextData(
    const ResourceContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict dict;
  const auto it = measurement_results_.find(context);
  if (it != measurement_results_.end()) {
    const CPUTimeResult& result = it->second;
    const base::TimeDelta measurement_interval =
        result.metadata.measurement_time - result.start_time;
    dict.Set("algorithm", static_cast<int>(result.metadata.algorithm));
    dict.Set("measurement_time", performance_manager::TimeSinceEpochToValue(
                                     result.metadata.measurement_time));
    dict.Set("measurement_interval",
             performance_manager::TimeDeltaToValue(measurement_interval));
    dict.Set("cumulative_cpu",
             performance_manager::TimeDeltaToValue(result.cumulative_cpu));
  }
  return dict;
}

CPUMeasurementMonitor::CPUMeasurement::CPUMeasurement(
    std::unique_ptr<CPUMeasurementDelegate> delegate)
    : delegate_(std::move(delegate)),
      // Record the CPU usage immediately on starting to measure a process, so
      // that the first call to MeasureAndDistributeCPUUsage() will cover the
      // time between the measurement starting and the snapshot.
      most_recent_measurement_(delegate_->GetCumulativeCPUUsage()),
      last_measurement_time_(base::TimeTicks::Now()) {}

CPUMeasurementMonitor::CPUMeasurement::~CPUMeasurement() = default;

CPUMeasurementMonitor::CPUMeasurement::CPUMeasurement(
    CPUMeasurementMonitor::CPUMeasurement&& other) = default;

CPUMeasurementMonitor::CPUMeasurement&
CPUMeasurementMonitor::CPUMeasurement::operator=(
    CPUMeasurementMonitor::CPUMeasurement&& other) = default;

void CPUMeasurementMonitor::CPUMeasurement::MeasureAndDistributeCPUUsage(
    const ProcessNode* process_node,
    const NodeSplitSet& extra_nodes,
    const NodeSplitSet& nodes_to_skip,
    std::map<ResourceContext, CPUTimeResult>& measurement_deltas) {
  // TODO(crbug.com/325330345): Handle final CPU usage of a process.
  //
  // There isn't a good way to get the process CPU usage after it exits here:
  //
  // 1. Attempts to measure it with GetCumulativeCPUUsage() will fail because
  //    the process info is already reaped.
  // 2. For these cases the ChildProcessTerminationInfo struct contains a final
  //    `cpu_usage` member. This needs to be collected by a
  //    RenderProcessHostObserver (either PM's RenderProcessUserData or a
  //    dedicated observer). But:
  // 3. MeasureAndDistributeCPUUsage() distributes the process measurements
  //    among FrameNodes and by the time the final `cpu_usage` is available, the
  //    FrameNodes for the process are often gone already. The reason is that
  //    FrameNodes are removed on process exit by another
  //    RenderProcessHostObserver, and the observers can fire in any order.
  //
  // For the record, the call stack that removes a FrameNode is:
  //
  // performance_manager::PerformanceManagerImpl::DeleteNode()
  // performance_manager::PerformanceManagerTabHelper::RenderFrameDeleted()
  // content::WebContentsImpl::WebContentsObserverList::NotifyObservers<>()
  // content::WebContentsImpl::RenderFrameDeleted()
  // content::RenderFrameHostImpl::RenderFrameDeleted()
  // content::RenderFrameHostImpl::RenderProcessGone()
  // content::SiteInstanceGroup::RenderProcessExited() <-- observer
  //
  // So it's not possible to attribute the final CPU usage of a process to its
  // frames without a refactor of PerformanceManager to keep the FrameNodes
  // alive slightly longer, or keeping a snapshot of the frame topology using
  // FrameContext until after the ChildProcessTerminationInfo is received, and
  // using that snapshot to distribute the measurements.
  //
  // Assume that the previous measurement was taken at time A
  // (`last_measurement_time_`), and the current measurement is being taken at
  // time B (TimeTicks::Now()). Since a measurement is taken in the
  // CPUMeasurement constructor, there will always be a previous measurement.
  //
  // Let CPU(T) be the cpu measurement at time T.
  //
  // Note that the process is only measured after it's passed to the graph,
  // which is shortly after it's created, so at "process creation time" C,
  // CPU(C) may have a small value instead of 0. On the first call to
  // MeasureAndDistributeCPUUsage(), `most_recent_measurement_` will be CPU(C),
  // from the measurement in the constructor.
  //
  // There are 4 cases:
  //
  // 1. The process was created at time A (this is the first measurement.)
  //
  //      A         B
  // |----|---------|
  // | 0% |    X%   |
  //
  //
  // cumulative_cpu += CPU(B) - CPU(A)
  //
  // CPU(B) = GetCumulativeCPUUsage()
  // CPU(A) = `most_recent_measurement_` (set in the constructor)
  //
  // 2. The process existed for the entire duration A..B.
  //
  // A              B
  // |--------------|
  // |      X%      |
  //
  // cumulative_cpu += CPU(B) - CPU(A)
  //
  // CPU(B) = GetCumulativeCPUUsage()
  // CPU(A) = `most_recent_measurement_`
  //
  // 3. The process existed at time A, but exited at time D, between A and B.
  //
  // A         D    B
  // |---------+----|
  // |    X%   | 0% |
  //
  // cumulative_cpu += CPU(D) - CPU(A)
  //
  // CPU(D) = ChildProcessTerminationInfo::cpu_usage (currently unavailable)
  // CPU(A) = `most_recent_measurement_`
  //
  // 4. Process created at time A and exited at time D, between A and B.
  //
  //      A    D    B
  // |----+----+----|
  // | 0% | X% | 0% |
  //
  // cumulative_cpu += CPU(D) - CPU(A)
  //
  // CPU(D) = ChildProcessTerminationInfo::cpu_usage (currently unavailable)
  // CPU(A) = `most_recent_measurement_` (set in the constructor)
  //
  // In case 1 and case 2, `cumulative_cpu` increases by
  // `GetCumulativeCPUUsage() - most_recent_measurement_`. Case 3 and 4 can be
  // ignored because GetCumulativeCPUUsage() will return an error code.
  const base::TimeTicks measurement_interval_start = last_measurement_time_;
  const base::TimeTicks measurement_interval_end = base::TimeTicks::Now();
  CHECK(!measurement_interval_start.is_null());
  CHECK(!measurement_interval_end.is_null());
  // TODO(https://crbug.com/326201232): Turn this back into a CHECK or remove it
  // after figuring out why it's being hit in production sometimes.
  if (process_node->GetLaunchTime() > measurement_interval_start) {
    SCOPED_CRASH_KEY_NUMBER(
        "CPUMeasurement", "process_start",
        (process_node->GetLaunchTime() - base::TimeTicks()).InNanoseconds());
    SCOPED_CRASH_KEY_NUMBER(
        "CPUMeasurement", "interval_start",
        (measurement_interval_start - base::TimeTicks()).InNanoseconds());
    base::debug::DumpWithoutCrashing();
  }
  if (measurement_interval_start == measurement_interval_end) {
    // No time has passed to measure.
    return;
  }
  CHECK_LT(measurement_interval_start, measurement_interval_end);

  std::optional<base::TimeDelta> current_cpu_usage =
      delegate_->GetCumulativeCPUUsage();
  if (!current_cpu_usage.has_value()) {
    // GetCumulativeCPUUsage() failed. Don't update the measurement state.
    return;
  }
  if (!most_recent_measurement_.has_value()) {
    // This is the first successful reading. Just record it.
    most_recent_measurement_ = current_cpu_usage;
    last_measurement_time_ = measurement_interval_end;
    return;
  }

  // When measured in quick succession, GetCumulativeCPUUsage() can go
  // backwards.
  if (current_cpu_usage.value() < most_recent_measurement_.value()) {
    current_cpu_usage = most_recent_measurement_;
  }

  const base::TimeDelta cumulative_cpu_delta =
      current_cpu_usage.value() - most_recent_measurement_.value();
  most_recent_measurement_ = current_cpu_usage;
  last_measurement_time_ = measurement_interval_end;

  auto record_cpu_deltas = [&measurement_deltas, &measurement_interval_start,
                            &measurement_interval_end](
                               const ResourceContext& context,
                               base::TimeDelta cpu_delta,
                               MeasurementAlgorithm algorithm) {
    // Each ProcessNode should be updated by one call to
    // MeasureAndDistributeCPUUsage(), and each FrameNode and WorkerNode is in a
    // single process, so none of these contexts should be in the map yet. Each
    // FrameNode or WorkerNode's containing process is measured when the node is
    // added, so `start_time` will be correctly set to the first time the node
    // is measured.
    CHECK(!cpu_delta.is_negative());
    const auto [_, inserted] = measurement_deltas.emplace(
        context,
        CPUTimeResult{
            .metadata = ResultMetadata(measurement_interval_end, algorithm),
            .start_time = measurement_interval_start,
            .cumulative_cpu = cpu_delta,
        });
    CHECK(inserted);
  };

  record_cpu_deltas(process_node->GetResourceContext(), cumulative_cpu_delta,
                    MeasurementAlgorithm::kDirectMeasurement);
  resource_attribution::SplitResourceAmongFramesAndWorkers(
      cumulative_cpu_delta, process_node, extra_nodes, nodes_to_skip,
      [&record_cpu_deltas](const FrameNode* f, base::TimeDelta cpu_delta) {
        record_cpu_deltas(f->GetResourceContext(), cpu_delta,
                          MeasurementAlgorithm::kSplit);
      },
      [&record_cpu_deltas](const WorkerNode* w, base::TimeDelta cpu_delta) {
        record_cpu_deltas(w->GetResourceContext(), cpu_delta,
                          MeasurementAlgorithm::kSplit);
      });
}

}  // namespace resource_attribution

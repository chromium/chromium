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
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/sequence_checker.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/resource_attribution/attribution_helpers.h"
#include "components/performance_manager/public/resource_attribution/frame_context.h"
#include "components/performance_manager/public/resource_attribution/worker_context.h"
#include "components/performance_manager/resource_attribution/graph_change.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/common/process_type.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace performance_manager::resource_attribution {

namespace {

class CPUMeasurementDelegateImpl final : public CPUMeasurementDelegate {
 public:
  // Default factory function.
  static std::unique_ptr<CPUMeasurementDelegate> Create(
      const ProcessNode* process_node) {
    return std::make_unique<CPUMeasurementDelegateImpl>(process_node);
  }

  explicit CPUMeasurementDelegateImpl(const ProcessNode* process_node);
  ~CPUMeasurementDelegateImpl() final = default;

  base::TimeDelta GetCumulativeCPUUsage() final;

 private:
  std::unique_ptr<base::ProcessMetrics> process_metrics_;
};

CPUMeasurementDelegateImpl::CPUMeasurementDelegateImpl(
    const ProcessNode* process_node) {
  const base::ProcessHandle handle = process_node->GetProcess().Handle();
#if BUILDFLAG(IS_MAC)
  process_metrics_ = base::ProcessMetrics::CreateProcessMetrics(
      handle, content::BrowserChildProcessHost::GetPortProvider());
#else
  process_metrics_ = base::ProcessMetrics::CreateProcessMetrics(handle);
#endif
}

base::TimeDelta CPUMeasurementDelegateImpl::GetCumulativeCPUUsage() {
#if BUILDFLAG(IS_WIN)
  return process_metrics_->GetPreciseCumulativeCPUUsage();
#else
  return process_metrics_->GetCumulativeCPUUsage();
#endif
}

// Returns true if `result` is in the default-initialized state.
bool IsEmptyCPUTimeResult(const CPUTimeResult& result) {
  if (result.metadata.measurement_time.is_null()) {
    CHECK(result.start_time.is_null());
    CHECK(result.cumulative_cpu.is_zero());
    return true;
  }
  return false;
}

// CHECK's that `result` obeys all constraints: either it is empty (both start
// and end timestamps are null, and `cumulative_cpu` is zero) or the start and
// end timestamps form a positive interval and `cumulative_cpu` will fit into
// that interval.
void ValidateCPUTimeResult(const CPUTimeResult& result) {
  // Empty struct is valid.
  if (IsEmptyCPUTimeResult(result)) {
    return;
  }

  // Start and end must form a valid interval.
  CHECK(!result.metadata.measurement_time.is_null());
  CHECK(!result.start_time.is_null());
  const base::TimeDelta interval =
      result.metadata.measurement_time - result.start_time;
  CHECK(interval.is_positive());

  CHECK(!result.cumulative_cpu.is_negative());
}

// Adds the measurement in `delta` to `result`. The start time of `delta` must
// follow the end time of `result`. Used for adding successive measurements of
// process, frame and worker contexts. There may be gaps between deltas, such as
// if a process died and was restarted.
void ApplySequentialDelta(CPUTimeResult& result, const CPUTimeResult& delta) {
  CHECK(!IsEmptyCPUTimeResult(delta));
  ValidateCPUTimeResult(delta);
  if (IsEmptyCPUTimeResult(result)) {
    result = delta;
  } else {
    ValidateCPUTimeResult(result);
    CHECK_LE(result.metadata.measurement_time, delta.start_time);
    result.metadata.measurement_time = delta.metadata.measurement_time;
    result.cumulative_cpu += delta.cumulative_cpu;
  }

  // Adding a valid delta to a valid result should produce a valid result.
  ValidateCPUTimeResult(result);
}

// Adds the measurement in `delta` to `result`. Delta may start before `result`
// or end after it. Used for adding frame and worker measurements to page
// contexts, since the frames and workers can be added in any order.
void ApplyOverlappingDelta(CPUTimeResult& result, const CPUTimeResult& delta) {
  CHECK(!IsEmptyCPUTimeResult(delta));
  ValidateCPUTimeResult(delta);
  if (IsEmptyCPUTimeResult(result)) {
    result = delta;
  } else {
    ValidateCPUTimeResult(result);
    result.metadata.measurement_time = std::max(
        result.metadata.measurement_time, delta.metadata.measurement_time);
    result.start_time = std::min(result.start_time, delta.start_time);
    result.cumulative_cpu += delta.cumulative_cpu;
  }

  // Adding a valid delta to a valid result should produce a valid result.
  ValidateCPUTimeResult(result);
}

// Recursively visits all client workers of `worker_node`, and all client frames
// of each worker, and adds each frame's PageNode to `client_pages`.
// `visited_workers` is used to check for loops in the graph of client workers.
// `graph_change` is a change to the graph topology in progress that may affect
// the client page set, or NoGraphChange.
void RecursivelyFindClientPages(const WorkerNode* worker_node,
                                GraphChange graph_change,
                                std::set<const PageNode*>& client_pages,
                                std::set<const WorkerNode*>& visited_workers) {
  const auto [_, inserted] = visited_workers.insert(worker_node);
  if (!inserted) {
    // Already visited, halt recursion.
    return;
  }
  worker_node->VisitClientFrames(
      [&client_pages, &graph_change, worker_node](const FrameNode* f) {
        const auto* add_client_frame_change =
            absl::get_if<GraphChangeAddClientFrameToWorker>(&graph_change);
        if (add_client_frame_change &&
            add_client_frame_change->worker_node == worker_node &&
            add_client_frame_change->client_frame_node == f) {
          // Skip this client node. The measurement being distributed includes
          // CPU usage from before it was added.
          return true;
        }
        client_pages.insert(f->GetPageNode());
        return true;
      });
  worker_node->VisitClientWorkers([&client_pages, &visited_workers,
                                   &graph_change,
                                   worker_node](const WorkerNode* w) {
    const auto* add_client_worker_change =
        absl::get_if<GraphChangeAddClientWorkerToWorker>(&graph_change);
    if (add_client_worker_change &&
        add_client_worker_change->worker_node == worker_node &&
        add_client_worker_change->client_worker_node == w) {
      // Skip this client node. The measurement being distributed includes CPU
      // usage from before it was added.
      return true;
    }

    RecursivelyFindClientPages(w, graph_change, client_pages, visited_workers);
    return true;
  });
  // Unlike FrameNode, WorkerNode does not update any graph links in
  // WorkerNodeImpl::OnBeforeLeavingGraph(). So no need to check for
  // GraphChangeRemoveClient*FromWorker.
  // TODO(https://crbug.com/1481676): If that changes. handle
  // `graph_change.client_*_node` as if it was visited by the above visitors.
}

// Returns the complete set of pages that are clients of `worker_node`.
// `graph_change` is a change to the graph topology in progress that may affect
// the client page set, or NoGraphChange.
std::set<const PageNode*> GetClientPages(const WorkerNode* worker_node,
                                         GraphChange graph_change) {
  std::set<const PageNode*> client_pages;
  std::set<const WorkerNode*> visited_workers;
  RecursivelyFindClientPages(worker_node, graph_change, client_pages,
                             visited_workers);
  return client_pages;
}

}  // namespace

CPUMeasurementMonitor::CPUMeasurementMonitor()
    : cpu_measurement_delegate_factory_(
          base::BindRepeating(&CPUMeasurementDelegateImpl::Create)) {}

CPUMeasurementMonitor::~CPUMeasurementMonitor() {
  if (graph_) {
    StopMonitoring();
  }
  CHECK(!graph_);
}

void CPUMeasurementMonitor::SetCPUMeasurementDelegateFactoryForTesting(
    CPUMeasurementDelegate::FactoryCallback factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ensure that all CPU measurements use the same delegate.
  CHECK(cpu_measurement_map_.empty());
  if (factory.is_null()) {
    cpu_measurement_delegate_factory_ =
        base::BindRepeating(&CPUMeasurementDelegateImpl::Create);
  } else {
    cpu_measurement_delegate_factory_ = std::move(factory);
  }
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
    if (process_node->GetProcessType() == content::PROCESS_TYPE_RENDERER &&
        process_node->GetProcessId() != base::kNullProcessId) {
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

std::map<ResourceContext, CPUTimeResult>
CPUMeasurementMonitor::UpdateAndGetCPUMeasurements() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateAllCPUMeasurements();
  std::map<ResourceContext, CPUTimeResult> results;
  for (const auto& [context, result] : measurement_results_) {
    ValidateCPUTimeResult(result);
    if (IsEmptyCPUTimeResult(result)) {
      // Don't include empty measurements in the public results.
      continue;
    }
    results.emplace(context, result);
  }
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
  // Only handle process start notifications (which is when the pid is
  // assigned), not exit notifications. Note the pid can be reassigned if a
  // process dies and a new one is started for the same ProcessNode - in that
  // case MonitorCPUUsage will reset the measurements and start monitoring the
  // new process from scratch.
  if (!process_node->GetProcess().IsValid()) {
    return;
  }
  CHECK_NE(process_node->GetProcessId(), base::kNullProcessId);
  if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER) {
    return;
  }
  MonitorCPUUsage(process_node);
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

void CPUMeasurementMonitor::MonitorCPUUsage(const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only measure renderers.
  // TODO(crbug.com/1471683): Measure other process types, just don't distribute
  // the measurements to frames and workers.
  CHECK_EQ(process_node->GetProcessType(), content::PROCESS_TYPE_RENDERER);

  // If a process crashes and is restarted, a new process can be assigned to the
  // same ProcessNode (and the same ProcessContext). When that happens
  // OnProcessLifetimeChange will call MonitorCPUUsage again for the same node,
  // creating a new CPUMeasurement that starts measuring the new process from 0.
  // ApplyMeasurementDeltas will add the new measurements and the old
  // measurements in the same ProcessContext.
  cpu_measurement_map_.insert_or_assign(
      process_node,
      CPUMeasurement(cpu_measurement_delegate_factory_.Run(process_node)));
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
                    nodes_to_skip.insert(change.frame_node);
                  },
                  [&nodes_to_skip](GraphChangeAddWorker change) {
                    nodes_to_skip.insert(change.worker_node);
                  },
                  [&extra_nodes](GraphChangeRemoveFrame change) {
                    extra_nodes.insert(change.frame_node);
                  },
                  [&extra_nodes](GraphChangeRemoveWorker change) {
                    extra_nodes.insert(change.worker_node);
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
    ApplySequentialDelta(measurement_results_[context], delta);

    // Aggregate new frame and worker measurements to pages.
    if (ContextIs<FrameContext>(context)) {
      const FrameNode* frame_node =
          AsContext<FrameContext>(context).GetFrameNode();
      CHECK(frame_node);
      ApplyOverlappingDelta(
          measurement_results_[frame_node->GetPageNode()->GetResourceContext()],
          delta);
    } else if (ContextIs<WorkerContext>(context)) {
      const WorkerNode* worker_node =
          AsContext<WorkerContext>(context).GetWorkerNode();
      CHECK(worker_node);
      for (const PageNode* page_node :
           GetClientPages(worker_node, graph_change)) {
        ApplyOverlappingDelta(
            measurement_results_[page_node->GetResourceContext()], delta);
      }
    }
  }
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
  // TODO(crbug.com/1471683): Handle final CPU usage of a process.
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
  // alive slightly longer.
  //
  // A better and more complete way to handle this would be to update the CPU
  // usage of a PageNode every time a frame or worker is created or deleted.
  // This would keep the estimate up to date with the page topology, which is
  // important to avoid under-estimating the CPU usage of pages that create a
  // lot of short-lived iframes.

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
  CHECK_LE(process_node->GetLaunchTime(), measurement_interval_start);
  if (measurement_interval_start == measurement_interval_end) {
    // No time has passed to measure.
    return;
  }
  CHECK_LT(measurement_interval_start, measurement_interval_end);

  base::TimeDelta current_cpu_usage = delegate_->GetCumulativeCPUUsage();
  if (!current_cpu_usage.is_positive()) {
    // GetCumulativeCPUUsage() failed. Don't update the measurement state.
    // Most platforms return a zero TimeDelta on error, Linux returns a
    // negative.
    return;
  }
  // When measured in quick succession, GetCumulativeCPUUsage() can go
  // backwards.
  if (current_cpu_usage < most_recent_measurement_) {
    current_cpu_usage = most_recent_measurement_;
  }

  const base::TimeDelta cumulative_cpu_delta =
      current_cpu_usage - most_recent_measurement_;
  most_recent_measurement_ = current_cpu_usage;
  last_measurement_time_ = measurement_interval_end;

  auto record_cpu_deltas = [&measurement_deltas, &measurement_interval_start,
                            &measurement_interval_end](
                               const ResourceContext& context,
                               base::TimeDelta cpu_delta) {
    // Each ProcessNode should be updated by one call to
    // MeasureAndDistributeCPUUsage(), and each FrameNode and WorkerNode is in a
    // single process, so none of these contexts should be in the map yet. Each
    // FrameNode or WorkerNode's containing process is measured when the node is
    // added, so `start_time` will be correctly set to the first time the node
    // is measured.
    CHECK(!cpu_delta.is_negative());
    const auto [_, inserted] = measurement_deltas.emplace(
        context, CPUTimeResult{
                     .metadata = {.measurement_time = measurement_interval_end},
                     .start_time = measurement_interval_start,
                     .cumulative_cpu = cpu_delta,
                 });
    CHECK(inserted);
  };

  record_cpu_deltas(process_node->GetResourceContext(), cumulative_cpu_delta);
  resource_attribution::SplitResourceAmongFramesAndWorkers(
      cumulative_cpu_delta, process_node, extra_nodes, nodes_to_skip,
      [&record_cpu_deltas](const FrameNode* f, base::TimeDelta cpu_delta) {
        record_cpu_deltas(f->GetResourceContext(), cpu_delta);
      },
      [&record_cpu_deltas](const WorkerNode* w, base::TimeDelta cpu_delta) {
        record_cpu_deltas(w->GetResourceContext(), cpu_delta);
      });
}

}  // namespace performance_manager::resource_attribution

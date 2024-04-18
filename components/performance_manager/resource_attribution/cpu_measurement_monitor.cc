// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/cpu_measurement_monitor.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "components/performance_manager/public/features.h"
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
#include "components/performance_manager/resource_attribution/node_data_describers.h"
#include "components/performance_manager/resource_attribution/worker_client_pages.h"
#include "content/public/common/process_type.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace resource_attribution {

namespace {

using performance_manager::features::kResourceAttributionIncludeOrigins;

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

std::optional<url::Origin> GetOriginForNode(const FrameNode* frame_node) {
  return frame_node->GetOrigin();
}

std::optional<url::Origin> GetOriginForNode(const WorkerNode* worker_node) {
  // TODO(http://crbug.com/333248839): Instead of creating the Origin from an
  // URL, which loses some information, should store it as a node property. See
  // https://chromium.googlesource.com/chromium/src/+/main/docs/security/origin-vs-url.md
  return url::Origin::Create(worker_node->GetURL());
}

template <typename FrameOrWorkerNode>
std::optional<OriginInPageContext> OriginInPageContextForNode(
    const FrameOrWorkerNode* node,
    const PageNode* page_node,
    GraphChange graph_change = NoGraphChange{}) {
  if (!base::FeatureList::IsEnabled(kResourceAttributionIncludeOrigins)) {
    return std::nullopt;
  }
  // If this node was just assigned a new origin, assign CPU usage before the
  // change to the previous origin.
  GraphChangeUpdateOrigin* origin_change =
      absl::get_if<GraphChangeUpdateOrigin>(&graph_change);
  const std::optional<url::Origin> origin =
      (origin_change && origin_change->node == node)
          ? origin_change->previous_origin
          : GetOriginForNode(node);
  if (!origin.has_value()) {
    return std::nullopt;
  }
  return OriginInPageContext(origin.value(), page_node->GetResourceContext());
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
  CHECK(dead_measurement_results_.empty());
  graph_ = graph;
  graph_->AddFrameNodeObserver(this);
  graph_->AddPageNodeObserver(this);
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
  dead_measurement_results_.clear();
  graph_->RemoveFrameNodeObserver(this);
  graph_->RemovePageNodeObserver(this);
  graph_->RemoveProcessNodeObserver(this);
  graph_->RemoveWorkerNodeObserver(this);
  graph_ = nullptr;
}

bool CPUMeasurementMonitor::IsMonitoring() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return graph_;
}

void CPUMeasurementMonitor::RepeatingQueryStarted(internal::QueryId query_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsMonitoring());
  // Start with an empty dead measurement list for this query.
  const auto [_, inserted] =
      dead_measurement_results_.try_emplace(query_id, DeadContextResultList{});
  CHECK(inserted);
}

void CPUMeasurementMonitor::RepeatingQueryStopped(internal::QueryId query_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsMonitoring());
  size_t erased = dead_measurement_results_.erase(query_id);
  CHECK_EQ(erased, 1u);
}

bool CPUMeasurementMonitor::IsTrackingQueryForTesting(
    internal::QueryId query_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Contains(dead_measurement_results_, query_id);
}

size_t CPUMeasurementMonitor::GetDeadContextCountForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t count = 0;
  for (const auto& [_, result_list] : dead_measurement_results_) {
    count += result_list.size();
  }
  return count;
}

QueryResultMap CPUMeasurementMonitor::UpdateAndGetCPUMeasurements(
    std::optional<internal::QueryId> query_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateAllCPUMeasurements();
  QueryResultMap results;
  for (const auto& [context, result_ptr] : measurement_results_) {
    CHECK(result_ptr);
    ValidateCPUTimeResult(result_ptr->data);
    results.emplace(context, QueryResults{.cpu_time_result = result_ptr->data});
  }

  if (query_id.has_value()) {
    // Include measurements for nodes that were deleted since the last time this
    // query got an update. This drops the query's reference to the
    // ScopedCPUTimeResultPtr so the result won't be sent again for this query,
    // and will be deleted once all queries have dropped their reference.
    auto it = dead_measurement_results_.find(query_id.value());
    CHECK(it != dead_measurement_results_.end());
    DeadContextResultList dead_results;
    std::swap(it->second, dead_results);
    for (const auto& [context, result_ptr] : dead_results) {
      CHECK(result_ptr);
      ValidateCPUTimeResult(result_ptr->data);
      results.emplace(context,
                      QueryResults{.cpu_time_result = result_ptr->data});
    }
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
  SaveFinalMeasurements({frame_node->GetResourceContext()});
}

void CPUMeasurementMonitor::OnOriginChanged(
    const FrameNode* frame_node,
    const std::optional<url::Origin>& previous_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Take a measurement of the process CPU usage, but assign this frame's CPU to
  // its previous origin for OriginInPageContext, so that the CPU usage from
  // before the navigation committed is attributed to the old origin.
  UpdateCPUMeasurements(frame_node->GetProcessNode(),
                        GraphChangeUpdateOrigin(frame_node, previous_value));
}

void CPUMeasurementMonitor::OnBeforePageNodeRemoved(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // No need to call UpdateCPUMeasurements() since a measurement was taken when
  // the last frame was removed from the page.
  const PageContext& page_context = page_node->GetResourceContext();
  std::vector<ResourceContext> contexts{page_context};
  if (base::FeatureList::IsEnabled(kResourceAttributionIncludeOrigins)) {
    // Find all OriginInPageContexts for this page.
    // TODO(crbug.com/333112603): OriginInPageContext results for opaque origins
    // could also be deleted when the PageNode no longer has any frames for the
    // given origin. Non-opaque origins should be kept for the lifetime of the
    // page, because new frames for the same origin could be created.
    for (const auto& [context, _] : measurement_results_) {
      auto origin_context = AsOptionalContext<OriginInPageContext>(context);
      if (origin_context.has_value() &&
          origin_context->GetPageContext() == page_context) {
        contexts.push_back(std::move(origin_context.value()));
      }
    }
  }
  SaveFinalMeasurements(contexts);
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
  // On most platforms this will get no updates as the OS process is no longer
  // running. Windows and Fuchsia will return final measurements of a process
  // after it exits.
  // TODO(crbug.com/325330345): Capture the full final measurement reported
  // through ChildProcessTerminationInfo::cpu_usage.
  UpdateCPUMeasurements(process_node);
  SaveFinalMeasurements({process_node->GetResourceContext()});
  cpu_measurement_map_.erase(process_node);
}

void CPUMeasurementMonitor::OnPriorityChanged(
    const ProcessNode* process_node,
    base::TaskPriority previous_value) {
  UpdateCPUMeasurements(process_node, GraphChangeUpdateProcessPriority(
                                          process_node, previous_value));
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
  SaveFinalMeasurements({worker_node->GetResourceContext()});
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

void CPUMeasurementMonitor::OnFinalResponseURLDetermined(
    const WorkerNode* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Take a measurement of the process CPU usage, but don't assign this worker's
  // CPU to an OriginInPageContext, since the origin wasn't defined until now.
  UpdateCPUMeasurements(
      worker_node->GetProcessNode(),
      GraphChangeUpdateOrigin(worker_node, /*previous_origin=*/std::nullopt));
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
    cpu_measurement.MeasureAndDistributeCPUUsage(node, NoGraphChange(),
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

  if (!base::FeatureList::IsEnabled(kResourceAttributionIncludeOrigins) &&
      absl::holds_alternative<GraphChangeUpdateOrigin>(graph_change)) {
    // No need to update measurements on origin changes when origins aren't
    // being measured.
    return;
  }

  // Update CPU metrics, attributing the cumulative CPU of the process to its
  // frames and workers.
  std::map<ResourceContext, CPUTimeResult> measurement_deltas;
  const auto it = cpu_measurement_map_.find(process_node);
  if (it == cpu_measurement_map_.end()) {
    // In tests, FrameNode's can be added to mock processes that don't have a
    // PID so aren't being monitored.
    return;
  }
  it->second.MeasureAndDistributeCPUUsage(it->first, graph_change,
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
      std::optional<OriginInPageContext> origin_in_page_context =
          OriginInPageContextForNode(frame_node, frame_node->GetPageNode(),
                                     graph_change);
      if (origin_in_page_context.has_value()) {
        ApplyOverlappingDelta(origin_in_page_context.value(), delta);
      }
    } else if (ContextIs<WorkerContext>(context)) {
      const WorkerNode* worker_node =
          AsContext<WorkerContext>(context).GetWorkerNode();
      CHECK(worker_node);
      for (const PageNode* page_node :
           GetWorkerClientPages(worker_node, graph_change)) {
        ApplyOverlappingDelta(page_node->GetResourceContext(), delta);
        std::optional<OriginInPageContext> origin_in_page_context =
            OriginInPageContextForNode(worker_node, page_node, graph_change);
        if (origin_in_page_context.has_value()) {
          ApplyOverlappingDelta(origin_in_page_context.value(), delta);
        }
      }
    }
  }
}

void CPUMeasurementMonitor::ApplySequentialDelta(const ResourceContext& context,
                                                 const CPUTimeResult& delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ValidateCPUTimeResult(delta);
  auto [it, inserted] =
      measurement_results_.try_emplace(context, ScopedCPUTimeResultPtr());
  if (inserted) {
    // First result for `context`, use `delta` unchanged.
    it->second =
        base::MakeRefCounted<base::RefCountedData<CPUTimeResult>>(delta);
    return;
  }
  CHECK(it->second);
  CPUTimeResult& result = it->second->data;
  ValidateCPUTimeResult(result);
  CHECK_EQ(result.metadata.algorithm, delta.metadata.algorithm);
  CHECK_LE(result.metadata.measurement_time, delta.start_time);
  result.metadata.measurement_time = delta.metadata.measurement_time;
  result.cumulative_cpu += delta.cumulative_cpu;
  result.cumulative_background_cpu += delta.cumulative_background_cpu;

  // Adding a valid delta to a valid result should produce a valid result.
  ValidateCPUTimeResult(result);
}

void CPUMeasurementMonitor::ApplyOverlappingDelta(
    const ResourceContext& context,
    const CPUTimeResult& delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ValidateCPUTimeResult(delta);
  auto [it, inserted] =
      measurement_results_.try_emplace(context, ScopedCPUTimeResultPtr());
  if (inserted) {
    // First result for `context`, use `delta` with correct algorithm for pages.
    it->second =
        base::MakeRefCounted<base::RefCountedData<CPUTimeResult>>(delta);
    it->second->data.metadata.algorithm = MeasurementAlgorithm::kSum;
    return;
  }
  CPUTimeResult& result = it->second->data;
  ValidateCPUTimeResult(result);
  CHECK_EQ(result.metadata.algorithm, MeasurementAlgorithm::kSum);
  result.metadata.measurement_time = std::max(result.metadata.measurement_time,
                                              delta.metadata.measurement_time);
  result.start_time = std::min(result.start_time, delta.start_time);
  result.cumulative_cpu += delta.cumulative_cpu;
  result.cumulative_background_cpu += delta.cumulative_background_cpu;

  // Adding a valid delta to a valid result should produce a valid result.
  ValidateCPUTimeResult(result);
}

void CPUMeasurementMonitor::SaveFinalMeasurements(
    const std::vector<ResourceContext>& contexts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& context : contexts) {
    auto it = measurement_results_.find(context);
    if (it == measurement_results_.end()) {
      continue;
    }
    // Copy the scoped_refptr to result list for every existing query_id.
    const ScopedCPUTimeResultPtr& result_ptr = it->second;
    for (auto& [query_id, result_list] : dead_measurement_results_) {
      result_list.emplace_back(context, result_ptr);
    }
    // Drop the scoped_refptr from the live measurement results. Now there's one
    // reference for every query, and the CPUTimeResult will be deleted once all
    // queries have gotten the result.
    measurement_results_.erase(it);
  }
}

base::Value::Dict CPUMeasurementMonitor::DescribeContextData(
    const ResourceContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict dict;
  const auto it = measurement_results_.find(context);
  if (it != measurement_results_.end()) {
    CHECK(it->second);
    const CPUTimeResult& result = it->second->data;
    const base::TimeDelta measurement_interval =
        result.metadata.measurement_time - result.start_time;
    dict.Merge(DescribeResultMetadata(result.metadata));
    dict.Set("measurement_interval",
             performance_manager::TimeDeltaToValue(measurement_interval));
    dict.Set("cumulative_cpu",
             performance_manager::TimeDeltaToValue(result.cumulative_cpu));
    dict.Set("cumulative_background_cpu",
             performance_manager::TimeDeltaToValue(
                 result.cumulative_background_cpu));
  }
  return dict;
}

CPUMeasurementMonitor::CPUMeasurement::CPUMeasurement(
    std::unique_ptr<CPUMeasurementDelegate> delegate)
    : delegate_(std::move(delegate)),
      // Record the CPU usage immediately on starting to measure a process, so
      // that the first call to MeasureAndDistributeCPUUsage() will cover the
      // time between the measurement starting and the snapshot.
      most_recent_measurement_(
          base::OptionalFromExpected(delegate_->GetCumulativeCPUUsage())),
      last_measurement_time_(base::TimeTicks::Now()) {}

CPUMeasurementMonitor::CPUMeasurement::~CPUMeasurement() = default;

CPUMeasurementMonitor::CPUMeasurement::CPUMeasurement(
    CPUMeasurementMonitor::CPUMeasurement&& other) = default;

CPUMeasurementMonitor::CPUMeasurement&
CPUMeasurementMonitor::CPUMeasurement::operator=(
    CPUMeasurementMonitor::CPUMeasurement&& other) = default;

void CPUMeasurementMonitor::CPUMeasurement::MeasureAndDistributeCPUUsage(
    const ProcessNode* process_node,
    GraphChange graph_change,
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
  if (measurement_interval_start == measurement_interval_end) {
    // No time has passed to measure.
    return;
  }
  CHECK_LT(measurement_interval_start, measurement_interval_end);

  std::optional<base::TimeDelta> current_cpu_usage =
      base::OptionalFromExpected(delegate_->GetCumulativeCPUUsage());
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

  // Determine the process priority during the measurement interval. If the
  // process' priority just changed, used the previous priority. Otherwise, use
  // the current priority.
  base::TaskPriority process_priority;
  GraphChangeUpdateProcessPriority* priority_change =
      absl::get_if<GraphChangeUpdateProcessPriority>(&graph_change);
  if (priority_change && priority_change->process_node == process_node) {
    process_priority = priority_change->previous_priority;
  } else {
    process_priority = process_node->GetPriority();
  }

  auto record_cpu_deltas = [&measurement_deltas, &measurement_interval_start,
                            &measurement_interval_end,
                            &process_priority](const ResourceContext& context,
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
            // `cumulative_background_cpu` accumulates CPU consumed while the
            // process' priority is `BEST_EFFORT`.
            .cumulative_background_cpu =
                (process_priority == base::TaskPriority::BEST_EFFORT)
                    ? cpu_delta
                    : base::TimeDelta()});
    CHECK(inserted);
  };

  // Don't distribute measurements to nodes that are being added to the graph.
  // The current measurement covers the time before the node was added.
  NodeSplitSet nodes_to_skip;

  // Include nodes that are being removed from the graph. They may not be
  // reachable through visitors at this point, but the current measurement
  // covers the time before they were removed.
  // TODO(crbug.com/40930981): Make the graph state consistent in
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

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/cpu_measurement_monitor.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
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
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/common/process_type.h"

namespace performance_manager::resource_attribution {

namespace {

class CPUMeasurementDelegateImpl final
    : public CPUMeasurementMonitor::CPUMeasurementDelegate {
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

  // Cumulative CPU must not be more than was actually available. Over very
  // short intervals (on the order of 0.01 ms) rounding up after dividing CPU
  // between frames can lead to `cumulative_cpu` being more than `interval`, so
  // allow 0.1 ms of slack.
  static constexpr base::TimeDelta kEpsilon = base::Milliseconds(0.1);
  static const int kNumProcessors = base::SysInfo::NumberOfProcessors();
  CHECK(!result.cumulative_cpu.is_negative());
  CHECK_LE(result.cumulative_cpu, interval * kNumProcessors + kEpsilon);
}

// Adds the measurement in `delta` to `result`. The start time of `delta` must
// immediately follow the end time of `result`. Used for adding successive
// measurements of process, frame and worker contexts.
void ApplySequentialDelta(CPUTimeResult& result, const CPUTimeResult& delta) {
  CHECK(!IsEmptyCPUTimeResult(delta));
  ValidateCPUTimeResult(delta);
  if (IsEmptyCPUTimeResult(result)) {
    result = delta;
  } else {
    // Successive measurement periods should be back to back.
    ValidateCPUTimeResult(result);
    CHECK_EQ(result.metadata.measurement_time, delta.start_time);
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
  graph_->AddProcessNodeObserver(this);

  // Start monitoring CPU usage for all existing processes. Can't read their CPU
  // usage until they have a pid assigned.
  for (const ProcessNode* process_node : graph->GetAllProcessNodes()) {
    if (process_node->GetProcessType() == content::PROCESS_TYPE_RENDERER &&
        process_node->GetProcessId() != base::kNullProcessId) {
      MonitorCPUUsage(process_node);
    }
  }
}

void CPUMeasurementMonitor::StopMonitoring() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(graph_);
  cpu_measurement_map_.clear();
  graph_->RemoveProcessNodeObserver(this);
  graph_ = nullptr;
}

std::map<ResourceContext, CPUTimeResult>
CPUMeasurementMonitor::UpdateAndGetCPUMeasurements() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateCPUMeasurements();
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

void CPUMeasurementMonitor::OnProcessLifetimeChange(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!graph_) {
    // Not monitoring CPU usage yet.
    CHECK(cpu_measurement_map_.empty());
    return;
  }
  // Only handle process start notifications (which is when the pid is
  // assigned), not exit notifications.
  if (!process_node->GetProcess().IsValid()) {
    return;
  }
  CHECK_NE(process_node->GetProcessId(), base::kNullProcessId);
  if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER) {
    return;
  }
  auto it = cpu_measurement_map_.find(process_node);
  if (it == cpu_measurement_map_.end()) {
    // Process isn't being measured yet so it must have been created while
    // measurements were already started.
    MonitorCPUUsage(process_node);
  }
}

void CPUMeasurementMonitor::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cpu_measurement_map_.erase(process_node);
}

void CPUMeasurementMonitor::MonitorCPUUsage(const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only measure renderers.
  // TODO(crbug.com/1471683): Measure other process types, just don't distribute
  // the measurements to frames and workers.
  CHECK_EQ(process_node->GetProcessType(), content::PROCESS_TYPE_RENDERER);
  const auto& [it, was_inserted] = cpu_measurement_map_.emplace(
      process_node,
      CPUMeasurement(cpu_measurement_delegate_factory_.Run(process_node)));
  CHECK(was_inserted);
}

void CPUMeasurementMonitor::UpdateCPUMeasurements() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Must call StartMonitoring() before getting measurements.
  CHECK(graph_);

  // Update CPU metrics, attributing the cumulative CPU of each process to its
  // frames and workers.
  std::map<ResourceContext, CPUTimeResult> measurement_deltas;
  for (auto& [process_node, cpu_measurement] : cpu_measurement_map_) {
    cpu_measurement.MeasureAndDistributeCPUUsage(process_node,
                                                 measurement_deltas);
  }

  // Add the new process, frame and worker measurements to the existing
  // measurements.
  for (const auto& [context, delta] : measurement_deltas) {
    CHECK(!ContextIs<PageContext>(context));
    ApplySequentialDelta(measurement_results_[context], delta);
  }

  // Aggregate new frame and worker measurements to pages.
  // TODO(crbug.com/1471683): Implement the resource attribution Query API and
  // keep track of which pages are being measured to avoid extra work here.
  graph_->VisitAllPageNodes(
      [this, &measurement_deltas](const PageNode* page_node) {
        EstimatePageCPUUsage(page_node, measurement_deltas);
        return true;
      });
}

void CPUMeasurementMonitor::EstimatePageCPUUsage(
    const PageNode* page_node,
    const std::map<ResourceContext, CPUTimeResult>& measurement_deltas) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CPUTimeResult& result = measurement_results_[page_node->GetResourceContext()];
  auto apply_delta_to_page =
      [&result, &measurement_deltas](const ResourceContext& context) {
        const auto it = measurement_deltas.find(context);
        if (it != measurement_deltas.end()) {
          ApplyOverlappingDelta(result, it->second);
        }
      };
  GraphOperations::VisitFrameTreePreOrder(page_node, [&apply_delta_to_page](
                                                         const FrameNode* f) {
    apply_delta_to_page(f->GetResourceContext());
    // TODO(crbug.com/1410503): Handle non-dedicated workers, which could
    // appear as children of multiple frames.
    f->VisitChildDedicatedWorkers([&apply_delta_to_page](const WorkerNode* w) {
      apply_delta_to_page(w->GetResourceContext());
      return true;
    });
    return true;
  });
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
    // single process, so none of these contexts should be in the map yet.
    const auto [_, inserted] = measurement_deltas.emplace(
        context,
        CPUTimeResult{
            .metadata = {.measurement_time = measurement_interval_end},
            // TODO(crbug.com/1471683): `start_time` for a frame or
            // worker might be different than its parent process, since it
            // might have been created partway through the current
            // measurement interval, after the parent is. Handle this by
            // watching for added/removed FrameNode's and WorkerNode's and
            // taking an immediate measurement, so that their lifetimes
            // always correspond with the measurement period.
            .start_time = measurement_interval_start,
            .cumulative_cpu = cpu_delta,
        });
    CHECK(inserted);
  };

  record_cpu_deltas(process_node->GetResourceContext(), cumulative_cpu_delta);
  resource_attribution::SplitResourceAmongFramesAndWorkers(
      cumulative_cpu_delta, process_node,
      [&record_cpu_deltas](const FrameNode* f, base::TimeDelta cpu_delta) {
        record_cpu_deltas(f->GetResourceContext(), cpu_delta);
      },
      [&record_cpu_deltas](const WorkerNode* w, base::TimeDelta cpu_delta) {
        record_cpu_deltas(w->GetResourceContext(), cpu_delta);
      });
}

}  // namespace performance_manager::resource_attribution

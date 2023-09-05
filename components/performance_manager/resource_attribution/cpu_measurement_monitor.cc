// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/cpu_measurement_monitor.h"

#include <map>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/sequence_checker.h"
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

}  // namespace

CPUMeasurementMonitor::CPUMeasurementMonitor()
    : cpu_measurement_delegate_factory_(
          base::BindRepeating(&CPUMeasurementDelegateImpl::Create)) {}

CPUMeasurementMonitor::~CPUMeasurementMonitor() = default;

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
  graph->AddProcessNodeObserver(this);

  CHECK(last_measurement_time_.is_null());
  last_measurement_time_ = base::TimeTicks::Now();

  // Start monitoring CPU usage for all existing processes. Can't read their CPU
  // usage until they have a pid assigned.
  for (const ProcessNode* process_node : graph->GetAllProcessNodes()) {
    if (process_node->GetProcessType() == content::PROCESS_TYPE_RENDERER &&
        process_node->GetProcessId() != base::kNullProcessId) {
      MonitorCPUUsage(process_node);
    }
  }
}

void CPUMeasurementMonitor::StopMonitoring(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cpu_measurement_map_.clear();
  CHECK(!last_measurement_time_.is_null());
  last_measurement_time_ = base::TimeTicks();
  graph->RemoveProcessNodeObserver(this);
}

CPUMeasurementMonitor::CPUUsageMap
CPUMeasurementMonitor::UpdateCPUMeasurements() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Update CPU metrics, attributing the cumulative CPU of each process to its
  // frames and workers.
  CHECK(!last_measurement_time_.is_null());
  CPUUsageMap cpu_usage_map;
  const base::TimeTicks now = base::TimeTicks::Now();
  for (auto& [process_node, cpu_measurement] : cpu_measurement_map_) {
    cpu_measurement.MeasureAndDistributeCPUUsage(
        process_node, last_measurement_time_, now, cpu_usage_map);
  }
  last_measurement_time_ = now;
  return cpu_usage_map;
}

// static
double CPUMeasurementMonitor::EstimatePageCPUUsage(
    const PageNode* page_node,
    const CPUUsageMap& cpu_usage_map) {
  double page_cpu_usage = 0.0;
  auto accumulate_cpu_usage = [&page_cpu_usage,
                               &cpu_usage_map](const ResourceContext& context) {
    const auto it = cpu_usage_map.find(context);
    // A context might be missing from the map if there was an error measuring
    // the CPU usage of its process.
    if (it != cpu_usage_map.end()) {
      page_cpu_usage += it->second;
    }
  };
  GraphOperations::VisitFrameTreePreOrder(page_node, [&accumulate_cpu_usage](
                                                         const FrameNode* f) {
    accumulate_cpu_usage(f->GetResourceContext());
    // TODO(crbug.com/1410503): Handle non-dedicated workers, which could appear
    // as children of multiple frames.
    f->VisitChildDedicatedWorkers([&accumulate_cpu_usage](const WorkerNode* w) {
      accumulate_cpu_usage(w->GetResourceContext());
      return true;
    });
    return true;
  });
  return page_cpu_usage;
}

void CPUMeasurementMonitor::OnProcessLifetimeChange(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (last_measurement_time_.is_null()) {
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
  CHECK_EQ(process_node->GetProcessType(), content::PROCESS_TYPE_RENDERER);
  const auto& [it, was_inserted] = cpu_measurement_map_.emplace(
      process_node,
      CPUMeasurement(cpu_measurement_delegate_factory_.Run(process_node)));
  CHECK(was_inserted);
}

CPUMeasurementMonitor::CPUMeasurement::CPUMeasurement(
    std::unique_ptr<CPUMeasurementDelegate> delegate)
    : delegate_(std::move(delegate)),
      // Record the CPU usage immediately on starting to measure a process, so
      // that the first call to MeasureAndDistributeCPUUsage() will cover the
      // time between the measurement starting and the snapshot.
      most_recent_measurement_(delegate_->GetCumulativeCPUUsage()) {}

CPUMeasurementMonitor::CPUMeasurement::~CPUMeasurement() = default;

CPUMeasurementMonitor::CPUMeasurement::CPUMeasurement(
    CPUMeasurementMonitor::CPUMeasurement&& other) = default;

CPUMeasurementMonitor::CPUMeasurement&
CPUMeasurementMonitor::CPUMeasurement::operator=(
    CPUMeasurementMonitor::CPUMeasurement&& other) = default;

void CPUMeasurementMonitor::CPUMeasurement::MeasureAndDistributeCPUUsage(
    const ProcessNode* process_node,
    base::TimeTicks measurement_interval_start,
    base::TimeTicks measurement_interval_end,
    CPUUsageMap& cpu_usage_map) {
  // TODO(crbug.com/1410503): There isn't a good way to get the process CPU
  // usage after it exits here:
  //
  // 1. Attempts to measure it with GetCumulativeCPUUsage() will fail because
  //    the process info is already reaped.
  // 2. For these cases the ChildProcessTerminationInfo struct contains a final
  //    `cpu_usage` member. This needs to be collected by a
  //    RenderProcessHostObserver (either PM's RenderProcessUserData or a
  //    dedicated observer). But:
  // 3. MeasureAndDistributeCPUUsage() distributes the process measurements
  // among FrameNodes and
  //    by the time the final `cpu_usage` is available, the FrameNodes for the
  //    process are often gone already. The reason is that FrameNodes are
  //    removed on process exit by another RenderProcessHostObserver, and the
  //    observers can fire in any order.
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

  CHECK(!measurement_interval_start.is_null());
  const base::TimeDelta measurement_interval =
      measurement_interval_end - measurement_interval_start;
  if (measurement_interval.is_zero()) {
    // No time has passed to measure.
    return;
  }
  CHECK(measurement_interval.is_positive());

  // Assume a measurement period running from time A
  // (`measurement_interval_start`) to time B (`measurement_interval_end`).
  //
  // Let CPU(T) be the cpu measurement at time T.
  //
  // Note that the process is only measured after it's passed to the graph,
  // which is shortly after it's created, so at "process creation time" C,
  // CPU(C) may have a small value instead of 0. On the first call to
  // MeasureAndDistributeCPUUsage(), `most_recent_measurement_` will be CPU(C).
  //
  // There are 4 cases:
  //
  // 1. The process is created at time C, between A and B.
  //
  // This snapshot should include 0% CPU for time A..C, and the measured % of
  // CPU for time C..B.
  //
  // A    C         B
  // |----+---------|
  // | 0% |   X%    |
  //
  // The overall CPU usage at this snapshot is (CPU(B) - CPU(C)) / (B-A)
  // CPU(B) = GetCumulativeCPUUsage()
  // CPU(C) = `most_recent_measurement_`
  //
  // 2. The process existed for the entire duration A..B.
  //
  // This snapshot should include the measured % of CPU for the whole time
  // A..B.
  //
  // A              B
  // |--------------|
  // |      X%      |
  //
  // The overall CPU usage at this snapshot is (CPU(B) - CPU(A)) / (B-A)
  // CPU(B) = GetCumulativeCPUUsage()
  // CPU(A) = `most_recent_measurement_`
  //
  // 3. Process created before time A, but exited at time D, between A and B.
  //
  // The snapshot should include the measured % of CPU for time A..D, and 0%
  // CPU for time D..B.
  //
  // A         D    B
  // |---------+----|
  // |    X%   | 0% |
  //
  // The overall CPU usage at this snapshot is (CPU(D) - CPU(A)) / (B-A)
  // CPU(D) = ChildProcessTerminationInfo::cpu_usage (currently unavailable)
  // CPU(A) = `most_recent_measurement_`
  //
  // 4. Process created at time C and exited at time D, both between A and B.
  //
  // The snapshot should include the measured % of CPU for time C..D, and 0%
  // CPU for the rest.
  //
  // A    C    D    B
  // |----+----+----|
  // | 0% | X% | 0% |
  //
  // The overall CPU usage at this snapshot is (CPU(D) - CPU(C) / (B-A)
  // CPU(D) = ChildProcessTerminationInfo::cpu_usage (currently unavailable)
  // CPU(C) = `most_recent_measurement_`
  //
  // In case 1 and case 2, the numerator is `GetCumulativeCPUUsage() -
  // most_recent_measurement_`. In case 3 and 4, GetCumulativeCPUUsage() will
  // return an error code.
  base::TimeDelta current_cpu_usage = delegate_->GetCumulativeCPUUsage();
  if (!current_cpu_usage.is_positive()) {
    // GetCumulativeCPUUsage() failed. Don't update the measurement state.
    // Most platforms return a zero TimeDelta on error, Linux returns a
    // negative.
    return;
  }
  const base::TimeDelta current_measurement =
      current_cpu_usage - most_recent_measurement_;
  most_recent_measurement_ = current_cpu_usage;

  const double current_cpu_proportion =
      current_measurement / measurement_interval;
  resource_attribution::SplitResourceAmongFramesAndWorkers(
      current_cpu_proportion, process_node,
      [&cpu_usage_map](const FrameNode* f, double cpu) {
        cpu_usage_map.emplace(f->GetResourceContext(), cpu);
      },
      [&cpu_usage_map](const WorkerNode* w, double cpu) {
        cpu_usage_map.emplace(w->GetResourceContext(), cpu);
      });
}

}  // namespace performance_manager::resource_attribution

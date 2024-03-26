// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/cpu_measurement_delegate.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/resource_attribution/cpu_measurement_monitor.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "components/performance_manager/resource_attribution/query_scheduler.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/common/process_type.h"

namespace resource_attribution {

namespace {

// A production CPUMeasurementDelegate that measures processes with
// base::ProcessMetrics.
class CPUMeasurementDelegateImpl final : public CPUMeasurementDelegate {
 public:
  explicit CPUMeasurementDelegateImpl(const ProcessNode* process_node);
  ~CPUMeasurementDelegateImpl() final = default;

  base::expected<base::TimeDelta, ProcessCPUUsageError> GetCumulativeCPUUsage()
      final;

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

base::expected<base::TimeDelta, CPUMeasurementDelegate::ProcessCPUUsageError>
CPUMeasurementDelegateImpl::GetCumulativeCPUUsage() {
  return process_metrics_->GetCumulativeCPUUsage();
}

// The default production factory for CPUMeasurementDelegateImpl objects.
class CPUMeasurementDelegateFactoryImpl final
    : public CPUMeasurementDelegate::Factory {
 public:
  CPUMeasurementDelegateFactoryImpl() = default;
  ~CPUMeasurementDelegateFactoryImpl() final = default;

  bool ShouldMeasureProcess(const ProcessNode* process_node) final;
  std::unique_ptr<CPUMeasurementDelegate> CreateDelegateForProcess(
      const ProcessNode* process_node) final;
};

bool CPUMeasurementDelegateFactoryImpl::ShouldMeasureProcess(
    const ProcessNode* process_node) {
  // The process start time is not available until the ProcessId is assigned.
  if (process_node->GetProcessId() == base::kNullProcessId) {
    return false;
  }
  // This can be called from OnProcessLifetimeChange after a process exits.
  // Only handle process start notifications (which is when the pid is
  // assigned), not exit notifications. Note the pid can be reassigned if a
  // process dies and a new one is started for the same ProcessNode - in that
  // case MonitorCPUUsage will reset the measurements and start monitoring the
  // new process from scratch.
  if (!process_node->GetProcess().IsValid()) {
    return false;
  }
  return true;
}

std::unique_ptr<CPUMeasurementDelegate>
CPUMeasurementDelegateFactoryImpl::CreateDelegateForProcess(
    const ProcessNode* process_node) {
  return std::make_unique<CPUMeasurementDelegateImpl>(process_node);
}

}  // namespace

// static
void CPUMeasurementDelegate::SetDelegateFactoryForTesting(Graph* graph,
                                                          Factory* factory) {
  auto* scheduler = internal::QueryScheduler::GetFromGraph(graph);
  CHECK(scheduler);
  scheduler
      ->GetCPUMonitorForTesting()                      // IN-TEST
      .SetDelegateFactoryForTesting(factory ? factory  // IN-TEST
                                            : GetDefaultFactory());
}

// static
CPUMeasurementDelegate::Factory* CPUMeasurementDelegate::GetDefaultFactory() {
  static base::NoDestructor<CPUMeasurementDelegateFactoryImpl> default_factory;
  return default_factory.get();
}

}  // namespace resource_attribution

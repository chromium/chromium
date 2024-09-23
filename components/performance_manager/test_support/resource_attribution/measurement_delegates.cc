// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/resource_attribution/measurement_delegates.h"

#include <map>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "content/public/common/process_type.h"

namespace resource_attribution {

using Graph = performance_manager::Graph;
using ProcessNode = performance_manager::ProcessNode;

SimulatedCPUMeasurementDelegateFactory::
    SimulatedCPUMeasurementDelegateFactory() = default;

SimulatedCPUMeasurementDelegateFactory::
    ~SimulatedCPUMeasurementDelegateFactory() {
  // Delete all delegates that are still owned by the factory.
  pending_cpu_delegates_.clear();

  // Now all delegates, owned and un-owned, should have been deleted.
  CHECK(simulated_cpu_delegates_.empty());
}

void SimulatedCPUMeasurementDelegateFactory::SetDefaultCPUUsage(
    SimulatedCPUUsage default_cpu_usage) {
  default_cpu_usage_ = default_cpu_usage;
}

void SimulatedCPUMeasurementDelegateFactory::SetRequireValidProcesses(
    bool require_valid) {
  require_valid_processes_ = require_valid;
}

SimulatedCPUMeasurementDelegate&
SimulatedCPUMeasurementDelegateFactory::GetDelegate(
    const ProcessNode* process_node) {
  // If a delegate already exists, use it.
  auto it = simulated_cpu_delegates_.find(process_node);
  if (it != simulated_cpu_delegates_.end()) {
    return *(it->second);
  }
  // Create a new delegate, saving it in `pending_cpu_delegates_` until someone
  // calls CreateDelegateForProcess().
  auto new_delegate = std::make_unique<SimulatedCPUMeasurementDelegate>(
      PassKey(), weak_factory_.GetSafeRef(), default_cpu_usage_);
  auto* delegate_ptr = new_delegate.get();
  simulated_cpu_delegates_.emplace(process_node, delegate_ptr);
  const auto [_, inserted] =
      pending_cpu_delegates_.emplace(process_node, std::move(new_delegate));
  CHECK(inserted);
  return *delegate_ptr;
}

bool SimulatedCPUMeasurementDelegateFactory::ShouldMeasureProcess(
    const ProcessNode* process_node) {
  if (require_valid_processes_) {
    // Delegate the decision to the production factory.
    return CPUMeasurementDelegate::GetDefaultFactory()->ShouldMeasureProcess(
        process_node);
  }
  return true;
}

std::unique_ptr<CPUMeasurementDelegate>
SimulatedCPUMeasurementDelegateFactory::CreateDelegateForProcess(
    const ProcessNode* process_node) {
  // If there was a delegate already created, use it.
  auto it = pending_cpu_delegates_.find(process_node);
  if (it != pending_cpu_delegates_.end()) {
    auto delegate = std::move(it->second);
    pending_cpu_delegates_.erase(it);
    CHECK_EQ(simulated_cpu_delegates_.at(process_node), delegate.get());
    return delegate;
  }
  // Create a new delegate.
  auto new_delegate = std::make_unique<SimulatedCPUMeasurementDelegate>(
      PassKey(), weak_factory_.GetSafeRef(), default_cpu_usage_);
  auto* delegate_ptr = new_delegate.get();
  simulated_cpu_delegates_.emplace(process_node, delegate_ptr);
  return new_delegate;
}

void SimulatedCPUMeasurementDelegateFactory::OnDelegateDeleted(
    base::PassKey<SimulatedCPUMeasurementDelegate>,
    SimulatedCPUMeasurementDelegate* delegate) {
  const size_t erased = std::erase_if(
      simulated_cpu_delegates_,
      [delegate](const auto& entry) { return delegate == entry.second; });
  CHECK_EQ(erased, 1U);
}

SimulatedCPUMeasurementDelegate::SimulatedCPUMeasurementDelegate(
    base::PassKey<SimulatedCPUMeasurementDelegateFactory>,
    base::SafeRef<SimulatedCPUMeasurementDelegateFactory> factory,
    SimulatedCPUUsage initial_usage)
    : factory_(factory) {
  SetCPUUsage(initial_usage);
}

SimulatedCPUMeasurementDelegate::~SimulatedCPUMeasurementDelegate() {
  factory_->OnDelegateDeleted(PassKey(), this);
}

void SimulatedCPUMeasurementDelegate::SetCPUUsage(SimulatedCPUUsage usage,
                                                  base::TimeTicks start_time) {
  if (!cpu_usage_periods_.empty()) {
    cpu_usage_periods_.back().end_time = start_time;
  }
  cpu_usage_periods_.push_back(CPUUsagePeriod{
      .start_time = start_time,
      .cpu_usage = usage,
  });
}

base::expected<base::TimeDelta, CPUMeasurementDelegate::ProcessCPUUsageError>
SimulatedCPUMeasurementDelegate::GetCumulativeCPUUsage() {
  if (error_.has_value()) {
    return base::unexpected(error_.value());
  }
  base::TimeDelta cumulative_usage;
  for (const auto& usage_period : cpu_usage_periods_) {
    CHECK(!usage_period.start_time.is_null());
    // The last interval in the list will have no end time.
    const base::TimeTicks end_time = usage_period.end_time.is_null()
                                         ? base::TimeTicks::Now()
                                         : usage_period.end_time;
    CHECK_GE(end_time, usage_period.start_time);
    cumulative_usage +=
        (end_time - usage_period.start_time) * usage_period.cpu_usage;
  }
  return base::ok(cumulative_usage);
}

FakeMemoryMeasurementDelegateFactory::FakeMemoryMeasurementDelegateFactory() =
    default;

FakeMemoryMeasurementDelegateFactory::~FakeMemoryMeasurementDelegateFactory() =
    default;

std::unique_ptr<MemoryMeasurementDelegate>
FakeMemoryMeasurementDelegateFactory::CreateDelegate(Graph*) {
  return std::make_unique<FakeMemoryMeasurementDelegate>(
      PassKey(), weak_factory_.GetSafeRef());
}

FakeMemoryMeasurementDelegate::FakeMemoryMeasurementDelegate(
    base::PassKey<FakeMemoryMeasurementDelegateFactory>,
    base::SafeRef<FakeMemoryMeasurementDelegateFactory> factory)
    : factory_(factory) {}

FakeMemoryMeasurementDelegate::~FakeMemoryMeasurementDelegate() = default;

void FakeMemoryMeasurementDelegate::RequestMemorySummary(
    base::OnceCallback<void(MemorySummaryMap)> callback) {
  std::move(callback).Run(factory_->memory_summaries());
}

}  // namespace resource_attribution

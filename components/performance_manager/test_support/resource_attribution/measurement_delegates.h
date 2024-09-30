// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_MEASUREMENT_DELEGATES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_MEASUREMENT_DELEGATES_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/public/resource_attribution/cpu_measurement_delegate.h"
#include "components/performance_manager/public/resource_attribution/memory_measurement_delegate.h"

namespace performance_manager {
class ProcessNode;
}

namespace resource_attribution {

class SimulatedCPUMeasurementDelegate;

// The proportion of CPU used over time as a fraction, on the same scale as
// ProcessMetrics::GetPlatformIndependentCPUUsage: 0% to 100% *
// SysInfo::NumberOfProcessors().
//
// Since tests should be independent of the number of processors, it's usually
// convenient to use a range of 0.0 to 1.0 (for 100%), simulating a
// single-processor system. But if the code under test scales CPU measurements
// by SysInfo::NumberOfProcessors(), it's better to set the simulated usage to
// SysInfo::NumberOfProcessors() * a fraction, so that the code under test
// gets the same results after scaling on every system.
using SimulatedCPUUsage = double;

// A factory that manages SimulatedCPUMeasurementDelegate instances. Embed an
// instance of this in a unit test, and pass it to
// CPUMeasurementDelegate::SetDelegateFactoryForTesting(). The caller must
// ensure that the SimulatedCPUMeasurementDelegateFactory outlives all callers
// of CreateDelegateForProcess().
class SimulatedCPUMeasurementDelegateFactory final
    : public CPUMeasurementDelegate::Factory {
 public:
  using PassKey = base::PassKey<SimulatedCPUMeasurementDelegateFactory>;

  SimulatedCPUMeasurementDelegateFactory();
  ~SimulatedCPUMeasurementDelegateFactory() final;

  SimulatedCPUMeasurementDelegateFactory(
      const SimulatedCPUMeasurementDelegateFactory&) = delete;
  SimulatedCPUMeasurementDelegateFactory& operator=(
      const SimulatedCPUMeasurementDelegateFactory&) = delete;

  // Sets the default CPU usage reported by delegates created by this factory.
  // If this is not called, all delegates will report 100% CPU.
  void SetDefaultCPUUsage(SimulatedCPUUsage default_cpu_usage);

  // By default, delegates created by this factory can return simulated
  // measurements for ProcessNodes without a valid base::Process backing them.
  // This is useful because most tests use shim ProcessNodes and only need the
  // delegates to provide simple test data. Calling this with `require_valid`
  // true will apply all the valid process checks used in production, for tests
  // that do strict validation of CPU measurements.
  void SetRequireValidProcesses(bool require_valid);

  // Returns a delegate for `process_node`. This can be used to set initial
  // values on delegates before the code under test gets a pointer to them using
  // CreateDelegateForProcess().
  SimulatedCPUMeasurementDelegate& GetDelegate(
      const performance_manager::ProcessNode* process_node);

  // CPUMeasurementDelegate::Factory:

  bool ShouldMeasureProcess(
      const performance_manager::ProcessNode* process_node) final;

  std::unique_ptr<CPUMeasurementDelegate> CreateDelegateForProcess(
      const performance_manager::ProcessNode* process_node) final;

  // Private implementation guarded by PassKey:

  // Called by `delegate` when it's deleted.
  void OnDelegateDeleted(base::PassKey<SimulatedCPUMeasurementDelegate>,
                         SimulatedCPUMeasurementDelegate* delegate);

 private:
  // The default CPU usage of delegates created by this factory.
  SimulatedCPUUsage default_cpu_usage_ = 1.0;

  // If true, this factory won't create delegates for ProcessNodes without a
  // valid base::Process.
  bool require_valid_processes_ = false;

  // Map of ProcessNode to CPUMeasurementDelegate that simulates that process.
  // The delegates are owned by `pending_cpu_delegates_` when they're created,
  // then ownership is passed to the caller of TakeDelegate().
  std::map<const performance_manager::ProcessNode*,
           raw_ptr<SimulatedCPUMeasurementDelegate, CtnExperimental>>
      simulated_cpu_delegates_;

  // CPUMeasurementDelegates that have been created but not passed to the caller
  // of TakeDelegate() yet.
  std::map<const performance_manager::ProcessNode*,
           std::unique_ptr<SimulatedCPUMeasurementDelegate>>
      pending_cpu_delegates_;

  base::WeakPtrFactory<SimulatedCPUMeasurementDelegateFactory> weak_factory_{
      this};
};

// State of a simulated process for CPU measurements.
class SimulatedCPUMeasurementDelegate final : public CPUMeasurementDelegate {
 public:
  using PassKey = base::PassKey<SimulatedCPUMeasurementDelegate>;

  // Only SimulatedCPUMeasurementDelegateFactory can call the constructor.
  SimulatedCPUMeasurementDelegate(
      base::PassKey<SimulatedCPUMeasurementDelegateFactory>,
      base::SafeRef<SimulatedCPUMeasurementDelegateFactory> factory,
      SimulatedCPUUsage initial_usage);

  ~SimulatedCPUMeasurementDelegate() final;

  SimulatedCPUMeasurementDelegate(const SimulatedCPUMeasurementDelegate&) =
      delete;
  SimulatedCPUMeasurementDelegate& operator=(
      const SimulatedCPUMeasurementDelegate&) = delete;

  // Sets the CPU usage to `usage`, starting at `start_time` (which must be
  // later than any `start_time` previously used).
  void SetCPUUsage(SimulatedCPUUsage usage,
                   base::TimeTicks start_time = base::TimeTicks::Now());

  // Sets an error to report from GetCumulativeCPUUsage(). If `error` is
  // nullopt, GetCumulativeCPUUsage() will instead return a value accumulated
  // by calls to SetCPUUsage().
  void SetError(std::optional<ProcessCPUUsageError> error) { error_ = error; }

  // CPUMeasurementDelegate:

  // Returns the simulated CPU usage of the process by summing
  // `cpu_usage_periods`, or an error code if SetError() was called.
  base::expected<base::TimeDelta, ProcessCPUUsageError> GetCumulativeCPUUsage()
      final;

 private:
  struct CPUUsagePeriod {
    base::TimeTicks start_time;
    base::TimeTicks end_time;
    SimulatedCPUUsage cpu_usage;
  };

  // The factory that created this delegate. The factory's destructor CHECK's
  // that all delegates it created have already been destroyed.
  base::SafeRef<SimulatedCPUMeasurementDelegateFactory> factory_;

  // List of periods of varying CPU usage.
  std::vector<CPUUsagePeriod> cpu_usage_periods_;

  // If this is not nullopt, GetCumulativeCPUUsage() will ignore
  // `cpu_usage_periods` and return it as an error instead.
  std::optional<ProcessCPUUsageError> error_;
};

// A factory that manages FakeMemoryMeasurementDelegate instances. Embed an
// instance of this in a unit test, and pass it to
// MemoryMeasurementDelegate::SetDelegateFactoryForTesting(). The caller must
// ensure that the FakeMemoryMeasurementDelegateFactory outlives all callers
// of CreateDelegate().
class FakeMemoryMeasurementDelegateFactory final
    : public MemoryMeasurementDelegate::Factory {
 public:
  using PassKey = base::PassKey<FakeMemoryMeasurementDelegateFactory>;

  FakeMemoryMeasurementDelegateFactory();
  ~FakeMemoryMeasurementDelegateFactory() final;

  FakeMemoryMeasurementDelegateFactory(
      const FakeMemoryMeasurementDelegateFactory&) = delete;
  FakeMemoryMeasurementDelegateFactory& operator=(
      const FakeMemoryMeasurementDelegateFactory&) = delete;

  // Returns a reference to the map of memory measurements that will be returned
  // by delegates created by this factory. Callers can modify the map through
  // the reference. To simulate a measurement error, use an empty map.
  MemoryMeasurementDelegate::MemorySummaryMap& memory_summaries() {
    return memory_summaries_;
  }

  // MemoryMeasurementDelegate::Factory:

  std::unique_ptr<MemoryMeasurementDelegate> CreateDelegate(
      performance_manager::Graph*) final;

 private:
  // The MemorySummary results returned by delegates created by this factory.
  MemoryMeasurementDelegate::MemorySummaryMap memory_summaries_;

  base::WeakPtrFactory<FakeMemoryMeasurementDelegateFactory> weak_factory_{
      this};
};

// A MemoryMeasurementDelegate that returns fake results.
class FakeMemoryMeasurementDelegate final : public MemoryMeasurementDelegate {
 public:
  using PassKey = base::PassKey<FakeMemoryMeasurementDelegate>;

  // Only FakeMemoryMeasurementDelegateFactory can call the constructor.
  FakeMemoryMeasurementDelegate(
      base::PassKey<FakeMemoryMeasurementDelegateFactory>,
      base::SafeRef<FakeMemoryMeasurementDelegateFactory> factory);

  ~FakeMemoryMeasurementDelegate() final;

  FakeMemoryMeasurementDelegate(const FakeMemoryMeasurementDelegate&) = delete;
  FakeMemoryMeasurementDelegate& operator=(
      const FakeMemoryMeasurementDelegate&) = delete;

  // MemoryMeasurementDelegate:

  // Invokes `callback` with the fake measurements returned by
  // FakeMemoryMeasurementDelegateFactory::memory_summaries().
  void RequestMemorySummary(
      base::OnceCallback<void(MemorySummaryMap)> callback) final;

 private:
  // The factory that created this delegate.
  base::SafeRef<FakeMemoryMeasurementDelegateFactory> factory_;
};

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_MEASUREMENT_DELEGATES_H_

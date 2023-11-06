// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_SIMULATED_CPU_MEASUREMENT_DELEGATE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_SIMULATED_CPU_MEASUREMENT_DELEGATE_H_

#include "components/performance_manager/public/resource_attribution/cpu_measurement_delegate.h"

#include <map>
#include <memory>
#include <vector>

#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager {
class ProcessNode;
}

namespace performance_manager::resource_attribution {

class SimulatedCPUMeasurementDelegate;

// A factory that manages SimulatedCPUMeasurementDelegate instances. Embed an
// instance of this in a unit test, and pass the result of GetFactoryCallback()
// to CPUMeasurementDelegate::SetDelegateFactoryForTesting().
class SimulatedCPUMeasurementDelegateFactory {
 public:
  using PassKey = base::PassKey<SimulatedCPUMeasurementDelegateFactory>;

  SimulatedCPUMeasurementDelegateFactory();
  ~SimulatedCPUMeasurementDelegateFactory();

  SimulatedCPUMeasurementDelegateFactory(
      const SimulatedCPUMeasurementDelegateFactory&) = delete;
  SimulatedCPUMeasurementDelegateFactory& operator=(
      const SimulatedCPUMeasurementDelegateFactory&) = delete;

  // Sets the default CPU usage reported by delegates created by this factory.
  void SetDefaultCPUUsage(double default_cpu_usage);

  // Returns a callback that can be passed to
  // CPUMeasurementDelegate::SetDelegateFactoryForTesting to use
  // SimulatedCPUMeasurementDelegates in tests. The caller must ensure that the
  // SimulatedCPUMeasurementDelegateFactory outlives all users of the callback,
  // otherwise it will crash when it's invoked.
  CPUMeasurementDelegate::FactoryCallback GetFactoryCallback();

  // Returns a delegate for `process_node`. This can be used to set initial
  // values on delegates before the code under test gets a pointer to them using
  // the factory callback returned from GetCallback().
  SimulatedCPUMeasurementDelegate& GetDelegate(const ProcessNode* process_node);

  // Called by `delegate` when it's deleted.
  void OnDelegateDeleted(base::PassKey<SimulatedCPUMeasurementDelegate>,
                         SimulatedCPUMeasurementDelegate* delegate);

  // Factory function for SimulatedCPUMeasurementDelegate objects. Passes
  // ownership of the delegate for `process_node` to the caller, creating a
  // delegate if none exists yet. Exposed through GetFactoryCallback().
  std::unique_ptr<CPUMeasurementDelegate> TakeDelegate(
      const ProcessNode* process_node);

  // The default CPU usage of delegates created by this factory.
  double default_cpu_usage_ = 0.0;

  // Map of ProcessNode to CPUMeasurementDelegate that simulates that process.
  // The delegates are owned by `pending_cpu_delegates_` when they're created,
  // then ownership is passed to the caller of TakeDelegate().
  std::map<const ProcessNode*, SimulatedCPUMeasurementDelegate*>
      simulated_cpu_delegates_;

  // CPUMeasurementDelegates that have been created but not passed to the caller
  // of TakeDelegate() yet.
  std::map<const ProcessNode*, std::unique_ptr<SimulatedCPUMeasurementDelegate>>
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
      base::SafeRef<SimulatedCPUMeasurementDelegateFactory> factory);

  ~SimulatedCPUMeasurementDelegate() final;

  SimulatedCPUMeasurementDelegate(const SimulatedCPUMeasurementDelegate&) =
      delete;
  SimulatedCPUMeasurementDelegate& operator=(
      const SimulatedCPUMeasurementDelegate&) = delete;

  // Sets the CPU usage to `usage`, starting at `start_time` (which must be
  // later than any `start_time` previously used).
  void SetCPUUsage(double usage,
                   base::TimeTicks start_time = base::TimeTicks::Now());

  // Sets the process to have an error that will be reported as `usage_error`.
  void SetError(base::TimeDelta usage_error);

  // Clears any error that was set with SetCPUUsageError().
  void ClearError();

  // CPUMeasurementDelegate interface:

  // Returns the simulated CPU usage of the process by summing
  // `cpu_usage_periods`.
  base::TimeDelta GetCumulativeCPUUsage() final;

 private:
  struct CPUUsagePeriod {
    base::TimeTicks start_time;
    base::TimeTicks end_time;
    double cpu_usage;
  };

  // The factory that created this delegate. The factory's destructor CHECK's
  // that all delegates it created have already been destroyed.
  base::SafeRef<SimulatedCPUMeasurementDelegateFactory> factory_;

  // List of periods of varying CPU usage.
  std::vector<CPUUsagePeriod> cpu_usage_periods_;

  // If not nullopt, GetCumulativeCPUUsage() will ignore `cpu_usage_periods` and
  // return this value to simulate an error.
  absl::optional<base::TimeDelta> usage_error_;
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_SIMULATED_CPU_MEASUREMENT_DELEGATE_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_DELEGATE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_DELEGATE_H_

#include <memory>

#include "base/process/process_metrics.h"
#include "base/time/time.h"
#include "base/types/expected.h"

namespace performance_manager {
class Graph;
class ProcessNode;
}  // namespace performance_manager

namespace resource_attribution {

// A shim that Resource Attribution queries use to request CPU measurements for
// a process. A new CPUMeasurementDelegate object will be created for each
// ProcessNode to be measured. Public so that users of the API can inject a test
// override by passing a factory object to SetDelegateFactoryForTesting().
class CPUMeasurementDelegate {
 public:
  class Factory;

  // Export ProcessCPUUsageError so callers don't need to include
  // "base/process/process_metrics.h" for the definition.
  using ProcessCPUUsageError = base::ProcessCPUUsageError;

  // The given `factory` will be used to create a CPUMeasurementDelegate for
  // each ProcessNode in `graph` to be measured. The factory object must outlive
  // the graph. Usually it's owned by the test harness. nullptr will cause the
  // factory returned by GetDefaultFactory() to be used.
  static void SetDelegateFactoryForTesting(performance_manager::Graph* graph,
                                           Factory* factory);

  // Returns the default factory to use in production.
  static Factory* GetDefaultFactory();

  CPUMeasurementDelegate() = default;
  virtual ~CPUMeasurementDelegate() = default;

  // Requests CPU usage for the process. Should match the semantics of
  // ProcessMetrics::GetCumulativeCPUUsage().
  virtual base::expected<base::TimeDelta, ProcessCPUUsageError>
  GetCumulativeCPUUsage() = 0;
};

class CPUMeasurementDelegate::Factory {
 public:
  virtual ~Factory() = default;

  // Returns true iff a CPUMeasurementDelegate should be created for
  // `process_node`. The production factory returns true to measure
  // renderer processes with a valid (running) base::Process and a
  // base::ProcessId assigned.
  virtual bool ShouldMeasureProcess(
      const performance_manager::ProcessNode* process_node) = 0;

  // Creates a CPUMeasurementDelegate for `process_node`. This should only be
  // called if ShouldMeasureProcess(process_node) returns true.
  virtual std::unique_ptr<CPUMeasurementDelegate> CreateDelegateForProcess(
      const performance_manager::ProcessNode* process_node) = 0;
};

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_DELEGATE_H_

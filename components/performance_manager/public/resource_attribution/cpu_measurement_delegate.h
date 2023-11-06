// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_DELEGATE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_DELEGATE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace performance_manager {
class Graph;
class ProcessNode;
}  // namespace performance_manager

namespace performance_manager::resource_attribution {

// A shim that Resource Attribution queries use to request CPU measurements for
// a process. A new CPUMeasurementDelegate object will be created for each
// ProcessNode to be measured. Public so that users of the API can inject a test
// override by passing a factory callback to SetDelegateFactoryForTesting().
class CPUMeasurementDelegate {
 public:
  using FactoryCallback =
      base::RepeatingCallback<std::unique_ptr<CPUMeasurementDelegate>(
          const ProcessNode*)>;

  // The given `factory_callback` will be called to create a
  // CPUMeasurementDelegate for each ProcessNode in `graph` to be measured.
  static void SetDelegateFactoryForTesting(Graph* graph,
                                           FactoryCallback factory_callback);

  CPUMeasurementDelegate() = default;
  virtual ~CPUMeasurementDelegate() = default;

  // Requests CPU usage for the process. This is [[nodiscard]] to match the
  // semantics of ProcessMetrics::GetCumulativeCPUUsage().
  [[nodiscard]] virtual base::TimeDelta GetCumulativeCPUUsage() = 0;
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_DELEGATE_H_

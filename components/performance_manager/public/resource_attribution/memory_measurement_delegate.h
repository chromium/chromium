// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_MEMORY_MEASUREMENT_DELEGATE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_MEMORY_MEASUREMENT_DELEGATE_H_

#include <compare>
#include <map>
#include <memory>

#include "base/functional/callback_forward.h"

namespace performance_manager {
class Graph;
}

namespace resource_attribution {

class ProcessContext;

// A shim that Resource Attribution queries use to request memory measurements.
// Public so that users of the API can inject a test override by passing a
// factory object to SetDelegateFactoryForTesting().
class MemoryMeasurementDelegate {
 public:
  class Factory;

  // The minimal results returned for a process memory measurement.
  // MemoryMeasurementProvider will wrap this in a full MemorySummaryResult.
  struct MemorySummaryMeasurement {
    uint64_t resident_set_size_kb = 0;
    uint64_t private_footprint_kb = 0;

    // Division operator required by SplitResourceAmongFramesAndWorkers().
    constexpr MemorySummaryMeasurement operator/(size_t divisor) {
      return MemorySummaryMeasurement{
          .resident_set_size_kb = resident_set_size_kb / divisor,
          .private_footprint_kb = private_footprint_kb / divisor};
    }

    friend constexpr auto operator<=>(const MemorySummaryMeasurement&,
                                      const MemorySummaryMeasurement&) =
        default;
    friend constexpr bool operator==(const MemorySummaryMeasurement&,
                                     const MemorySummaryMeasurement&) = default;
  };

  using MemorySummaryMap = std::map<ProcessContext, MemorySummaryMeasurement>;

  // The given `factory` will be used to create a MemoryMeasurementDelegate to
  // measure ProcessNodes in `graph`. The factory object must outlive the graph.
  // Usually it's owned by the test harness. nullptr will cause the factory
  // returned by GetDefaultFactory() to be used.
  static void SetDelegateFactoryForTesting(performance_manager::Graph* graph,
                                           Factory* factory);

  // Returns the default factory to use in production.
  static Factory* GetDefaultFactory();

  virtual ~MemoryMeasurementDelegate() = default;

  // Requests a memory summary for all processes. `callback` will be invoked
  // with the results, or an empty map on error.
  virtual void RequestMemorySummary(
      base::OnceCallback<void(MemorySummaryMap)>) = 0;
};

class MemoryMeasurementDelegate::Factory {
 public:
  virtual ~Factory() = default;

  // Creates a MemoryMeasurementDelegate for all ProcessNodes in `graph`.
  virtual std::unique_ptr<MemoryMeasurementDelegate> CreateDelegate(
      performance_manager::Graph* graph) = 0;
};

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_MEMORY_MEASUREMENT_DELEGATE_H_

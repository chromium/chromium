// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_MEMORY_MEASUREMENT_DELEGATE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_MEMORY_MEASUREMENT_DELEGATE_H_

#include <stddef.h>
#include <stdint.h>

#include <compare>
#include <map>
#include <memory>

#include "base/byte_count.h"
#include "base/containers/variant_map.h"
#include "base/functional/callback_forward.h"
#include "components/performance_manager/public/resource_attribution/process_context.h"

namespace performance_manager {
class Graph;
}

namespace resource_attribution {

// A shim that Resource Attribution queries use to request memory measurements.
// Public so that users of the API can inject a test override by passing a
// factory object to SetDelegateFactoryForTesting().
class MemoryMeasurementDelegate {
 public:
  class Factory;

  // The minimal results returned for a process memory measurement. The default
  // implementation fills this in from a memory_instrumentation.mojom.OSMemDump.
  // See the comments in memory_instrumentation.mojom for more details.
  //
  // MemoryMeasurementProvider will wrap this in a full MemorySummaryResult.
  struct MemorySummaryMeasurement {
    base::ByteCount resident_set_size;
    base::ByteCount private_footprint;

    // Only populated by default on Linux, ChromeOS and Android.
    base::ByteCount private_swap;

    // Division operator required by SplitResourceAmongFramesAndWorkers().
    constexpr MemorySummaryMeasurement operator/(size_t divisor) {
      return MemorySummaryMeasurement{
          .resident_set_size = resident_set_size / divisor,
          .private_footprint = private_footprint / divisor,
          .private_swap = private_swap / divisor};
    }

    friend constexpr auto operator<=>(const MemorySummaryMeasurement&,
                                      const MemorySummaryMeasurement&) =
        default;
    friend constexpr bool operator==(const MemorySummaryMeasurement&,
                                     const MemorySummaryMeasurement&) = default;
  };

  // TODO(crbug.com/433462519): Replace this with a concrete map type after
  // using VariantMap to measure the performance of various impls.
  using MemorySummaryMap =
      base::VariantMap<ProcessContext, MemorySummaryMeasurement>;

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

 protected:
  // Allow implementations to use MemoryMeasurementDelegate's passkey to create
  // MemorySummaryMaps.
  static MemorySummaryMap CreateMemorySummaryMap();
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

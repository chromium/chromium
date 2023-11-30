// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_MEMORY_MEASUREMENT_PROVIDER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_MEMORY_MEASUREMENT_PROVIDER_H_

#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/resource_attribution/memory_measurement_delegate.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"

namespace performance_manager {
class Graph;
}

namespace performance_manager::resource_attribution {

class MemoryMeasurementProvider {
 public:
  explicit MemoryMeasurementProvider(Graph* graph);
  ~MemoryMeasurementProvider();

  MemoryMeasurementProvider(const MemoryMeasurementProvider& other) = delete;
  MemoryMeasurementProvider& operator=(const MemoryMeasurementProvider&) =
      delete;

  // The given `factory` will be used to create a MemoryMeasurementDelegate for
  // ProcessNodes to be measured.
  void SetDelegateFactoryForTesting(
      MemoryMeasurementDelegate::Factory* factory);

  // Results of a memory summary query. Each QueryResult variant will contain a
  // MemorySummaryResult.
  using ResultCallback =
      base::OnceCallback<void(std::map<ResourceContext, QueryResult>)>;

  // Requests memory summaries for all processes. `callback` will be invoked
  // with the results.
  void RequestMemorySummary(ResultCallback callback);

 private:
  static void OnMemorySummary(
      ResultCallback callback,
      MemoryMeasurementDelegate::MemorySummaryMap process_summaries);

  SEQUENCE_CHECKER(sequence_checker_);

  // Delegate that measures memory usage of ProcessNodes.
  std::unique_ptr<MemoryMeasurementDelegate> measurement_delegate_
      GUARDED_BY_CONTEXT(sequence_checker_);

  raw_ptr<Graph> graph_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_MEMORY_MEASUREMENT_PROVIDER_H_

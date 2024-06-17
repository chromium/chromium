// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_SCHEDULER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_SCHEDULER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "components/performance_manager/resource_attribution/cpu_measurement_monitor.h"
#include "components/performance_manager/resource_attribution/memory_measurement_provider.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"

namespace performance_manager {
class Graph;
}

namespace resource_attribution {
class ContextCollection;
}

namespace resource_attribution::internal {

class QueryParams;

// QueryScheduler keeps track of all queries for a particular resource type and
// owns the machinery that performs measurements.
class QueryScheduler
    : public performance_manager::GraphOwnedAndRegistered<QueryScheduler> {
 public:
  QueryScheduler();
  ~QueryScheduler() override;

  QueryScheduler(const QueryScheduler&) = delete;
  QueryScheduler& operator=(const QueryScheduler&) = delete;

  base::WeakPtr<QueryScheduler> GetWeakPtr();

  // Invokes `callback` on the PM sequence with a pointer to the registered
  // QueryScheduler.
  static void CallWithScheduler(
      base::OnceCallback<void(QueryScheduler*)> callback,
      const base::Location& location = base::Location::Current());

  // Adds a scoped query for `query_params`. Increases the query count for all
  // resource types and contexts referenced in `query_params`.
  void AddScopedQuery(QueryParams* query_params);

  // Decreases the query count for all resource types and contexts referenced in
  // `query_params` and deletes `query_params`.
  void RemoveScopedQuery(std::unique_ptr<QueryParams> query_params);

  // Notifies the scheduler that a scoped query will begin repeatedly requesting
  // results. The query now needs a QueryId to track what results it has
  // received.
  void StartRepeatingQuery(QueryParams* query_params);

  // Requests the latest results for the given `query_params`, and passes them
  // to `callback`.
  void RequestResults(const QueryParams& query_params,
                      base::OnceCallback<void(const QueryResultMap&)> callback);

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) final;
  void OnTakenFromGraph(Graph* graph) final;

  // Gives tests direct access to `cpu_monitor_`.
  CPUMeasurementMonitor& GetCPUMonitorForTesting();

  // Gives tests direct access to `memory_provider_`.
  MemoryMeasurementProvider& GetMemoryProviderForTesting();

  // Gives tests access to the query count for `resource_type`.
  uint32_t GetQueryCountForTesting(ResourceType resource_type) const;

  // Logs metrics on Resource Attribution's memory usage to UMA.
  void RecordMemoryMetrics();

 private:
  // Increases the CPU query count. `cpu_monitor_` will start monitoring CPU
  // usage when the count > 0.
  void AddCPUQuery();

  // Decreases the CPU query count. `cpu_monitor_` will stop monitoring CPU
  // usage when the count == 0.
  void RemoveCPUQuery();

  // Increases the memory query count.
  void AddMemoryQuery();

  // Decreases the memory query count.
  void RemoveMemoryQuery();

  // Invoked from RequestResults when all results are received. `all_results`
  // will contain a separate result map for each ResourceType that was
  // requested.
  void OnResultsReceived(
      const ContextCollection& contexts,
      base::OnceCallback<void(const QueryResultMap&)> callback,
      std::vector<QueryResultMap> all_results);

  SEQUENCE_CHECKER(sequence_checker_);

  // CPU measurement machinery.
  CPUMeasurementMonitor cpu_monitor_ GUARDED_BY_CONTEXT(sequence_checker_);
  uint32_t cpu_query_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // Memory measurement machinery.
  std::optional<MemoryMeasurementProvider> memory_provider_
      GUARDED_BY_CONTEXT(sequence_checker_);
  uint32_t memory_query_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  base::WeakPtrFactory<QueryScheduler> weak_factory_{this};
};

}  // namespace resource_attribution::internal

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_SCHEDULER_H_

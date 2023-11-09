// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_SCHEDULER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_SCHEDULER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/resource_attribution/cpu_measurement_monitor.h"

namespace base {
class TaskRunner;
}

namespace performance_manager::resource_attribution {

namespace internal {
struct QueryParams;
}

// QueryScheduler keeps track of all queries for a particular resource type and
// owns the machinery that performs measurements.
class QueryScheduler : public GraphRegisteredImpl<QueryScheduler>,
                       public GraphOwned {
 public:
  QueryScheduler();
  ~QueryScheduler() override;

  QueryScheduler(const QueryScheduler&) = delete;
  QueryScheduler& operator=(const QueryScheduler&) = delete;

  base::WeakPtr<QueryScheduler> GetWeakPtr();

  // Invokes `callback` on the PM sequence with a pointer to the registered
  // QueryScheduler.
  static void CallOnGraphWithScheduler(
      base::OnceCallback<void(QueryScheduler*)> callback,
      const base::Location& location = base::Location::Current());

  // Increases the CPU query count. `cpu_monitor_` will start monitoring CPU
  // usage when the count > 0.
  // TODO(crbug.com/1471683): Make this private. It should only be called by
  // AddScopedQuery().
  void AddCPUQuery();

  // Decreases the CPU query count. `cpu_monitor_` will stop monitoring CPU
  // usage when the count == 0.
  // TODO(crbug.com/1471683): Make this private. It should only be called by
  // RemoveScopedQuery().
  void RemoveCPUQuery();

  // Adds a scoped query for `query_params`. Increases the query count for all
  // resource types and contexts referenced in `query_params`.
  void AddScopedQuery(internal::QueryParams* query_params);

  // Decreases the query count for all resource types and contexts referenced in
  // `query_params` and deletes `query_params`.
  void RemoveScopedQuery(std::unique_ptr<internal::QueryParams> query_params);

  // Requests the latest CPU measurements from `cpu_monitor_`, and posts them
  // to `callback` on `task_runner`. Asserts that the CPU query count > 0.
  // TODO(crbug.com/1471683): Replace with a general RequestResults that handles
  // any QueryParams.
  void RequestCPUResults(
      base::OnceCallback<void(const QueryResultMap&)> callback,
      scoped_refptr<base::TaskRunner> task_runner);

  // Gives tests direct access to `cpu_monitor_`.
  CPUMeasurementMonitor& GetCPUMonitorForTesting();

  // GraphOwned overrides:
  void OnPassedToGraph(Graph* graph) final;
  void OnTakenFromGraph(Graph* graph) final;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<Graph> graph_ GUARDED_BY_CONTEXT(sequence_checker_);

  // CPU measurement machinery.
  CPUMeasurementMonitor cpu_monitor_ GUARDED_BY_CONTEXT(sequence_checker_);
  uint32_t cpu_query_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  base::WeakPtrFactory<QueryScheduler> weak_factory_{this};
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_SCHEDULER_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_SCHEDULER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_SCHEDULER_H_

#include "base/functional/callback_forward.h"
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

class CPUMeasurementMonitor;

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

  // CPU measurement accessors.

  // Increases the CPU query count. `cpu_monitor_` will start monitoring CPU
  // usage when the count > 0.
  void AddCPUQuery();

  // Decreases the CPU query count. `cpu_monitor_` will stop monitoring CPU
  // usage when the count == 0.
  void RemoveCPUQuery();

  // Requests the latest CPU measurements from `cpu_monitor_`, and posts them
  // to `callback` on `task_runner`. Asserts that the CPU query count > 0.
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

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/query_scheduler.h"

#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/check_op.h"
#include "base/containers/enum_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/optional_util.h"
#include "base/types/pass_key.h"
#include "base/types/variant_util.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "components/performance_manager/resource_attribution/context_collection.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "components/performance_manager/resource_attribution/query_params.h"

namespace resource_attribution::internal {

namespace {

// A global singleton that holds the TaskRunner the QueryScheduler runs on. In
// production this is the PM graph sequence, but in unit tests it can be the
// main thread. The TaskRunner is set when the QueryScheduler is passed to the
// PM graph, so it will be reset between tests and can be null during graph
// teardown.
//
// This is used instead of CallOnGraph because there are many
// resource attribution tests that use GraphTestHarness and mock graphs instead
// of PerformanceManagerTestHarness, that don't use any PerformanceManager hooks
// except for the QueryScheduler.
class SchedulerTaskRunner {
 public:
  SchedulerTaskRunner(const SchedulerTaskRunner&) = delete;
  SchedulerTaskRunner operator=(const SchedulerTaskRunner&) = delete;

  static SchedulerTaskRunner* GetInstance();

  // Registers the current sequence's TaskRunner as the QueryScheduler
  // TaskRunner.
  void OnSchedulerPassedToGraph(Graph* graph);

  // Clears the QueryScheduler TaskRunner.
  void OnSchedulerTakenFromGraph(Graph* graph);

  // Returns the QueryScheduler TaskRunner, or null if there is none.
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();

  // Looks up the QueryScheduler and passes it to `callback`. This must run on
  // the TaskRunner returned by GetTaskRunner(). If there is no QueryScheduler
  // (which can happen if the scheduler is deleted after the CallWithScheduler
  // task is posted), `callback` is dropped.
  void CallWithScheduler(base::OnceCallback<void(QueryScheduler*)> callback);

 private:
  friend class base::NoDestructor<SchedulerTaskRunner>;

  SchedulerTaskRunner() = default;
  ~SchedulerTaskRunner() = default;

  base::Lock task_runner_lock_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_
      GUARDED_BY(task_runner_lock_);

  raw_ptr<Graph> graph_ = nullptr;
};

// static
SchedulerTaskRunner* SchedulerTaskRunner::GetInstance() {
  static base::NoDestructor<SchedulerTaskRunner> instance;
  return instance.get();
}

void SchedulerTaskRunner::OnSchedulerPassedToGraph(Graph* graph) {
  base::AutoLock lock(task_runner_lock_);
  CHECK(!task_runner_);
  // Use the PM task runner if QueryScheduler is installed on the PM. (In tests
  // it might not be.) This is used instead of GetCurrentDefault() because the
  // PM task runner might be a wrapper for the default.
  if (PerformanceManager::GetTaskRunner()->RunsTasksInCurrentSequence()) {
    task_runner_ = PerformanceManager::GetTaskRunner();
  } else {
    task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  }
  CHECK(task_runner_);
  CHECK(!graph_);
  graph_ = graph;
}

void SchedulerTaskRunner::OnSchedulerTakenFromGraph(Graph* graph) {
  base::AutoLock lock(task_runner_lock_);
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  task_runner_.reset();
  CHECK_EQ(graph_.get(), graph);
  graph_ = nullptr;
}

scoped_refptr<base::SequencedTaskRunner> SchedulerTaskRunner::GetTaskRunner() {
  base::AutoLock lock(task_runner_lock_);
  return task_runner_;
}

void SchedulerTaskRunner::CallWithScheduler(
    base::OnceCallback<void(QueryScheduler*)> callback) {
  if (graph_) {
    std::move(callback).Run(QueryScheduler::GetFromGraph(graph_.get()));
  }
}

}  // namespace

QueryScheduler::QueryScheduler() = default;

QueryScheduler::~QueryScheduler() = default;

base::WeakPtr<QueryScheduler> QueryScheduler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// static
void QueryScheduler::CallWithScheduler(
    base::OnceCallback<void(QueryScheduler*)> callback,
    const base::Location& location) {
  auto* scheduler_and_task_runner = SchedulerTaskRunner::GetInstance();
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      scheduler_and_task_runner->GetTaskRunner();
  if (task_runner) {
    // Unretained is safe because SchedulerTaskRunner is a leaked singleton.
    task_runner->PostTask(
        location, base::BindOnce(&SchedulerTaskRunner::CallWithScheduler,
                                 base::Unretained(scheduler_and_task_runner),
                                 std::move(callback)));
  }
}

void QueryScheduler::AddScopedQuery(QueryParams* query_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(query_params);
  // TODO(crbug.com/40926264): Associate a notifier with the params so that when
  // a scheduled measurement is done, the correct ScopedResourceUsageQuery can
  // be notified. (Currently queries are only notified when they request it by
  // calling RequestResults().)
  if (query_params->resource_types.Has(ResourceType::kCPUTime)) {
    AddCPUQuery();
  }
  if (query_params->resource_types.Has(ResourceType::kMemorySummary)) {
    AddMemoryQuery();
  }
}

void QueryScheduler::RemoveScopedQuery(
    std::unique_ptr<QueryParams> query_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(query_params);
  // TODO(crbug.com/40926264): Forget the notifier associated with the params.
  if (query_params->resource_types.Has(ResourceType::kCPUTime)) {
    const std::optional<QueryId>& query_id =
        query_params->GetId(base::PassKey<QueryScheduler>());
    if (query_id.has_value()) {
      cpu_monitor_.RepeatingQueryStopped(query_id.value());
    }
    RemoveCPUQuery();
  }
  if (query_params->resource_types.Has(ResourceType::kMemorySummary)) {
    RemoveMemoryQuery();
  }
  // `query_params` goes out of scope and is deleted here.
}

void QueryScheduler::StartRepeatingQuery(QueryParams* query_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(query_params);
  // Assign a QueryId to the query. This isn't done in AddScopedQuery() because
  // the QueryId is used to identify queries that need to be notified of
  // results, and a ScopedResourceUsageQuery that never calls Start() doesn't
  // need to be notified.
  static QueryId::Generator id_generator;
  std::optional<QueryId>& query_id =
      query_params->GetMutableId(base::PassKey<QueryScheduler>());
  CHECK(!query_id.has_value());
  query_id = id_generator.GenerateNextId();
  if (query_params->resource_types.Has(ResourceType::kCPUTime)) {
    cpu_monitor_.RepeatingQueryStarted(query_id.value());
  }
}

void QueryScheduler::RequestResults(
    const QueryParams& query_params,
    base::OnceCallback<void(const QueryResultMap&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Send out a measurement request for each resource type. The BarrierCallback
  // will invoke OnResultsReceived when all have responded.
  const size_t num_requests = query_params.resource_types.size();
  auto barrier_callback = base::BarrierCallback<QueryResultMap>(
      num_requests, base::BindOnce(&QueryScheduler::OnResultsReceived,
                                   weak_factory_.GetWeakPtr(),
                                   query_params.contexts, std::move(callback)));

  size_t requests_sent = 0;
  for (ResourceType resource_type : query_params.resource_types) {
    switch (resource_type) {
      case ResourceType::kCPUTime:
        if (cpu_monitor_.IsMonitoring()) {
          // Pass the QueryId of a scoped query or nullopt for a one-shot.
          barrier_callback.Run(cpu_monitor_.UpdateAndGetCPUMeasurements(
              query_params.GetId(base::PassKey<QueryScheduler>())));
        } else {
          // If no scoped query is keeping the CPU monitor running, just return
          // empty results.
          // TODO(crbug.com/40926264): Could run the CPU monitor for a few
          // seconds instead.
          barrier_callback.Run({});
        }
        requests_sent++;
        break;
      case ResourceType::kMemorySummary:
        memory_provider_->RequestMemorySummary(barrier_callback);
        requests_sent++;
        break;
    }
  }
  CHECK_EQ(requests_sent, num_requests);
}

void QueryScheduler::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  memory_provider_.emplace(graph);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(
      base::OptionalToPtr(memory_provider_), "ResourceAttr.Memory");
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(&cpu_monitor_,
                                                           "ResourceAttr.CPU");
  SchedulerTaskRunner::GetInstance()->OnSchedulerPassedToGraph(graph);
}

void QueryScheduler::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SchedulerTaskRunner::GetInstance()->OnSchedulerTakenFromGraph(graph);
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(&cpu_monitor_);
  if (cpu_query_count_ > 0) {
    cpu_monitor_.StopMonitoring();
  }
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(
      base::OptionalToPtr(memory_provider_));
  memory_provider_.reset();
}

CPUMeasurementMonitor& QueryScheduler::GetCPUMonitorForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cpu_monitor_;
}

MemoryMeasurementProvider& QueryScheduler::GetMemoryProviderForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return memory_provider_.value();
}

uint32_t QueryScheduler::GetQueryCountForTesting(
    ResourceType resource_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (resource_type) {
    case ResourceType::kCPUTime:
      return cpu_query_count_;
    case ResourceType::kMemorySummary:
      return memory_query_count_;
  }
  NOTREACHED();
}

void QueryScheduler::RecordMemoryMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cpu_monitor_.RecordMemoryMetrics();
}

void QueryScheduler::AddCPUQuery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cpu_query_count_ += 1;
  // Check for overflow.
  CHECK_GT(cpu_query_count_, 0U);
  if (cpu_query_count_ == 1) {
    CHECK(!cpu_monitor_.IsMonitoring());
    cpu_monitor_.StartMonitoring(GetOwningGraph());
  }
}

void QueryScheduler::RemoveCPUQuery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(cpu_query_count_, 1U);
  cpu_query_count_ -= 1;
  if (cpu_query_count_ == 0) {
    CHECK(cpu_monitor_.IsMonitoring());
    cpu_monitor_.StopMonitoring();
  }
}

void QueryScheduler::AddMemoryQuery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  memory_query_count_ += 1;
  // Check for overflow.
  CHECK_GT(memory_query_count_, 0U);
}

void QueryScheduler::RemoveMemoryQuery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(memory_query_count_, 1U);
  memory_query_count_ -= 1;
}

void QueryScheduler::OnResultsReceived(
    const ContextCollection& contexts,
    base::OnceCallback<void(const QueryResultMap&)> callback,
    std::vector<QueryResultMap> all_results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QueryResultMap merged_results;
  for (auto& result_map : all_results) {
    for (auto& [context, result] : result_map) {
      if (!contexts.ContainsContext(context)) {
        continue;
      }
      QueryResults& merged_result = merged_results[context];
      // Move from `result` into `merged_result`. Only one member of `result`
      // should be set since each element of `all_results` is the result for a
      // single resource type.
      if (result.cpu_time_result.has_value()) {
        std::swap(result.cpu_time_result, merged_result.cpu_time_result);
      } else if (result.memory_summary_result.has_value()) {
        std::swap(result.memory_summary_result,
                  merged_result.memory_summary_result);
      }
      // If this fails, either `result` had multiple members set, or multiple
      // entries of `all_results` copied measurements of the same resource into
      // `merged_result` and the earlier measurement was swapped into `result`.
      CHECK(result == QueryResults{});
    }
  }
  std::move(callback).Run(merged_results);
}

}  // namespace resource_attribution::internal

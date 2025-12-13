// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/query_scheduler.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/check_op.h"
#include "base/containers/enum_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/types/optional_util.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "components/performance_manager/resource_attribution/context_collection.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "components/performance_manager/resource_attribution/query_params.h"

namespace resource_attribution::internal {

namespace {

QueryScheduler* g_query_scheduler = nullptr;

}  // namespace

QueryScheduler::QueryScheduler() = default;

QueryScheduler::~QueryScheduler() = default;

// static
QueryScheduler* QueryScheduler::Get() {
  return g_query_scheduler;
}

base::WeakPtr<QueryScheduler> QueryScheduler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
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
  const std::optional<QueryId>& query_id =
      query_params->GetId(base::PassKey<QueryScheduler>());
  if (query_id.has_value()) {
    // `passive_observer_queries_` will contain this ID iff the query was
    // started with a `passive_observer_callback`.
    passive_observer_queries_.erase(query_id.value());
  }
  if (query_params->resource_types.Has(ResourceType::kCPUTime)) {
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

void QueryScheduler::StartRepeatingQuery(
    QueryParams* query_params,
    base::RepeatingCallback<void(const QueryResultMap&)>
        passive_observer_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(query_params);
  // Assign a QueryId to the query. This isn't done in AddScopedQuery() because
  // the QueryId is used to identify queries that need to be notified of
  // results, and a ScopedResourceUsageQuery that never calls Start() doesn't
  // need to be notified.
  static QueryId::Generator id_generator;
  std::optional<QueryId>& query_id =
      query_params->GetMutableId(base::PassKey<QueryScheduler>());
  // If this fails, Start() was called more than once on the same query.
  CHECK(!query_id.has_value());
  query_id = id_generator.GenerateNextId();
  if (query_params->resource_types.Has(ResourceType::kCPUTime)) {
    cpu_monitor_.RepeatingQueryStarted(query_id.value());
  }
  if (passive_observer_callback) {
    auto [_, inserted] = passive_observer_queries_.emplace(
        query_id.value(), std::make_pair(query_params->Clone(),
                                         std::move(passive_observer_callback)));
    CHECK(inserted);
  }
}

void QueryScheduler::RequestResults(
    const QueryParams& query_params,
    base::OnceCallback<void(const QueryResultMap&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This timer runs from the beginning of RequestResults until the end of
  // OnResultsReceived, going out of scope just before `callback` runs.
  auto histogram_timer = std::make_unique<base::ScopedUmaHistogramTimer>(
      "PerformanceManager.ResourceQueryTime.RequestResults",
      base::ScopedUmaHistogramTimer::ScopedHistogramTiming::kMicrosecondTimes);

  const std::optional<QueryId>& query_id =
      query_params.GetId(base::PassKey<QueryScheduler>());
  // Send out a measurement request for each resource type. The BarrierCallback
  // will invoke OnResultsReceived when all have responded.
  const size_t num_requests = query_params.resource_types.size();
  auto barrier_callback = base::BarrierCallback<QueryResultMap>(
      num_requests,
      base::BindOnce(&QueryScheduler::OnResultsReceived,
                     weak_factory_.GetWeakPtr(), query_id,
                     query_params.contexts, std::move(histogram_timer),
                     std::move(callback)));

  size_t requests_sent = 0;
  for (ResourceType resource_type : query_params.resource_types) {
    switch (resource_type) {
      case ResourceType::kCPUTime:
        if (cpu_monitor_.IsMonitoring()) {
          // Pass the QueryId of a scoped query or nullopt for a one-shot.
          barrier_callback.Run(
              cpu_monitor_.UpdateAndGetCPUMeasurements(query_id));
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
  CHECK(!g_query_scheduler);
  g_query_scheduler = this;
  memory_provider_.emplace(graph);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(
      base::OptionalToPtr(memory_provider_), "ResourceAttr.Memory");
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(&cpu_monitor_,
                                                           "ResourceAttr.CPU");
}

void QueryScheduler::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(&cpu_monitor_);
  if (cpu_query_count_ > 0) {
    cpu_monitor_.StopMonitoring();
  }
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(
      base::OptionalToPtr(memory_provider_));
  memory_provider_.reset();
  CHECK_EQ(g_query_scheduler, this);
  g_query_scheduler = nullptr;
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
    const std::optional<QueryId>& query_id,
    const ContextCollection& contexts,
    std::unique_ptr<base::ScopedUmaHistogramTimer> request_timer,
    base::OnceCallback<void(const QueryResultMap&)> callback,
    std::vector<QueryResultMap> all_results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QueryResultMap merged_results;
  {
    base::ScopedUmaHistogramTimer histogram_timer(
        "PerformanceManager.ResourceQueryTime.OnResultsReceived",
        base::ScopedUmaHistogramTimer::ScopedHistogramTiming::
            kMicrosecondTimes);
    for (auto& result_map : all_results) {
      for (auto& [context, result] : result_map) {
        // Notify any other query that's observing this context and result type.
        for (const auto& [other_query_id, params_and_callback] :
             passive_observer_queries_) {
          const auto& [other_query_params, other_callback] =
              params_and_callback;
          if (query_id.has_value() && query_id.value() == other_query_id) {
            // This query triggered the measurement and is already being
            // notified.
            continue;
          }

          // Check if the other query wants any resource types that were
          // measured.
          const CPUTimeResult* observed_cpu_time = nullptr;
          if (other_query_params->resource_types.Has(ResourceType::kCPUTime)) {
            observed_cpu_time = base::OptionalToPtr(result.cpu_time_result);
          }
          const MemorySummaryResult* observed_memory_summary = nullptr;
          if (other_query_params->resource_types.Has(
                  ResourceType::kMemorySummary)) {
            observed_memory_summary =
                base::OptionalToPtr(result.memory_summary_result);
          }
          if (!observed_cpu_time && !observed_memory_summary) {
            continue;
          }

          // Check if the other query wants this context.
          if (!other_query_params->contexts.ContainsContext(context)) {
            continue;
          }

          // Notify the other query with this partial result. Each query may be
          // notified many times. This makes no attempt to coalesce results
          // because that would add extra complexity.
          QueryResults observed_results;
          if (observed_cpu_time) {
            observed_results.cpu_time_result = *observed_cpu_time;
          }
          if (observed_memory_summary) {
            observed_results.memory_summary_result = *observed_memory_summary;
          }
          QueryResultMap single_result_map;
          single_result_map.emplace(context, std::move(observed_results));
          other_callback.Run(std::move(single_result_map));
        }

        // Merge this context and result type into the results for the
        // triggering query.
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
        // entries of `all_results` copied measurements of the same resource
        // into `merged_result` and the earlier measurement was swapped into
        // `result`.
        CHECK(result == QueryResults{});
      }
    }
  }
  request_timer.reset();

  std::move(callback).Run(merged_results);
}

}  // namespace resource_attribution::internal

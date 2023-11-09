// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/query_scheduler.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/enum_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/task_runner.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/resource_attribution/query_params.h"

namespace performance_manager::resource_attribution {

namespace {

using QueryParams = internal::QueryParams;

QueryScheduler* GetSchedulerFromGraph(Graph* graph) {
  auto* scheduler = QueryScheduler::GetFromGraph(graph);
  CHECK(scheduler);
  return scheduler;
}

}  // namespace

QueryScheduler::QueryScheduler() = default;

QueryScheduler::~QueryScheduler() = default;

base::WeakPtr<QueryScheduler> QueryScheduler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// static
void QueryScheduler::CallOnGraphWithScheduler(
    base::OnceCallback<void(QueryScheduler*)> callback,
    const base::Location& location) {
  PerformanceManager::CallOnGraph(
      location,
      base::BindOnce(&GetSchedulerFromGraph).Then(std::move(callback)));
}

void QueryScheduler::AddCPUQuery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(graph_, nullptr);
  cpu_query_count_ += 1;
  // Check for overflow.
  CHECK_GT(cpu_query_count_, 0U);
  if (cpu_query_count_ == 1) {
    CHECK(!cpu_monitor_.IsMonitoring());
    cpu_monitor_.StartMonitoring(graph_);
  }
}

void QueryScheduler::RemoveCPUQuery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(graph_, nullptr);
  CHECK_GE(cpu_query_count_, 1U);
  cpu_query_count_ -= 1;
  if (cpu_query_count_ == 0) {
    CHECK(cpu_monitor_.IsMonitoring());
    cpu_monitor_.StopMonitoring();
  }
}

void QueryScheduler::AddScopedQuery(QueryParams* query_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(query_params);
  // TODO(crbug.com/1471683): Associate a notifier with the params so that when
  // a scheduled measurement is done, the correct ScopedResourceUsageQuery can
  // be notified.
  if (query_params->resource_types.Has(ResourceType::kCPUTime)) {
    AddCPUQuery();
  }
}

void QueryScheduler::RemoveScopedQuery(
    std::unique_ptr<QueryParams> query_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(query_params);
  // TODO(crbug.com/1471683): Forget the notifier associated with the params.
  if (query_params->resource_types.Has(ResourceType::kCPUTime)) {
    RemoveCPUQuery();
  }
  // `query_params` goes out of scope and is deleted here.
}

void QueryScheduler::RequestCPUResults(
    base::OnceCallback<void(const QueryResultMap&)> callback,
    scoped_refptr<base::TaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(cpu_monitor_.IsMonitoring());
  QueryResultMap results;
  for (auto& [context, cpu_time_result] :
       cpu_monitor_.UpdateAndGetCPUMeasurements()) {
    // TODO(crbug.com/1471683): Filter the results by ResourceContexts in the
    // request.
    results[context].push_back(std::move(cpu_time_result));
  }
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(results)));
}

CPUMeasurementMonitor& QueryScheduler::GetCPUMonitorForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cpu_monitor_;
}

void QueryScheduler::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(graph_, nullptr);
  graph_ = graph;
  graph_->RegisterObject(this);
}

void QueryScheduler::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(graph_, graph);
  graph_->UnregisterObject(this);
  graph_ = nullptr;
  if (cpu_query_count_ > 0) {
    cpu_monitor_.StopMonitoring();
  }
}

}  // namespace performance_manager::resource_attribution

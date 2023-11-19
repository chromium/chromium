// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/query_scheduler.h"

#include <bitset>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/enum_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "components/performance_manager/resource_attribution/query_params.h"

namespace performance_manager::resource_attribution {

namespace {

using QueryParams = internal::QueryParams;

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
  task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  CHECK(!graph_);
  graph_ = graph;
}

void SchedulerTaskRunner::OnSchedulerTakenFromGraph(Graph* graph) {
  base::AutoLock lock(task_runner_lock_);
  CHECK_EQ(task_runner_, base::SequencedTaskRunner::GetCurrentDefault());
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

void QueryScheduler::RequestResults(
    const QueryParams& query_params,
    base::OnceCallback<void(const QueryResultMap&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QueryResultMap results;
  // If no scoped query is keeping the CPU monitor running, just return empty
  // results.
  // TODO(crbug.com/1471683): Could run the CPU monitor for a few seconds
  // instead.
  if (query_params.resource_types.Has(ResourceType::kCPUTime) &&
      cpu_monitor_.IsMonitoring()) {
    // TODO(crbug.com/1471683): Pass the contexts of interest into
    // `cpu_monitor_` so it doesn't have to do work measuring contexts that will
    // be filtered out.
    for (auto& [context, cpu_time_result] :
         cpu_monitor_.UpdateAndGetCPUMeasurements()) {
      // index() gets context's type index in the ResourceContext variant.
      if (query_params.all_context_types.test(context.index()) ||
          base::Contains(query_params.resource_contexts, context)) {
        results[context].push_back(std::move(cpu_time_result));
      }
    }
  }
  std::move(callback).Run(results);
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
  SchedulerTaskRunner::GetInstance()->OnSchedulerPassedToGraph(graph);
}

void QueryScheduler::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(graph_, graph);
  graph_->UnregisterObject(this);
  graph_ = nullptr;
  SchedulerTaskRunner::GetInstance()->OnSchedulerTakenFromGraph(graph);
  if (cpu_query_count_ > 0) {
    cpu_monitor_.StopMonitoring();
  }
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

}  // namespace performance_manager::resource_attribution

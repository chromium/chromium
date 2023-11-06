// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/query_scheduler.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/task_runner.h"

namespace performance_manager::resource_attribution {

QueryScheduler::QueryScheduler() = default;

QueryScheduler::~QueryScheduler() = default;

base::WeakPtr<QueryScheduler> QueryScheduler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
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

void QueryScheduler::RequestCPUResults(
    base::OnceCallback<void(const QueryResultMap&)> callback,
    scoped_refptr<base::TaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(cpu_monitor_.IsMonitoring());
  // TODO(crbug.com/1471683): Keep track of which ResourceContexts are being
  // queried, and return only those from `cpu_monitor_`.
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                cpu_monitor_.UpdateAndGetCPUMeasurements()));
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

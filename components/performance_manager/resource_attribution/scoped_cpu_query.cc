// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/scoped_cpu_query.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "components/performance_manager/resource_attribution/query_scheduler.h"

namespace performance_manager::resource_attribution {

ScopedCPUQuery::ScopedCPUQuery(Graph* graph) {
  auto* scheduler = QueryScheduler::GetFromGraph(graph);
  CHECK(scheduler);
  scheduler->AddCPUQuery();
  scheduler_ = scheduler->GetWeakPtr();
}

ScopedCPUQuery::~ScopedCPUQuery() {
  if (scheduler_) {
    scheduler_->RemoveCPUQuery();
  }
}

void ScopedCPUQuery::QueryOnce(
    base::OnceCallback<void(const QueryResultMap&)> callback,
    scoped_refptr<base::TaskRunner> task_runner) {
  if (scheduler_) {
    scheduler_->RequestCPUResults(std::move(callback), task_runner);
  }
  // Drop the callback if the scheduler is unavailable, since this means
  // PerformanceManager is being torn down so queries sent to the PM sequence
  // would be dropped.
}

}  // namespace performance_manager::resource_attribution

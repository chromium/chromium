// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/scoped_cpu_query.h"

#include "base/check.h"
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

QueryResultMap ScopedCPUQuery::QueryOnce() {
  if (scheduler_) {
    return scheduler_->RequestCPUResults();
  } else {
    return QueryResultMap();
  }
}

}  // namespace performance_manager::resource_attribution

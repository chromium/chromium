// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/scoped_cpu_query.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"

namespace performance_manager::resource_attribution {

ScopedCPUQuery::ScopedCPUQuery()
    : wrapped_query_(QueryBuilder()
                         .AddResourceType(ResourceType::kCPUTime)
                         .AddAllContextsOfType<FrameContext>()
                         .AddAllContextsOfType<PageContext>()
                         .AddAllContextsOfType<ProcessContext>()
                         .AddAllContextsOfType<WorkerContext>()
                         .CreateScopedQuery()) {
  wrapped_query_.AddObserver(this);
}

ScopedCPUQuery::~ScopedCPUQuery() {
  wrapped_query_.RemoveObserver(this);
}

void ScopedCPUQuery::QueryOnce(ScopedCPUQuery::ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callbacks_.empty()) {
    // No queries in flight, so start a new query. Further QueryOnce() calls
    // before OnResourceUsageUpdate() clears the callback list will use the same
    // query.
    wrapped_query_.QueryOnce();
  }
  callbacks_.push_back(std::move(callback));
}

void ScopedCPUQuery::OnResourceUsageUpdated(const QueryResultMap& results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!callbacks_.empty());
  for (ResultCallback& callback : callbacks_) {
    std::move(callback).Run(results);
  }
  callbacks_.clear();
}

}  // namespace performance_manager::resource_attribution

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_PARAMS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_PARAMS_H_

#include <memory>
#include <optional>

#include "base/types/id_type.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "components/performance_manager/resource_attribution/context_collection.h"

namespace resource_attribution::internal {

class QueryScheduler;

// Id to identify the ScopedResourceUsageQuery that QueryParams came from.
// One-shot queries (created with QueryBuilder::QueryOnce()) won't have an id.
using QueryId = base::IdTypeU32<class QueryIdTag>;

class QueryParams {
 public:
  QueryParams();
  ~QueryParams();

  // Move-only.
  QueryParams(const QueryParams&) = delete;
  QueryParams& operator=(const QueryParams&) = delete;
  QueryParams(QueryParams&& other);
  QueryParams& operator=(QueryParams&& other);

  // Resource types to measure.
  ResourceTypeSet resource_types;

  // Contexts to measure.
  ContextCollection contexts;

  // Only QueryScheduler can access the QueryId outside of tests, to ensure it's
  // only used on the QueryScheduler sequence.
  const std::optional<QueryId>& GetId(base::PassKey<QueryScheduler>) const {
    return id_;
  }
  std::optional<QueryId>& GetMutableId(base::PassKey<QueryScheduler>) {
    return id_;
  }

  // Allow tests to validate the QueryId, but not update it.
  std::optional<QueryId> GetIdForTesting() const { return id_; }

  // Returns a copy of these params without a QueryId, to create an identical
  // query.
  std::unique_ptr<QueryParams> Clone() const;

 private:
  std::optional<QueryId> id_;
};

}  // namespace resource_attribution::internal

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_PARAMS_H_

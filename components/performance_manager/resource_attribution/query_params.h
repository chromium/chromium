// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_PARAMS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_PARAMS_H_

#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "components/performance_manager/resource_attribution/context_collection.h"

namespace resource_attribution::internal {

struct QueryParams {
  QueryParams();
  ~QueryParams();

  QueryParams(const QueryParams& other);
  QueryParams& operator=(const QueryParams& other);

  friend bool operator==(const QueryParams&, const QueryParams&) = default;

  // Resource types to measure.
  ResourceTypeSet resource_types;

  // Contexts to measure.
  ContextCollection contexts;
};

}  // namespace resource_attribution::internal

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_PARAMS_H_

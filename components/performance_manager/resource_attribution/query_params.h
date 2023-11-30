// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_PARAMS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_PARAMS_H_

#include <bitset>
#include <compare>
#include <set>

#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace performance_manager::resource_attribution::internal {

struct QueryParams {
  // Context types that can be added with AddAllContextsOfType, based on their
  // index in the ResourceContexts variant.
  using ContextTypeSet =
      std::bitset<absl::variant_size<ResourceContext>::value>;

  QueryParams();
  ~QueryParams();

  QueryParams(const QueryParams& other);
  QueryParams& operator=(const QueryParams& other);

  friend constexpr bool operator==(const QueryParams&,
                                   const QueryParams&) = default;

  // Individual resource contexts to measure.
  std::set<ResourceContext> resource_contexts;

  // For each of these context types, all contexts that exist will be measured.
  ContextTypeSet all_context_types;

  // Resource types to measure.
  ResourceTypeSet resource_types;
};

}  // namespace performance_manager::resource_attribution::internal

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_QUERY_PARAMS_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/query_params.h"

namespace resource_attribution::internal {

QueryParams::QueryParams() = default;

QueryParams::~QueryParams() = default;

QueryParams::QueryParams(QueryParams&& other) = default;

QueryParams& QueryParams::operator=(QueryParams&& other) = default;

std::unique_ptr<QueryParams> QueryParams::Clone() const {
  auto cloned_params = std::make_unique<QueryParams>();
  cloned_params->resource_types = resource_types;
  cloned_params->contexts = contexts;
  return cloned_params;
}

}  // namespace resource_attribution::internal

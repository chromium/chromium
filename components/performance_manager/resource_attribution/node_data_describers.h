// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_NODE_DATA_DESCRIBERS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_NODE_DATA_DESCRIBERS_H_

#include "base/values.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"

namespace resource_attribution {

// Returns a description of `metadata` for chrome://discards/graph. The result
// can be added to the description of a node with base::Value::Dict::Merge().
base::Value::Dict DescribeResultMetadata(const ResultMetadata& metadata);

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_NODE_DATA_DESCRIBERS_H_

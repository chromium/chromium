// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_WORKER_CLIENT_PAGES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_WORKER_CLIENT_PAGES_H_

#include <set>

#include "components/performance_manager/resource_attribution/graph_change.h"

namespace performance_manager {
class PageNode;
class WorkerNode;
}  // namespace performance_manager

namespace performance_manager::resource_attribution {

// Returns the complete set of pages that are clients of `worker_node`.
// `graph_change` is a change to the graph topology in progress that may affect
// the client page set, or NoGraphChange.
std::set<const PageNode*> GetWorkerClientPages(
    const WorkerNode* worker_node,
    GraphChange graph_change = NoGraphChange{});

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_WORKER_CLIENT_PAGES_H_

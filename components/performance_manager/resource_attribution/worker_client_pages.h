// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_WORKER_CLIENT_PAGES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_WORKER_CLIENT_PAGES_H_

#include <set>
#include <utility>

#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "content/public/browser/browsing_instance_id.h"

namespace resource_attribution {

// Returns the set of pages and browsing instances that are clients of
// `worker_node`.
std::pair<std::set<const PageNode*>, std::set<content::BrowsingInstanceId>>
GetWorkerClientPagesAndBrowsingInstances(const WorkerNode* worker_node);

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_WORKER_CLIENT_PAGES_H_

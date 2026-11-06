// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_WORKER_CLIENT_PAGES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_WORKER_CLIENT_PAGES_H_

#include <functional>
#include <utility>

#include "base/containers/flat_set.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "content/public/browser/browsing_instance_id.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace resource_attribution {

using ClientPageSet = base::flat_set<const PageNode*,
                                     std::less<>,
                                     absl::InlinedVector<const PageNode*, 8>>;
using ClientBrowsingInstanceSet =
    base::flat_set<content::BrowsingInstanceId,
                   std::less<>,
                   absl::InlinedVector<content::BrowsingInstanceId, 8>>;

// Returns the set of pages and browsing instances that are clients of
// `worker_node`.
std::pair<ClientPageSet, ClientBrowsingInstanceSet>
GetWorkerClientPagesAndBrowsingInstances(const WorkerNode* worker_node);

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_WORKER_CLIENT_PAGES_H_

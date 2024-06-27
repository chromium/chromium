// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/worker_client_pages.h"

#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/worker_node.h"

namespace resource_attribution {

std::pair<std::set<const PageNode*>, std::set<content::BrowsingInstanceId>>
GetWorkerClientPagesAndBrowsingInstances(const WorkerNode* worker_node) {
  std::set<const PageNode*> client_pages;
  std::set<content::BrowsingInstanceId> client_browsing_instances;
  performance_manager::GraphOperations::VisitAllWorkerClients(
      worker_node,
      [&](const FrameNode* f) {
        client_pages.insert(f->GetPageNode());
        client_browsing_instances.insert(f->GetBrowsingInstanceId());
        return true;
      },
      [&](const WorkerNode* w) { return true; });
  return {std::move(client_pages), std::move(client_browsing_instances)};
}

}  // namespace resource_attribution

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/worker_client_pages.h"

#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"

namespace resource_attribution {

namespace {

// Recursively visits all client workers of `worker_node`, and all client frames
// of each worker, and adds each frame's PageNode and browsing instance to
// `client_pages` and `client_browsing_instances`. `visited_workers` is used to
// check for loops in the graph of client workers.
void RecursivelyFindClientPagesAndBrowsingInstances(
    const WorkerNode* worker_node,
    std::set<const PageNode*>& client_pages,
    std::set<content::BrowsingInstanceId>& client_browsing_instances,
    std::set<const WorkerNode*>& visited_workers) {
  const auto [_, inserted] = visited_workers.insert(worker_node);
  if (!inserted) {
    // Already visited, halt recursion.
    return;
  }
  worker_node->VisitClientFrames([&](const FrameNode* f) {
    client_pages.insert(f->GetPageNode());
    client_browsing_instances.insert(f->GetBrowsingInstanceId());
    return true;
  });
  worker_node->VisitClientWorkers([&](const WorkerNode* w) {
    RecursivelyFindClientPagesAndBrowsingInstances(
        w, client_pages, client_browsing_instances, visited_workers);
    return true;
  });
}

}  // namespace

std::pair<std::set<const PageNode*>, std::set<content::BrowsingInstanceId>>
GetWorkerClientPagesAndBrowsingInstances(const WorkerNode* worker_node) {
  std::set<const PageNode*> client_pages;
  std::set<content::BrowsingInstanceId> client_browsing_instances;
  std::set<const WorkerNode*> visited_workers;
  RecursivelyFindClientPagesAndBrowsingInstances(
      worker_node, client_pages, client_browsing_instances, visited_workers);
  return {std::move(client_pages), std::move(client_browsing_instances)};
}
}  // namespace resource_attribution

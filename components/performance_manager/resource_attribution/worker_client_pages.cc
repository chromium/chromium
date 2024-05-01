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

// Recursively visits all client workers of `worker_node`, and all client
// frames of each worker, and adds each frame's PageNode to `client_pages`.
// `visited_workers` is used to check for loops in the graph of client
// workers.
void RecursivelyFindClientPages(const WorkerNode* worker_node,
                                std::set<const PageNode*>& client_pages,
                                std::set<const WorkerNode*>& visited_workers) {
  const auto [_, inserted] = visited_workers.insert(worker_node);
  if (!inserted) {
    // Already visited, halt recursion.
    return;
  }
  worker_node->VisitClientFrames([&client_pages](const FrameNode* f) {
    client_pages.insert(f->GetPageNode());
    return true;
  });
  worker_node->VisitClientWorkers(
      [&client_pages, &visited_workers](const WorkerNode* w) {
        RecursivelyFindClientPages(w, client_pages, visited_workers);
        return true;
      });
}

}  // namespace

std::set<const PageNode*> GetWorkerClientPages(const WorkerNode* worker_node) {
  std::set<const PageNode*> client_pages;
  std::set<const WorkerNode*> visited_workers;
  RecursivelyFindClientPages(worker_node, client_pages, visited_workers);
  return client_pages;
}

}  // namespace resource_attribution

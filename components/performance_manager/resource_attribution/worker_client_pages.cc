// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/worker_client_pages.h"

#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace performance_manager::resource_attribution {

namespace {

// Recursively visits all client workers of `worker_node`, and all client
// frames of each worker, and adds each frame's PageNode to `client_pages`.
// `visited_workers` is used to check for loops in the graph of client
// workers. `graph_change` is a change to the graph topology in progress that
// may affect the client page set, or NoGraphChange.
void RecursivelyFindClientPages(const WorkerNode* worker_node,
                                GraphChange graph_change,
                                std::set<const PageNode*>& client_pages,
                                std::set<const WorkerNode*>& visited_workers) {
  const auto [_, inserted] = visited_workers.insert(worker_node);
  if (!inserted) {
    // Already visited, halt recursion.
    return;
  }
  worker_node->VisitClientFrames(
      [&client_pages, &graph_change, worker_node](const FrameNode* f) {
        const auto* add_client_frame_change =
            absl::get_if<GraphChangeAddClientFrameToWorker>(&graph_change);
        if (add_client_frame_change &&
            add_client_frame_change->worker_node == worker_node &&
            add_client_frame_change->client_frame_node == f) {
          // Skip this client node. The measurement being distributed includes
          // results from before it was added.
          return true;
        }
        client_pages.insert(f->GetPageNode());
        return true;
      });
  worker_node->VisitClientWorkers([&client_pages, &visited_workers,
                                   &graph_change,
                                   worker_node](const WorkerNode* w) {
    const auto* add_client_worker_change =
        absl::get_if<GraphChangeAddClientWorkerToWorker>(&graph_change);
    if (add_client_worker_change &&
        add_client_worker_change->worker_node == worker_node &&
        add_client_worker_change->client_worker_node == w) {
      // Skip this client node. The measurement being distributed includes
      // results from before it was added.
      return true;
    }

    RecursivelyFindClientPages(w, graph_change, client_pages, visited_workers);
    return true;
  });
  // Unlike FrameNode, WorkerNode does not update any graph links in
  // WorkerNodeImpl::OnBeforeLeavingGraph(). So no need to check for
  // GraphChangeRemoveClient*FromWorker.
  // TODO(https://crbug.com/1481676): If that changes. handle
  // `graph_change.client_*_node` as if it was visited by the above visitors.
}

}  // namespace

std::set<const PageNode*> GetWorkerClientPages(const WorkerNode* worker_node,
                                               GraphChange graph_change) {
  std::set<const PageNode*> client_pages;
  std::set<const WorkerNode*> visited_workers;
  RecursivelyFindClientPages(worker_node, graph_change, client_pages,
                             visited_workers);
  return client_pages;
}

}  // namespace performance_manager::resource_attribution

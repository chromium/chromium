// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/shared_worker_watcher.h"

#include <utility>
#include <vector>

#include "components/performance_manager/frame_node_source.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/process_node_source.h"
#include "content/public/browser/shared_worker_instance.h"

namespace performance_manager {

namespace {

// Helper function to add |worker_node| as a child to |frame_node| on the PM
// sequence.
void AddWorkerToFrameNode(FrameNodeImpl* frame_node,
                          WorkerNodeImpl* worker_node,
                          GraphImpl* graph) {
  worker_node->AddClientFrame(frame_node);
}

// Helper function to remove |worker_node| from |frame_node| on the PM sequence.
void RemoveWorkerFromFrameNode(FrameNodeImpl* frame_node,
                               WorkerNodeImpl* worker_node,
                               GraphImpl* graph) {
  worker_node->RemoveClientFrame(frame_node);
}

// Helper function to remove all |worker_nodes| from |frame_node| on the PM
// sequence.
void RemoveWorkersFromFrameNode(
    FrameNodeImpl* frame_node,
    const base::flat_set<WorkerNodeImpl*>& worker_nodes,
    GraphImpl* graph) {
  for (auto* worker_node : worker_nodes)
    worker_node->RemoveClientFrame(frame_node);
}

}  // namespace

SharedWorkerWatcher::SharedWorkerWatcher(
    const std::string& browser_context_id,
    content::SharedWorkerService* shared_worker_service,
    ProcessNodeSource* process_node_source,
    FrameNodeSource* frame_node_source)
    : browser_context_id_(browser_context_id),
      shared_worker_service_observer_(this),
      process_node_source_(process_node_source),
      frame_node_source_(frame_node_source) {
  DCHECK(shared_worker_service);
  DCHECK(process_node_source_);
  DCHECK(frame_node_source_);
  shared_worker_service_observer_.Add(shared_worker_service);
}

SharedWorkerWatcher::~SharedWorkerWatcher() {
  DCHECK(frame_node_child_workers_.empty());
  DCHECK(worker_nodes_.empty());
  DCHECK(!shared_worker_service_observer_.IsObservingSources());
}

void SharedWorkerWatcher::TearDown() {
  // First clear client-child relations between frames and workers.
  for (auto& kv : frame_node_child_workers_) {
    const FrameInfo& frame_info = kv.first;
    base::flat_set<WorkerNodeImpl*>& child_workers = kv.second;

    frame_node_source_->UnsubscribeFromFrameNode(frame_info.render_process_id,
                                                 frame_info.frame_id);

    // Disconnect all child workers from |frame_node|.
    FrameNodeImpl* frame_node = frame_node_source_->GetFrameNode(
        frame_info.render_process_id, frame_info.frame_id);
    DCHECK(frame_node);
    DCHECK(!child_workers.empty());
    PerformanceManagerImpl::CallOnGraphImpl(
        FROM_HERE,
        base::BindOnce(&RemoveWorkersFromFrameNode, frame_node, child_workers));
  }
  frame_node_child_workers_.clear();

  // Then clean all the worker nodes.
  std::vector<std::unique_ptr<NodeBase>> nodes;
  nodes.reserve(worker_nodes_.size());
  for (auto& node : worker_nodes_)
    nodes.push_back(std::move(node.second));
  worker_nodes_.clear();

  PerformanceManagerImpl::GetInstance()->BatchDeleteNodes(std::move(nodes));

  shared_worker_service_observer_.RemoveAll();
}

void SharedWorkerWatcher::OnWorkerStarted(
    const content::SharedWorkerInstance& instance,
    int worker_process_id,
    const base::UnguessableToken& dev_tools_token) {
  auto worker_node = PerformanceManagerImpl::GetInstance()->CreateWorkerNode(
      browser_context_id_, WorkerNode::WorkerType::kShared,
      process_node_source_->GetProcessNode(worker_process_id), instance.url(),
      dev_tools_token);
  bool inserted =
      worker_nodes_.emplace(instance, std::move(worker_node)).second;
  DCHECK(inserted);
}

void SharedWorkerWatcher::OnBeforeWorkerTerminated(
    const content::SharedWorkerInstance& instance) {
  auto it = worker_nodes_.find(instance);
  DCHECK(it != worker_nodes_.end());

  auto worker_node = std::move(it->second);
#if DCHECK_IS_ON()
  DCHECK(!base::Contains(clients_to_remove_, worker_node.get()));
#endif  // DCHECK_IS_ON()
  PerformanceManagerImpl::GetInstance()->DeleteNode(std::move(worker_node));

  worker_nodes_.erase(it);
}

void SharedWorkerWatcher::OnClientAdded(
    const content::SharedWorkerInstance& instance,
    int client_process_id,
    int frame_id) {
  FrameNodeImpl* frame_node =
      frame_node_source_->GetFrameNode(client_process_id, frame_id);
  DCHECK(frame_node);

  // Connect the nodes in the PM graph.
  WorkerNodeImpl* worker_node = GetWorkerNode(instance);
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&AddWorkerToFrameNode, frame_node, worker_node));

  // Keep track of the shared workers that this frame is a client to.
  if (AddChildWorker(client_process_id, frame_id, worker_node)) {
    frame_node_source_->SubscribeToFrameNode(
        client_process_id, frame_id,
        base::BindOnce(&SharedWorkerWatcher::OnBeforeFrameNodeRemoved,
                       base::Unretained(this), client_process_id, frame_id));
  }
}

void SharedWorkerWatcher::OnClientRemoved(
    const content::SharedWorkerInstance& instance,
    int client_process_id,
    int frame_id) {
  WorkerNodeImpl* worker_node = GetWorkerNode(instance);

  FrameNodeImpl* frame_node =
      frame_node_source_->GetFrameNode(client_process_id, frame_id);

  // It's possible that the frame was destroyed before receiving the
  // OnClientRemoved() for all of its child shared worker. Nothing to do in
  // that case because OnBeforeFrameNodeRemoved() took care of removing this
  // client from its child worker nodes.
  if (!frame_node) {
#if DCHECK_IS_ON()
    // These debug only checks ensure that this code path is only taken if
    // OnBeforeFrameNodeRemoved() was already called for that frame.
    auto it = clients_to_remove_.find(worker_node);
    DCHECK(it != clients_to_remove_.end());

    int& count = it->second;
    DCHECK_GT(count, 0);
    --count;

    if (count == 0)
      clients_to_remove_.erase(it);
#endif  // DCHECK_IS_ON()
    return;
  }

  // Disconnect the node.
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&RemoveWorkerFromFrameNode, frame_node, worker_node));

  // Remove |worker_node| from the set of workers that this frame is a client
  // of.
  if (RemoveChildWorker(client_process_id, frame_id, worker_node))
    frame_node_source_->UnsubscribeFromFrameNode(client_process_id, frame_id);
}

void SharedWorkerWatcher::OnBeforeFrameNodeRemoved(int render_process_id,
                                                   int frame_id,
                                                   FrameNodeImpl* frame_node) {
  auto it =
      frame_node_child_workers_.find(FrameInfo{render_process_id, frame_id});
  DCHECK(it != frame_node_child_workers_.end());

  // Clean up all child workers of this frame node.
  base::flat_set<WorkerNodeImpl*> child_workers = std::move(it->second);
  frame_node_child_workers_.erase(it);

  // Disconnect all child workers from |frame_node|.
  DCHECK(!child_workers.empty());
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&RemoveWorkersFromFrameNode, frame_node, child_workers));

#if DCHECK_IS_ON()
  for (WorkerNodeImpl* worker_node : child_workers) {
    // Now expect that this frame will be removed as a client for each worker
    // in |child_workers|.
    // Note: the [] operator is intentionally used to default initialize the
    // count to zero if needed.
    clients_to_remove_[worker_node]++;
  }
#endif  // DCHECK_IS_ON()
}

bool SharedWorkerWatcher::AddChildWorker(int render_process_id,
                                         int frame_id,
                                         WorkerNodeImpl* child_worker_node) {
  auto insertion_result =
      frame_node_child_workers_.insert({{render_process_id, frame_id}, {}});

  auto& child_workers = insertion_result.first->second;
  bool inserted = child_workers.insert(child_worker_node).second;
  DCHECK(inserted);

  return insertion_result.second;
}

bool SharedWorkerWatcher::RemoveChildWorker(int render_process_id,
                                            int frame_id,
                                            WorkerNodeImpl* child_worker_node) {
  auto it =
      frame_node_child_workers_.find(FrameInfo{render_process_id, frame_id});
  DCHECK(it != frame_node_child_workers_.end());
  auto& child_workers = it->second;

  size_t removed = child_workers.erase(child_worker_node);
  DCHECK_EQ(removed, 1u);

  if (child_workers.empty()) {
    frame_node_child_workers_.erase(it);
    return true;
  }
  return false;
}

WorkerNodeImpl* SharedWorkerWatcher::GetWorkerNode(
    const content::SharedWorkerInstance& instance) {
  auto it = worker_nodes_.find(instance);
  if (it == worker_nodes_.end()) {
    NOTREACHED();
    return nullptr;
  }
  return it->second.get();
}

bool operator<(const SharedWorkerWatcher::FrameInfo& lhs,
               const SharedWorkerWatcher::FrameInfo& rhs) {
  return std::tie(lhs.render_process_id, lhs.frame_id) <
         std::tie(rhs.render_process_id, rhs.frame_id);
}

}  // namespace performance_manager

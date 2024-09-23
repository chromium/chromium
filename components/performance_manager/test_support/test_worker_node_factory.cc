// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/test_worker_node_factory.h"

#include "base/memory/raw_ptr.h"

namespace performance_manager {

namespace {

// Disconnects the |worker_node| from its clients.
void CleanupWorker(WorkerNodeImpl* worker_node) {
  // Create a copy since RemoveClientFrame()/RemoveClientWorker() will modify
  // the container.
  std::vector<FrameNodeImpl*> client_frames =
      worker_node->client_frames().AsVector();
  for (FrameNodeImpl* client_frame_node : client_frames)
    worker_node->RemoveClientFrame(client_frame_node);

  std::vector<WorkerNodeImpl*> client_workers =
      worker_node->client_workers().AsVector();
  for (WorkerNodeImpl* client_worker_node : client_workers)
    worker_node->RemoveClientWorker(client_worker_node);
}

}  // namespace

TestWorkerNodeFactory::TestWorkerNodeFactory(TestGraphImpl* graph)
    : graph_(graph) {}

TestWorkerNodeFactory::~TestWorkerNodeFactory() {
  for (auto& worker_node : worker_nodes_)
    CleanupWorker(worker_node.get());
}

WorkerNodeImpl* TestWorkerNodeFactory::CreateDedicatedWorker(
    ProcessNodeImpl* process_node,
    FrameNodeImpl* client_frame_node) {
  auto insertion_result =
      worker_nodes_.insert(TestNodeWrapper<WorkerNodeImpl>::Create(
          graph_, WorkerNode::WorkerType::kDedicated, process_node));
  DCHECK(insertion_result.second);

  WorkerNodeImpl* worker_node = insertion_result.first->get();

  worker_node->AddClientFrame(client_frame_node);

  return worker_node;
}

WorkerNodeImpl* TestWorkerNodeFactory::CreateDedicatedWorker(
    ProcessNodeImpl* process_node,
    WorkerNodeImpl* client_worker_node) {
  auto insertion_result =
      worker_nodes_.insert(TestNodeWrapper<WorkerNodeImpl>::Create(
          graph_, WorkerNode::WorkerType::kDedicated, process_node));
  DCHECK(insertion_result.second);

  WorkerNodeImpl* worker_node = insertion_result.first->get();

  worker_node->AddClientWorker(client_worker_node);

  return worker_node;
}

WorkerNodeImpl* TestWorkerNodeFactory::CreateSharedWorker(
    ProcessNodeImpl* process_node,
    const std::vector<FrameNodeImpl*>& client_frame_nodes) {
  auto insertion_result =
      worker_nodes_.insert(TestNodeWrapper<WorkerNodeImpl>::Create(
          graph_, WorkerNode::WorkerType::kShared, process_node));
  DCHECK(insertion_result.second);

  WorkerNodeImpl* worker_node = insertion_result.first->get();

  for (FrameNodeImpl* client_frame_node : client_frame_nodes)
    worker_node->AddClientFrame(client_frame_node);

  return worker_node;
}

void TestWorkerNodeFactory::DeleteWorker(WorkerNodeImpl* worker_node) {
  CleanupWorker(worker_node);
  size_t removed = worker_nodes_.erase(worker_node);
  DCHECK_EQ(removed, 1u);
}

}  // namespace performance_manager

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_WORKER_NODE_FACTORY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_WORKER_NODE_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/test_support/graph_test_harness.h"

namespace performance_manager {

struct TestNodeWrapperComparator {
  using is_transparent = int;

  template <typename T>
  bool operator()(const TestNodeWrapper<T>& lhs,
                  const TestNodeWrapper<T>& rhs) const {
    return lhs.get() < rhs.get();
  }

  template <typename T>
  bool operator()(const T* lhs, const TestNodeWrapper<T>& rhs) const {
    return lhs < rhs.get();
  }

  template <typename T>
  bool operator()(const TestNodeWrapper<T>& lhs, const T* rhs) const {
    return lhs.get() < rhs;
  }
};

// Simplifies the creation of worker nodes with clients for tests.
struct TestWorkerNodeFactory {
 public:
  explicit TestWorkerNodeFactory(TestGraphImpl* graph);
  ~TestWorkerNodeFactory();

  // Creates a dedicated worker with a client frame.
  WorkerNodeImpl* CreateDedicatedWorker(ProcessNodeImpl* process_node,
                                        FrameNodeImpl* client_frame_node);

  // Creates a dedicated worker with a client worker.
  WorkerNodeImpl* CreateDedicatedWorker(ProcessNodeImpl* process_node,
                                        WorkerNodeImpl* client_worker_node);

  // Create a shared worker node with the possibility of having any number of
  // clients.
  WorkerNodeImpl* CreateSharedWorker(
      ProcessNodeImpl* process_node,
      const std::vector<FrameNodeImpl*>& client_frame_nodes);

  // Deletes a worker created by this factory and cleans up the relationships
  // with its clients.
  void DeleteWorker(WorkerNodeImpl* worker_node);

 private:
  raw_ptr<TestGraphImpl> graph_;

  base::flat_set<TestNodeWrapper<WorkerNodeImpl>, TestNodeWrapperComparator>
      worker_nodes_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_WORKER_NODE_FACTORY_H_

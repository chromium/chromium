// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/graph_impl_operations.h"

#include <algorithm>
#include <vector>

#include "base/functional/bind.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using ::testing::UnorderedElementsAre;

class GraphImplOperationsTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  // Sets up two parallel frame trees that span multiple processes each, and a
  // set of workers.
  void SetUp() override {
    Super::SetUp();

    process1_ = CreateNode<ProcessNodeImpl>();
    process2_ = CreateNode<ProcessNodeImpl>();
    page1_ = CreateNode<PageNodeImpl>();
    page2_ = CreateNode<PageNodeImpl>();

    mainframe1_ = CreateFrameNodeAutoId(process1_.get(), page1_.get(), nullptr);
    mainframe2_ = CreateFrameNodeAutoId(process2_.get(), page2_.get(), nullptr);
    childframe1a_ =
        CreateFrameNodeAutoId(process2_.get(), page1_.get(), mainframe1_.get());
    childframe1b_ =
        CreateFrameNodeAutoId(process2_.get(), page1_.get(), mainframe1_.get());
    childframe2a_ =
        CreateFrameNodeAutoId(process1_.get(), page2_.get(), mainframe2_.get());
    childframe2b_ =
        CreateFrameNodeAutoId(process1_.get(), page2_.get(), mainframe2_.get());

    dedicated_worker_ = CreateNode<WorkerNodeImpl>(
        WorkerNode::WorkerType::kDedicated, process1_.get());
    shared_worker_ = CreateNode<WorkerNodeImpl>(WorkerNode::WorkerType::kShared,
                                                process1_.get());
    service_worker_ = CreateNode<WorkerNodeImpl>(
        WorkerNode::WorkerType::kService, process1_.get());
    service_worker_->AddClientFrame(mainframe1_.get());
    service_worker_->AddClientWorker(dedicated_worker_.get());
    service_worker_->AddClientWorker(shared_worker_.get());

    // Give the dedicated worker its own client frame and nested dedicated
    // workers.
    dedicated_worker2_ = CreateNode<WorkerNodeImpl>(
        WorkerNode::WorkerType::kDedicated, process1_.get());
    dedicated_worker3_ = CreateNode<WorkerNodeImpl>(
        WorkerNode::WorkerType::kDedicated, process1_.get());
    service_worker_->AddClientWorker(dedicated_worker3_.get());
    dedicated_worker3_->AddClientWorker(dedicated_worker2_.get());
    dedicated_worker2_->AddClientWorker(dedicated_worker_.get());
    dedicated_worker_->AddClientFrame(mainframe2_.get());
  }

  // Removes worker clients to clean up.
  void TearDown() override {
    dedicated_worker_->RemoveClientFrame(mainframe2_.get());
    dedicated_worker2_->RemoveClientWorker(dedicated_worker_.get());
    dedicated_worker3_->RemoveClientWorker(dedicated_worker2_.get());
    service_worker_->RemoveClientWorker(dedicated_worker3_.get());
    service_worker_->RemoveClientWorker(shared_worker_.get());
    service_worker_->RemoveClientWorker(dedicated_worker_.get());
    service_worker_->RemoveClientFrame(mainframe1_.get());
    Super::TearDown();
  }

  TestNodeWrapper<ProcessNodeImpl> process1_;
  TestNodeWrapper<ProcessNodeImpl> process2_;
  TestNodeWrapper<PageNodeImpl> page1_;
  TestNodeWrapper<PageNodeImpl> page2_;

  // Root nodes. |mainframeX_| is in |processX_|.
  TestNodeWrapper<FrameNodeImpl> mainframe1_;
  TestNodeWrapper<FrameNodeImpl> mainframe2_;

  // Children of |mainframe1_|, but in |process2_|.
  TestNodeWrapper<FrameNodeImpl> childframe1a_;
  TestNodeWrapper<FrameNodeImpl> childframe1b_;

  // Children of |mainframe2_|, but in |process1_|.
  TestNodeWrapper<FrameNodeImpl> childframe2a_;
  TestNodeWrapper<FrameNodeImpl> childframe2b_;

  // Workers of all types.
  TestNodeWrapper<WorkerNodeImpl> dedicated_worker_;
  TestNodeWrapper<WorkerNodeImpl> shared_worker_;
  TestNodeWrapper<WorkerNodeImpl> service_worker_;

  // More dedicated workers to test nested clients.
  TestNodeWrapper<WorkerNodeImpl> dedicated_worker2_;
  TestNodeWrapper<WorkerNodeImpl> dedicated_worker3_;
};

}  // namespace

TEST_F(GraphImplOperationsTest, GetAssociatedPageNodes) {
  auto page_nodes =
      GraphImplOperations::GetAssociatedPageNodes(process1_.get());
  EXPECT_EQ(2u, page_nodes.size());
  EXPECT_THAT(page_nodes, UnorderedElementsAre(page1_.get(), page2_.get()));
}

TEST_F(GraphImplOperationsTest, GetAssociatedProcessNodes) {
  auto process_nodes =
      GraphImplOperations::GetAssociatedProcessNodes(page1_.get());
  EXPECT_EQ(2u, process_nodes.size());
  EXPECT_THAT(process_nodes,
              UnorderedElementsAre(process1_.get(), process2_.get()));
}

TEST_F(GraphImplOperationsTest, GetFrameNodes) {
  // Add a grandchild frame.
  auto grandchild =
      CreateFrameNodeAutoId(process1_.get(), page1_.get(), childframe1a_.get());

  auto frame_nodes = GraphImplOperations::GetFrameNodes(page1_.get());
  EXPECT_THAT(frame_nodes,
              UnorderedElementsAre(mainframe1_.get(), childframe1a_.get(),
                                   childframe1b_.get(), grandchild.get()));
  // In a level order the main-frame is first, and the grandchild is last. The
  // two children can come in any order.
  EXPECT_EQ(mainframe1_.get(), frame_nodes[0]);
  EXPECT_EQ(grandchild.get(), frame_nodes[3]);
}

TEST_F(GraphImplOperationsTest, VisitFrameTree) {
  auto frame_nodes = GraphImplOperations::GetFrameNodes(page1_.get());

  std::vector<FrameNodeImpl*> visited;
  EXPECT_TRUE(GraphImplOperations::VisitFrameTreePreOrder(
      page1_.get(), [&visited](FrameNodeImpl* frame_node) -> bool {
        visited.push_back(frame_node);
        return true;
      }));
  EXPECT_THAT(visited,
              UnorderedElementsAre(mainframe1_.get(), childframe1a_.get(),
                                   childframe1b_.get()));
  // In pre-order the main frame is first.
  EXPECT_EQ(mainframe1_.get(), visited[0]);

  // Do an aborted visit pre-order visit.
  visited.clear();
  EXPECT_FALSE(GraphImplOperations::VisitFrameTreePreOrder(
      page1_.get(), [&visited](FrameNodeImpl* frame_node) -> bool {
        visited.push_back(frame_node);
        return false;
      }));
  EXPECT_EQ(1u, visited.size());

  visited.clear();
  EXPECT_TRUE(GraphImplOperations::VisitFrameTreePostOrder(
      page1_.get(), [&visited](FrameNodeImpl* frame_node) -> bool {
        visited.push_back(frame_node);
        return true;
      }));
  EXPECT_THAT(visited,
              UnorderedElementsAre(mainframe1_.get(), childframe1a_.get(),
                                   childframe1b_.get()));
  // In post-order the main frame is last.
  EXPECT_EQ(mainframe1_.get(), visited[2]);

  // Do an aborted post-order visit.
  visited.clear();
  EXPECT_FALSE(GraphImplOperations::VisitFrameTreePostOrder(
      page1_.get(), [&visited](FrameNodeImpl* frame_node) -> bool {
        visited.push_back(frame_node);
        return false;
      }));
  EXPECT_EQ(1u, visited.size());
}

TEST_F(GraphImplOperationsTest, HasFrame) {
  EXPECT_TRUE(GraphImplOperations::HasFrame(page1_.get(), childframe1a_.get()));
  EXPECT_FALSE(
      GraphImplOperations::HasFrame(page1_.get(), childframe2a_.get()));
}

TEST_F(GraphImplOperationsTest, VisitAllWorkerClients) {
  // Complete iteration.
  std::vector<FrameNodeImpl*> visited_frames;
  std::vector<WorkerNodeImpl*> visited_workers;
  EXPECT_TRUE(GraphImplOperations::VisitAllWorkerClients(
      service_worker_.get(),
      [&visited_frames](FrameNodeImpl* frame) {
        visited_frames.push_back(frame);
        return true;
      },
      [&visited_workers](WorkerNodeImpl* worker) {
        visited_workers.push_back(worker);
        return true;
      }));
  // Each client should only be visited once.
  EXPECT_THAT(visited_frames,
              UnorderedElementsAre(mainframe1_.get(), mainframe2_.get()));
  EXPECT_THAT(
      visited_workers,
      UnorderedElementsAre(dedicated_worker_.get(), dedicated_worker2_.get(),
                           dedicated_worker3_.get(), shared_worker_.get()));

  // Stop iteration from frame.
  visited_frames.clear();
  visited_workers.clear();
  EXPECT_FALSE(GraphImplOperations::VisitAllWorkerClients(
      service_worker_.get(),
      [&visited_frames](FrameNodeImpl* frame) {
        visited_frames.push_back(frame);
        return false;
      },
      [&visited_workers](WorkerNodeImpl* worker) {
        visited_workers.push_back(worker);
        return true;
      }));
  // Only `mainframe1_` is a direct client of `service_worker_`.
  EXPECT_THAT(visited_frames, UnorderedElementsAre(mainframe1_.get()));
  EXPECT_TRUE(visited_workers.empty());

  // Stop iteration from worker.
  visited_frames.clear();
  visited_workers.clear();
  EXPECT_FALSE(GraphImplOperations::VisitAllWorkerClients(
      service_worker_.get(),
      [&visited_frames](FrameNodeImpl* frame) {
        visited_frames.push_back(frame);
        return true;
      },
      [&visited_workers](WorkerNodeImpl* worker) {
        visited_workers.push_back(worker);
        return false;
      }));
  // In preorder traversal, workers are visited before `mainframe2_` so
  // iteration stops before it's added.
  EXPECT_THAT(visited_frames, UnorderedElementsAre(mainframe1_.get()));
  // Only 1 worker should be visited, but it could be any direct client.
  EXPECT_EQ(1u, visited_workers.size());

  // For safety, make sure to escape from infinite loops, even though this
  // client topography should be impossible in practice.
  dedicated_worker_->AddClientWorker(dedicated_worker3_.get());
  EXPECT_TRUE(GraphImplOperations::VisitAllWorkerClients(
      service_worker_.get(), [](FrameNodeImpl* frame) { return true; },
      [](WorkerNodeImpl* worker) { return true; }));
  dedicated_worker_->RemoveClientWorker(dedicated_worker3_.get());
}

}  // namespace performance_manager

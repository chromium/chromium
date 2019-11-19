// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/worker_node_impl.h"

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class WorkerNodeImplTest : public GraphTestHarness {
 public:
 protected:
};

}  // namespace

TEST_F(WorkerNodeImplTest, SafeDowncast) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto worker = CreateNode<WorkerNodeImpl>(WorkerNode::WorkerType::kDedicated,
                                           process.get());
  WorkerNode* node = worker.get();
  EXPECT_EQ(worker.get(), WorkerNodeImpl::FromNode(node));
  NodeBase* base = worker.get();
  EXPECT_EQ(base, NodeBase::FromNode(node));
  EXPECT_EQ(static_cast<Node*>(node), base->ToNode());
}

using WorkerNodeImplDeathTest = WorkerNodeImplTest;

TEST_F(WorkerNodeImplDeathTest, SafeDowncast) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto worker = CreateNode<WorkerNodeImpl>(WorkerNode::WorkerType::kDedicated,
                                           process.get());
  ASSERT_DEATH_IF_SUPPORTED(FrameNodeImpl::FromNodeBase(worker.get()), "");
}

TEST_F(WorkerNodeImplTest, ConstProperties) {
  const WorkerNode::WorkerType kWorkerType = WorkerNode::WorkerType::kShared;
  const std::string kTestBrowserContextId =
      base::UnguessableToken::Create().ToString();
  auto process = CreateNode<ProcessNodeImpl>();
  static const GURL kTestUrl("testurl.com");
  static const base::UnguessableToken kTestDevToolsToken =
      base::UnguessableToken::Create();

  auto worker_impl = CreateNode<WorkerNodeImpl>(kWorkerType, process.get(),
                                                kTestBrowserContextId, kTestUrl,
                                                kTestDevToolsToken);

  // Test private interface.
  EXPECT_EQ(worker_impl->browser_context_id(), kTestBrowserContextId);
  EXPECT_EQ(worker_impl->worker_type(), kWorkerType);
  EXPECT_EQ(worker_impl->process_node(), process.get());
  EXPECT_EQ(worker_impl->url(), kTestUrl);
  EXPECT_EQ(worker_impl->dev_tools_token(), kTestDevToolsToken);

  // Test public interface.
  const WorkerNode* worker = worker_impl.get();

  EXPECT_EQ(worker->GetBrowserContextID(), kTestBrowserContextId);
  EXPECT_EQ(worker->GetWorkerType(), kWorkerType);
  EXPECT_EQ(worker->GetProcessNode(), process.get());
  EXPECT_EQ(worker->GetURL(), kTestUrl);
  EXPECT_EQ(worker->GetDevToolsToken(), kTestDevToolsToken);
}

// Create a worker of each type and register the frame as a client of each.
TEST_F(WorkerNodeImplTest, AddWorkerNodes) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame = CreateFrameNodeAutoId(process.get(), page.get());
  auto dedicated_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, process.get());
  auto shared_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kShared, process.get());
  auto service_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kService, process.get());

  // Each workers have no clients.
  EXPECT_TRUE(dedicated_worker->client_frames().empty());
  EXPECT_TRUE(shared_worker->client_frames().empty());
  EXPECT_TRUE(service_worker->client_frames().empty());

  // The client frame doesn't have any child worker yet.
  EXPECT_TRUE(frame->child_worker_nodes().empty());

  dedicated_worker->AddClientFrame(frame.get());
  shared_worker->AddClientFrame(frame.get());
  service_worker->AddClientFrame(frame.get());

  // Each workers have one client frame.
  EXPECT_EQ(dedicated_worker->client_frames().size(), 1u);
  EXPECT_EQ(shared_worker->client_frames().size(), 1u);
  EXPECT_EQ(service_worker->client_frames().size(), 1u);

  // The client frame knows about the 3 workers.
  EXPECT_EQ(frame->child_worker_nodes().size(), 3u);

  // Remove client connections.
  service_worker->RemoveClientFrame(frame.get());
  shared_worker->RemoveClientFrame(frame.get());
  dedicated_worker->RemoveClientFrame(frame.get());
}

// Create a frame and a worker of each type that are all clients of the service
// worker.
TEST_F(WorkerNodeImplTest, ClientsOfServiceWorkers) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame = CreateFrameNodeAutoId(process.get(), page.get());
  auto dedicated_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, process.get());
  auto shared_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kShared, process.get());
  auto service_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kService, process.get());

  // The service worker has no clients.
  EXPECT_TRUE(service_worker->client_frames().empty());
  EXPECT_TRUE(service_worker->client_workers().empty());

  // The frame and the other workers aren't connected to the service worker yet.
  EXPECT_TRUE(frame->child_worker_nodes().empty());
  EXPECT_TRUE(dedicated_worker->child_workers().empty());
  EXPECT_TRUE(shared_worker->child_workers().empty());

  service_worker->AddClientFrame(frame.get());
  service_worker->AddClientWorker(dedicated_worker.get());
  service_worker->AddClientWorker(shared_worker.get());

  EXPECT_EQ(service_worker->client_frames().size(), 1u);
  EXPECT_EQ(service_worker->client_workers().size(), 2u);

  EXPECT_EQ(frame->child_worker_nodes().size(), 1u);
  EXPECT_EQ(shared_worker->child_workers().size(), 1u);
  EXPECT_EQ(dedicated_worker->child_workers().size(), 1u);

  // Remove client connections.
  service_worker->RemoveClientWorker(shared_worker.get());
  service_worker->RemoveClientWorker(dedicated_worker.get());
  service_worker->RemoveClientFrame(frame.get());
}

// Create a hierarchy of nested dedicated workers where the parent one has 2
// children and one grandchildren.
TEST_F(WorkerNodeImplTest, NestedDedicatedWorkers) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame = CreateFrameNodeAutoId(process.get(), page.get());
  auto parent_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, process.get());
  auto child_worker_1 = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, process.get());
  auto child_worker_2 = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, process.get());
  auto grandchild_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, process.get());

  parent_worker->AddClientFrame(frame.get());
  child_worker_1->AddClientWorker(parent_worker.get());
  child_worker_2->AddClientWorker(parent_worker.get());
  grandchild_worker->AddClientWorker(child_worker_1.get());

  EXPECT_EQ(parent_worker->client_frames().size(), 1u);
  EXPECT_EQ(parent_worker->client_workers().size(), 0u);

  EXPECT_EQ(parent_worker->child_workers().size(), 2u);

  grandchild_worker->RemoveClientWorker(child_worker_1.get());
  child_worker_2->RemoveClientWorker(parent_worker.get());
  child_worker_1->RemoveClientWorker(parent_worker.get());
  parent_worker->RemoveClientFrame(frame.get());
}

class TestWorkerNodeObserver : public WorkerNodeObserver {
 public:
  TestWorkerNodeObserver() = default;
  ~TestWorkerNodeObserver() override = default;

  void OnWorkerNodeAdded(const WorkerNode* worker_node) override {
    EXPECT_TRUE(client_frames_.insert({worker_node, {}}).second);
    EXPECT_TRUE(client_workers_.insert({worker_node, {}}).second);
  }

  void OnBeforeWorkerNodeRemoved(const WorkerNode* worker_node) override {
    EXPECT_TRUE(client_frames_.empty());
    EXPECT_TRUE(client_workers_.empty());
    EXPECT_EQ(client_frames_.erase(worker_node), 1u);
    EXPECT_EQ(client_workers_.erase(worker_node), 1u);
  }

  void OnClientFrameAdded(const WorkerNode* worker_node,
                          const FrameNode* client_frame_node) override {
    auto& client_frames = client_frames_.find(worker_node)->second;
    EXPECT_TRUE(client_frames.insert(client_frame_node).second);
  }
  void OnBeforeClientFrameRemoved(const WorkerNode* worker_node,
                                  const FrameNode* client_frame_node) override {
    auto& client_frames = client_frames_.find(worker_node)->second;
    EXPECT_EQ(client_frames.erase(client_frame_node), 1u);
  }
  void OnClientWorkerAdded(const WorkerNode* worker_node,
                           const WorkerNode* client_worker_node) override {
    auto& client_workers = client_workers_.find(worker_node)->second;
    EXPECT_TRUE(client_workers.insert(client_worker_node).second);
  }
  void OnBeforeClientWorkerRemoved(
      const WorkerNode* worker_node,
      const WorkerNode* client_worker_node) override {
    auto& client_workers = client_workers_.find(worker_node)->second;
    EXPECT_EQ(client_workers.erase(client_worker_node), 1u);
  }

  const base::flat_map<const WorkerNode*, base::flat_set<const FrameNode*>>&
  client_frames() {
    return client_frames_;
  }
  const base::flat_map<const WorkerNode*, base::flat_set<const WorkerNode*>>&
  client_workers() {
    return client_workers_;
  }

 private:
  base::flat_map<const WorkerNode*, base::flat_set<const FrameNode*>>
      client_frames_;
  base::flat_map<const WorkerNode*, base::flat_set<const WorkerNode*>>
      client_workers_;

  DISALLOW_COPY_AND_ASSIGN(TestWorkerNodeObserver);
};

// Same as the AddWorkerNodes test, but the graph is verified through the
// WorkerNodeObserver interface.
TEST_F(WorkerNodeImplTest, Observer_AddWorkerNodes) {
  TestWorkerNodeObserver worker_node_observer;
  graph()->AddWorkerNodeObserver(&worker_node_observer);

  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame = CreateFrameNodeAutoId(process.get(), page.get());
  auto dedicated_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, process.get());
  auto shared_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kShared, process.get());
  auto service_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kService, process.get());

  dedicated_worker->AddClientFrame(frame.get());
  shared_worker->AddClientFrame(frame.get());
  service_worker->AddClientFrame(frame.get());

  // 3 different workers observed.
  EXPECT_EQ(worker_node_observer.client_frames().size(), 3u);
  for (const auto& worker_and_client_frames :
       worker_node_observer.client_frames()) {
    // For each worker, check that |frame| is a client.
    const base::flat_set<const FrameNode*>& client_frames =
        worker_and_client_frames.second;
    EXPECT_TRUE(client_frames.find(frame.get()) != client_frames.end());
  }

  // Remove client connections.
  service_worker->RemoveClientFrame(frame.get());
  shared_worker->RemoveClientFrame(frame.get());
  dedicated_worker->RemoveClientFrame(frame.get());

  graph()->RemoveWorkerNodeObserver(&worker_node_observer);
}

// Same as the ClientsOfServiceWorkers test, but the graph is verified through
// the WorkerNodeObserver interface.
TEST_F(WorkerNodeImplTest, Observer_ClientsOfServiceWorkers) {
  TestWorkerNodeObserver worker_node_observer;
  graph()->AddWorkerNodeObserver(&worker_node_observer);

  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame = CreateFrameNodeAutoId(process.get(), page.get());
  auto dedicated_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, process.get());
  auto shared_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kShared, process.get());
  auto service_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kService, process.get());

  service_worker->AddClientFrame(frame.get());
  service_worker->AddClientWorker(dedicated_worker.get());
  service_worker->AddClientWorker(shared_worker.get());

  // 3 different workers observed.
  EXPECT_EQ(worker_node_observer.client_frames().size(), 3u);

  // Check clients of the service worker.
  const base::flat_set<const FrameNode*>& client_frames =
      worker_node_observer.client_frames().find(service_worker.get())->second;
  EXPECT_TRUE(client_frames.find(frame.get()) != client_frames.end());

  const base::flat_set<const WorkerNode*>& client_workers =
      worker_node_observer.client_workers().find(service_worker.get())->second;
  EXPECT_TRUE(client_workers.find(dedicated_worker.get()) !=
              client_workers.end());
  EXPECT_TRUE(client_workers.find(shared_worker.get()) != client_workers.end());

  // Remove client connections.
  service_worker->RemoveClientWorker(shared_worker.get());
  service_worker->RemoveClientWorker(dedicated_worker.get());
  service_worker->RemoveClientFrame(frame.get());

  graph()->RemoveWorkerNodeObserver(&worker_node_observer);
}

}  // namespace performance_manager

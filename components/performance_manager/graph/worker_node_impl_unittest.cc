// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/worker_node_impl.h"

#include "base/task/task_traits.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager {

namespace {

class WorkerNodeImplTest : public GraphTestHarness {
 public:
 protected:
};

// Mock observer for the basic ObserverWorks test.
class LenientMockObserver : public WorkerNodeImpl::Observer {
 public:
  LenientMockObserver() = default;
  ~LenientMockObserver() override = default;

  MOCK_METHOD(void, OnWorkerNodeAdded, (const WorkerNode*), (override));
  MOCK_METHOD(void, OnBeforeWorkerNodeRemoved, (const WorkerNode*), (override));
  MOCK_METHOD(void,
              OnFinalResponseURLDetermined,
              (const WorkerNode*),
              (override));
  MOCK_METHOD(void,
              OnBeforeClientFrameAdded,
              (const WorkerNode*, const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnClientFrameAdded,
              (const WorkerNode*, const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnBeforeClientFrameRemoved,
              (const WorkerNode*, const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnBeforeClientWorkerAdded,
              (const WorkerNode*, const WorkerNode*),
              (override));
  MOCK_METHOD(void,
              OnClientWorkerAdded,
              (const WorkerNode*, const WorkerNode*),
              (override));
  MOCK_METHOD(void,
              OnBeforeClientWorkerRemoved,
              (const WorkerNode*, const WorkerNode*),
              (override));
  MOCK_METHOD(void,
              OnPriorityAndReasonChanged,
              (const WorkerNode*, const PriorityAndReason&),
              (override));
};

using MockObserver = ::testing::StrictMock<LenientMockObserver>;

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;

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
  static const blink::WorkerToken kTestWorkerToken;
  static const auto kTestWorkerOrigin =
      url::Origin::Create(GURL("https://example.com"));

  auto worker_impl = CreateNode<WorkerNodeImpl>(
      kWorkerType, process.get(), kTestBrowserContextId, kTestWorkerToken,
      kTestWorkerOrigin);

  // Test private interface.
  EXPECT_EQ(worker_impl->process_node(), process.get());

  // Test public interface.
  const WorkerNode* worker = worker_impl.get();

  EXPECT_EQ(worker->GetBrowserContextID(), kTestBrowserContextId);
  EXPECT_EQ(worker->GetWorkerType(), kWorkerType);
  EXPECT_EQ(worker->GetProcessNode(), process.get());
  EXPECT_EQ(worker->GetWorkerToken(), kTestWorkerToken);
  EXPECT_EQ(worker->GetOrigin(), kTestWorkerOrigin);
}

TEST_F(WorkerNodeImplTest, OnFinalResponseURLDetermined) {
  auto process = CreateNode<ProcessNodeImpl>();
  static const GURL kTestUrl("testurl.com");

  auto worker_impl = CreateNode<WorkerNodeImpl>(WorkerNode::WorkerType::kShared,
                                                process.get());

  // Initially empty.
  EXPECT_TRUE(worker_impl->GetURL().is_empty());

  // Set when OnFinalResponseURLDetermined() is called.
  worker_impl->OnFinalResponseURLDetermined(kTestUrl);
  EXPECT_EQ(worker_impl->GetURL(), kTestUrl);
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

TEST_F(WorkerNodeImplTest, PriorityAndReason) {
  auto process = CreateNode<ProcessNodeImpl>();
  constexpr PriorityAndReason kTestPriorityAndReason(
      base::TaskPriority::HIGHEST, "Test reason");

  auto worker_impl = CreateNode<WorkerNodeImpl>(WorkerNode::WorkerType::kShared,
                                                process.get());

  // Initially the default priority.
  EXPECT_EQ(worker_impl->GetPriorityAndReason(),
            PriorityAndReason(base::TaskPriority::LOWEST,
                              WorkerNodeImpl::kDefaultPriorityReason));

  worker_impl->SetPriorityAndReason(kTestPriorityAndReason);

  EXPECT_EQ(worker_impl->GetPriorityAndReason(), kTestPriorityAndReason);
}

TEST_F(WorkerNodeImplTest, ObserverWorks) {
  auto process = CreateNode<ProcessNodeImpl>();

  MockObserver head_obs;
  MockObserver obs;
  MockObserver tail_obs;
  graph()->AddWorkerNodeObserver(&head_obs);
  graph()->AddWorkerNodeObserver(&obs);
  graph()->AddWorkerNodeObserver(&tail_obs);

  // Remove observers at the head and tail of the list inside a callback, and
  // expect that `obs` is still notified correctly.
  EXPECT_CALL(head_obs, OnWorkerNodeAdded(_)).WillOnce(InvokeWithoutArgs([&] {
    graph()->RemoveWorkerNodeObserver(&head_obs);
    graph()->RemoveWorkerNodeObserver(&tail_obs);
  }));
  // `tail_obs` should not be notified as it was removed.
  EXPECT_CALL(tail_obs, OnWorkerNodeAdded(_)).Times(0);

  // Create a worker node and expect a matching call to "OnWorkerNodeAdded".
  const WorkerNode* worker_node = nullptr;
  EXPECT_CALL(obs, OnWorkerNodeAdded(_))
      .WillOnce(Invoke([&](const WorkerNode* node) { worker_node = node; }));
  auto dedicated_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, process.get());
  EXPECT_EQ(worker_node, dedicated_worker.get());

  // Re-entrant iteration should work.
  EXPECT_CALL(obs, OnFinalResponseURLDetermined(worker_node))
      .WillOnce(InvokeWithoutArgs([&] {
        dedicated_worker->SetPriorityAndReason(PriorityAndReason(
            base::TaskPriority::USER_BLOCKING, "test priority"));
      }));
  EXPECT_CALL(obs, OnPriorityAndReasonChanged(worker_node, _));
  dedicated_worker->OnFinalResponseURLDetermined(GURL("https://example.com"));

  graph()->RemoveWorkerNodeObserver(&obs);
}

// Same as the AddWorkerNodes test, but the graph is verified through the
// WorkerNodeObserver interface.
TEST_F(WorkerNodeImplTest, Observer_AddWorkerNodes) {
  InSequence s;

  MockObserver obs;
  graph()->AddWorkerNodeObserver(&obs);

  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame = CreateFrameNodeAutoId(process.get(), page.get());

  // Create workers.
  EXPECT_CALL(obs, OnWorkerNodeAdded(_)).Times(3);

  auto dedicated_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, process.get());
  auto shared_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kShared, process.get());
  auto service_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kService, process.get());

  // Add client connections.
  EXPECT_CALL(obs,
              OnBeforeClientFrameAdded(dedicated_worker.get(), frame.get()));
  EXPECT_CALL(obs, OnClientFrameAdded(dedicated_worker.get(), frame.get()));
  EXPECT_CALL(obs, OnBeforeClientFrameAdded(shared_worker.get(), frame.get()));
  EXPECT_CALL(obs, OnClientFrameAdded(shared_worker.get(), frame.get()));
  EXPECT_CALL(obs, OnBeforeClientFrameAdded(service_worker.get(), frame.get()));
  EXPECT_CALL(obs, OnClientFrameAdded(service_worker.get(), frame.get()));

  dedicated_worker->AddClientFrame(frame.get());
  shared_worker->AddClientFrame(frame.get());
  service_worker->AddClientFrame(frame.get());

  // Remove client connections.
  EXPECT_CALL(obs,
              OnBeforeClientFrameRemoved(service_worker.get(), frame.get()));
  EXPECT_CALL(obs,
              OnBeforeClientFrameRemoved(shared_worker.get(), frame.get()));
  EXPECT_CALL(obs,
              OnBeforeClientFrameRemoved(dedicated_worker.get(), frame.get()));

  service_worker->RemoveClientFrame(frame.get());
  shared_worker->RemoveClientFrame(frame.get());
  dedicated_worker->RemoveClientFrame(frame.get());

  EXPECT_CALL(obs, OnBeforeWorkerNodeRemoved(service_worker.get()));
  EXPECT_CALL(obs, OnBeforeWorkerNodeRemoved(shared_worker.get()));
  EXPECT_CALL(obs, OnBeforeWorkerNodeRemoved(dedicated_worker.get()));

  // Clean up workers.
  service_worker.reset();
  shared_worker.reset();
  dedicated_worker.reset();

  graph()->RemoveWorkerNodeObserver(&obs);
}

// Same as the ClientsOfServiceWorkers test, but the graph is verified through
// the WorkerNodeObserver interface.
TEST_F(WorkerNodeImplTest, Observer_ClientsOfServiceWorkers) {
  InSequence s;

  MockObserver obs;
  graph()->AddWorkerNodeObserver(&obs);

  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame = CreateFrameNodeAutoId(process.get(), page.get());

  // Create workers.
  EXPECT_CALL(obs, OnWorkerNodeAdded(_)).Times(3);

  auto dedicated_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, process.get());
  auto shared_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kShared, process.get());
  auto service_worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kService, process.get());

  // Add client connections.
  EXPECT_CALL(obs, OnBeforeClientFrameAdded(service_worker.get(), frame.get()));
  EXPECT_CALL(obs, OnClientFrameAdded(service_worker.get(), frame.get()));
  EXPECT_CALL(obs, OnBeforeClientWorkerAdded(service_worker.get(),
                                             dedicated_worker.get()));
  EXPECT_CALL(
      obs, OnClientWorkerAdded(service_worker.get(), dedicated_worker.get()));
  EXPECT_CALL(obs, OnBeforeClientWorkerAdded(service_worker.get(),
                                             shared_worker.get()));
  EXPECT_CALL(obs,
              OnClientWorkerAdded(service_worker.get(), shared_worker.get()));

  service_worker->AddClientFrame(frame.get());
  service_worker->AddClientWorker(dedicated_worker.get());
  service_worker->AddClientWorker(shared_worker.get());


  // Remove client connections.
  EXPECT_CALL(obs, OnBeforeClientWorkerRemoved(service_worker.get(),
                                               shared_worker.get()));
  EXPECT_CALL(obs, OnBeforeClientWorkerRemoved(service_worker.get(),
                                               dedicated_worker.get()));
  EXPECT_CALL(obs,
              OnBeforeClientFrameRemoved(service_worker.get(), frame.get()));

  service_worker->RemoveClientWorker(shared_worker.get());
  service_worker->RemoveClientWorker(dedicated_worker.get());
  service_worker->RemoveClientFrame(frame.get());

  // Clean up workers.
  EXPECT_CALL(obs, OnBeforeWorkerNodeRemoved(service_worker.get()));
  EXPECT_CALL(obs, OnBeforeWorkerNodeRemoved(shared_worker.get()));
  EXPECT_CALL(obs, OnBeforeWorkerNodeRemoved(dedicated_worker.get()));

  service_worker.reset();
  shared_worker.reset();
  dedicated_worker.reset();

  graph()->RemoveWorkerNodeObserver(&obs);
}

TEST_F(WorkerNodeImplTest, Observer_OnFinalResponseURLDetermined) {
  InSequence s;

  MockObserver obs;
  graph()->AddWorkerNodeObserver(&obs);

  auto process = CreateNode<ProcessNodeImpl>();

  // Create the worker.
  EXPECT_CALL(obs, OnWorkerNodeAdded(_));
  auto worker = CreateNode<WorkerNodeImpl>(WorkerNode::WorkerType::kDedicated,
                                           process.get());

  // Set the final response URL.
  EXPECT_CALL(obs, OnFinalResponseURLDetermined(worker.get()));
  worker->OnFinalResponseURLDetermined(GURL("testurl.com"));

  graph()->RemoveWorkerNodeObserver(&obs);
}

TEST_F(WorkerNodeImplTest, Observer_OnPriorityAndReasonChanged) {
  InSequence s;

  MockObserver obs;
  graph()->AddWorkerNodeObserver(&obs);

  auto process = CreateNode<ProcessNodeImpl>();

  // Create the worker.
  EXPECT_CALL(obs, OnWorkerNodeAdded(_));
  auto worker = CreateNode<WorkerNodeImpl>(WorkerNode::WorkerType::kDedicated,
                                           process.get());

  static const PriorityAndReason kPriorityAndReason(base::TaskPriority::HIGHEST,
                                                    "this is a reason!");
  EXPECT_CALL(obs, OnPriorityAndReasonChanged(worker.get(), _));
  worker->SetPriorityAndReason(kPriorityAndReason);

  graph()->RemoveWorkerNodeObserver(&obs);
}

}  // namespace performance_manager

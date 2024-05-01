// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/worker_node_impl.h"

#include <ostream>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
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

using ::testing::ElementsAre;

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

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;

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

enum class NotificationMethod {
  kOnWorkerNodeAdded,
  kOnBeforeWorkerNodeRemoved,
  kOnFinalResponseURLDetermined,
  kOnBeforeClientFrameAdded,
  kOnClientFrameAdded,
  kOnBeforeClientFrameRemoved,
  kOnBeforeClientWorkerAdded,
  kOnClientWorkerAdded,
  kOnBeforeClientWorkerRemoved,
  kOnPriorityAndReasonChanged,
};

std::ostream& operator<<(std::ostream& os, NotificationMethod method) {
  switch (method) {
    case NotificationMethod::kOnWorkerNodeAdded:
      return os << "OnWorkerNodeAdded";
    case NotificationMethod::kOnBeforeWorkerNodeRemoved:
      return os << "OnBeforeWorkerNodeRemoved";
    case NotificationMethod::kOnFinalResponseURLDetermined:
      return os << "OnFinalResponseURLDetermined";
    case NotificationMethod::kOnBeforeClientFrameAdded:
      return os << "OnBeforeClientFrameAdded";
    case NotificationMethod::kOnClientFrameAdded:
      return os << "OnClientFrameAdded";
    case NotificationMethod::kOnBeforeClientFrameRemoved:
      return os << "OnBeforeClientFrameRemoved";
    case NotificationMethod::kOnClientWorkerAdded:
      return os << "OnClientWorkerAdded";
    case NotificationMethod::kOnBeforeClientWorkerAdded:
      return os << "OnBeforeClientWorkerAdded";
    case NotificationMethod::kOnBeforeClientWorkerRemoved:
      return os << "OnBeforeClientWorkerRemoved";
    case NotificationMethod::kOnPriorityAndReasonChanged:
      return os << "OnPriorityAndReasonChanged";
  }
  return os << "Unknown:" << static_cast<int>(method);
}

// All clients of a WorkerNode, including those in the process of being added.
struct WorkerClients {
  // Client frames.
  base::flat_set<raw_ptr<const FrameNode, CtnExperimental>> frames;

  // Clients that are about to be added to `frames`.
  base::flat_set<raw_ptr<const FrameNode, CtnExperimental>> frames_to_add;

  // Client workers.
  base::flat_set<raw_ptr<const WorkerNode, CtnExperimental>> workers;

  // Clients that are about to be added to `workers`.
  base::flat_set<raw_ptr<const WorkerNode, CtnExperimental>> workers_to_add;
};

// A more complicated observer that tests the consistency of client
// relationships.
class TestWorkerNodeObserver : public WorkerNodeObserver {
 public:
  using FrameNodeSet =
      base::flat_set<raw_ptr<const FrameNode, CtnExperimental>>;
  using WorkerNodeSet =
      base::flat_set<raw_ptr<const WorkerNode, CtnExperimental>>;

  TestWorkerNodeObserver() = default;

  TestWorkerNodeObserver(const TestWorkerNodeObserver&) = delete;
  TestWorkerNodeObserver& operator=(const TestWorkerNodeObserver&) = delete;

  ~TestWorkerNodeObserver() override = default;

  void OnWorkerNodeAdded(const WorkerNode* worker_node) override {
    methods_called_.push_back(NotificationMethod::kOnWorkerNodeAdded);

    // Create an empty client map for the worker.
    EXPECT_TRUE(clients_.insert({worker_node, {}}).second);
  }

  void OnBeforeWorkerNodeRemoved(const WorkerNode* worker_node) override {
    methods_called_.push_back(NotificationMethod::kOnBeforeWorkerNodeRemoved);

    // Ensure all clients were disconnected before deleting the worker.
    const WorkerClients& clients = clients_.at(worker_node);
    EXPECT_TRUE(clients.frames.empty());
    EXPECT_TRUE(clients.frames_to_add.empty());
    EXPECT_TRUE(clients.workers.empty());
    EXPECT_TRUE(clients.workers_to_add.empty());
    EXPECT_EQ(clients_.erase(worker_node), 1u);
  }

  void OnFinalResponseURLDetermined(const WorkerNode* worker_node) override {
    methods_called_.push_back(
        NotificationMethod::kOnFinalResponseURLDetermined);
  }

  void OnBeforeClientFrameAdded(const WorkerNode* worker_node,
                                const FrameNode* client_node) override {
    methods_called_.push_back(NotificationMethod::kOnBeforeClientFrameAdded);

    // Ensure OnBeforeClientFrameAdded is only called once for this client, and
    // OnClientFrameAdded wasn't called yet.
    WorkerClients& clients = clients_.at(worker_node);
    EXPECT_FALSE(base::Contains(clients.frames, client_node));
    EXPECT_FALSE(base::Contains(clients.frames_to_add, client_node));
    clients.frames_to_add.insert(client_node);
  }

  void OnClientFrameAdded(const WorkerNode* worker_node,
                          const FrameNode* client_node) override {
    methods_called_.push_back(NotificationMethod::kOnClientFrameAdded);

    // Ensure OnBeforeClientFrameAdded was already called for this client, and
    // OnClientFrameAdded is only called once.
    WorkerClients& clients = clients_.at(worker_node);
    EXPECT_FALSE(base::Contains(clients.frames, client_node));
    EXPECT_TRUE(base::Contains(clients.frames_to_add, client_node));
    clients.frames_to_add.erase(client_node);
    clients.frames.insert(client_node);
  }

  void OnBeforeClientFrameRemoved(const WorkerNode* worker_node,
                                  const FrameNode* client_node) override {
    methods_called_.push_back(NotificationMethod::kOnBeforeClientFrameRemoved);

    // Ensure OnBeforeClientFrameRemoved is only called once for this client.
    WorkerClients& clients = clients_.at(worker_node);
    EXPECT_TRUE(base::Contains(clients.frames, client_node));
    EXPECT_FALSE(base::Contains(clients.frames_to_add, client_node));
    clients.frames.erase(client_node);
  }

  void OnBeforeClientWorkerAdded(const WorkerNode* worker_node,
                                 const WorkerNode* client_node) override {
    methods_called_.push_back(NotificationMethod::kOnBeforeClientWorkerAdded);

    // Ensure OnBeforeClientWorkerAdded is only called once for this client, and
    // OnClientWorkerAdded wasn't called yet.
    WorkerClients& clients = clients_.at(worker_node);
    EXPECT_FALSE(base::Contains(clients.workers, client_node));
    EXPECT_FALSE(base::Contains(clients.workers_to_add, client_node));
    clients.workers_to_add.insert(client_node);
  }

  void OnClientWorkerAdded(const WorkerNode* worker_node,
                           const WorkerNode* client_node) override {
    methods_called_.push_back(NotificationMethod::kOnClientWorkerAdded);

    // Ensure OnBeforeClientWorkerAdded was already called for this client, and
    // OnClientWorkerAdded is only called once.
    WorkerClients& clients = clients_.at(worker_node);
    EXPECT_FALSE(base::Contains(clients.workers, client_node));
    EXPECT_TRUE(base::Contains(clients.workers_to_add, client_node));
    clients.workers_to_add.erase(client_node);
    clients.workers.insert(client_node);
  }

  void OnBeforeClientWorkerRemoved(const WorkerNode* worker_node,
                                   const WorkerNode* client_node) override {
    methods_called_.push_back(NotificationMethod::kOnBeforeClientWorkerRemoved);

    // Ensure OnBeforeClientWorkerRemoved is only called once for this client.
    WorkerClients& clients = clients_.at(worker_node);
    EXPECT_TRUE(base::Contains(clients.workers, client_node));
    EXPECT_FALSE(base::Contains(clients.workers_to_add, client_node));
    clients.workers.erase(client_node);
  }

  void OnPriorityAndReasonChanged(
      const WorkerNode* worker_node,
      const PriorityAndReason& previous_value) override {
    methods_called_.push_back(NotificationMethod::kOnPriorityAndReasonChanged);
  }

  const base::flat_map<const WorkerNode*, WorkerClients>& clients() const {
    return clients_;
  }

  std::vector<NotificationMethod> methods_called() const {
    return methods_called_;
  }

 private:
  std::vector<NotificationMethod> methods_called_;
  base::flat_map<const WorkerNode*, WorkerClients> clients_;
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
  EXPECT_EQ(worker_node_observer.clients().size(), 3u);
  for (const auto& [worker_node, clients] : worker_node_observer.clients()) {
    // For each worker, check that `frame` is a client.
    EXPECT_TRUE(base::Contains(clients.frames, frame.get()));
  }

  // Remove client connections.
  service_worker->RemoveClientFrame(frame.get());
  shared_worker->RemoveClientFrame(frame.get());
  dedicated_worker->RemoveClientFrame(frame.get());

  service_worker.reset();
  shared_worker.reset();
  dedicated_worker.reset();

  EXPECT_THAT(worker_node_observer.methods_called(),
              ElementsAre(
                  // 3 workers created
                  NotificationMethod::kOnWorkerNodeAdded,
                  NotificationMethod::kOnWorkerNodeAdded,
                  NotificationMethod::kOnWorkerNodeAdded,
                  // 3 client frames added
                  NotificationMethod::kOnBeforeClientFrameAdded,
                  NotificationMethod::kOnClientFrameAdded,
                  NotificationMethod::kOnBeforeClientFrameAdded,
                  NotificationMethod::kOnClientFrameAdded,
                  NotificationMethod::kOnBeforeClientFrameAdded,
                  NotificationMethod::kOnClientFrameAdded,
                  // 3 client frames removed
                  NotificationMethod::kOnBeforeClientFrameRemoved,
                  NotificationMethod::kOnBeforeClientFrameRemoved,
                  NotificationMethod::kOnBeforeClientFrameRemoved,
                  // 3 workers deleted
                  NotificationMethod::kOnBeforeWorkerNodeRemoved,
                  NotificationMethod::kOnBeforeWorkerNodeRemoved,
                  NotificationMethod::kOnBeforeWorkerNodeRemoved));

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
  EXPECT_EQ(worker_node_observer.clients().size(), 3u);

  // Check clients of the service worker.
  const WorkerClients& clients =
      worker_node_observer.clients().at(service_worker.get());
  EXPECT_TRUE(base::Contains(clients.frames, frame.get()));
  EXPECT_TRUE(base::Contains(clients.workers, dedicated_worker.get()));
  EXPECT_TRUE(base::Contains(clients.workers, shared_worker.get()));

  // Remove client connections.
  service_worker->RemoveClientWorker(shared_worker.get());
  service_worker->RemoveClientWorker(dedicated_worker.get());
  service_worker->RemoveClientFrame(frame.get());

  service_worker.reset();
  shared_worker.reset();
  dedicated_worker.reset();

  EXPECT_THAT(worker_node_observer.methods_called(),
              ElementsAre(
                  // 3 workers created
                  NotificationMethod::kOnWorkerNodeAdded,
                  NotificationMethod::kOnWorkerNodeAdded,
                  NotificationMethod::kOnWorkerNodeAdded,
                  // 3 clients added
                  NotificationMethod::kOnBeforeClientFrameAdded,
                  NotificationMethod::kOnClientFrameAdded,
                  NotificationMethod::kOnBeforeClientWorkerAdded,
                  NotificationMethod::kOnClientWorkerAdded,
                  NotificationMethod::kOnBeforeClientWorkerAdded,
                  NotificationMethod::kOnClientWorkerAdded,
                  // 3 client frames removed
                  NotificationMethod::kOnBeforeClientWorkerRemoved,
                  NotificationMethod::kOnBeforeClientWorkerRemoved,
                  NotificationMethod::kOnBeforeClientFrameRemoved,
                  // 3 workers deleted
                  NotificationMethod::kOnBeforeWorkerNodeRemoved,
                  NotificationMethod::kOnBeforeWorkerNodeRemoved,
                  NotificationMethod::kOnBeforeWorkerNodeRemoved));

  graph()->RemoveWorkerNodeObserver(&worker_node_observer);
}

TEST_F(WorkerNodeImplTest, Observer_OnFinalResponseURLDetermined) {
  TestWorkerNodeObserver worker_node_observer;
  graph()->AddWorkerNodeObserver(&worker_node_observer);

  auto process = CreateNode<ProcessNodeImpl>();
  auto worker = CreateNode<WorkerNodeImpl>(WorkerNode::WorkerType::kDedicated,
                                           process.get());

  EXPECT_FALSE(
      base::Contains(worker_node_observer.methods_called(),
                     NotificationMethod::kOnFinalResponseURLDetermined));
  worker->OnFinalResponseURLDetermined(GURL("testurl.com"));

  EXPECT_THAT(worker_node_observer.methods_called(),
              ElementsAre(NotificationMethod::kOnWorkerNodeAdded,
                          NotificationMethod::kOnFinalResponseURLDetermined));

  graph()->RemoveWorkerNodeObserver(&worker_node_observer);
}

TEST_F(WorkerNodeImplTest, Observer_OnPriorityAndReasonChanged) {
  TestWorkerNodeObserver worker_node_observer;
  graph()->AddWorkerNodeObserver(&worker_node_observer);

  auto process = CreateNode<ProcessNodeImpl>();
  auto worker = CreateNode<WorkerNodeImpl>(WorkerNode::WorkerType::kDedicated,
                                           process.get());

  EXPECT_FALSE(base::Contains(worker_node_observer.methods_called(),
                              NotificationMethod::kOnPriorityAndReasonChanged));
  static const PriorityAndReason kPriorityAndReason(base::TaskPriority::HIGHEST,
                                                    "this is a reason!");
  worker->SetPriorityAndReason(kPriorityAndReason);
  EXPECT_EQ(worker->GetPriorityAndReason(), kPriorityAndReason);

  EXPECT_THAT(worker_node_observer.methods_called(),
              ElementsAre(NotificationMethod::kOnWorkerNodeAdded,
                          NotificationMethod::kOnPriorityAndReasonChanged));

  graph()->RemoveWorkerNodeObserver(&worker_node_observer);
}

}  // namespace performance_manager

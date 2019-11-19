// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/shared_worker_watcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/performance_manager/frame_node_source.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/process_node_source.h"
#include "content/public/browser/shared_worker_instance.h"
#include "content/public/browser/shared_worker_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

// Generates a new sequential int ID. Used for things that need a unique ID.
int GenerateNextId() {
  static int next_id = 0;
  return next_id++;
}

// Generates a unique URL for a fake worker node.
GURL GenerateWorkerUrl() {
  return GURL(base::StringPrintf("https://www.foo.org/worker_script_%d.js",
                                 GenerateNextId()));
}

// Helper function to check that |worker_node| and |client_frame_node| are
// correctly hooked up together.
bool IsWorkerClient(WorkerNodeImpl* worker_node,
                    FrameNodeImpl* client_frame_node) {
  return base::Contains(worker_node->client_frames(), client_frame_node) &&
         base::Contains(client_frame_node->child_worker_nodes(), worker_node);
}

// TestSharedWorkerService -----------------------------------------------------

// A test SharedWorkerService that allows to simulate a worker starting and
// stopping and adding clients to running workers.
class TestSharedWorkerService : public content::SharedWorkerService {
 public:
  TestSharedWorkerService();
  ~TestSharedWorkerService() override;

  // content::SharedWorkerService
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool TerminateWorker(const GURL& url,
                       const std::string& name,
                       const url::Origin& constructor_origin) override;

  // Starts a new shared worker and returns its instance.
  content::SharedWorkerInstance StartSharedWorker(int worker_process_id);

  // Stops a running shared worker.
  void StopSharedWorker(const content::SharedWorkerInstance& instance);

  // Adds a new frame client to an existing worker.
  void AddWorkerClient(const content::SharedWorkerInstance& instance,
                       int client_process_id,
                       int frame_id);

  // Removes an existing frame client from a worker.
  void RemoveWorkerClient(const content::SharedWorkerInstance& instance,
                          int client_process_id,
                          int frame_id);

 private:
  base::ObserverList<Observer> observer_list_;

  // The ID that the next SharedWorkerInstance will be assigned.
  int64_t next_shared_worker_instance_id_ = 0;

  // Contains the set of clients for each running workers.
  base::flat_map<content::SharedWorkerInstance,
                 base::flat_set<std::pair<int, int>>>
      shared_worker_client_frames_;

  DISALLOW_COPY_AND_ASSIGN(TestSharedWorkerService);
};

TestSharedWorkerService::TestSharedWorkerService() = default;

TestSharedWorkerService::~TestSharedWorkerService() = default;

void TestSharedWorkerService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TestSharedWorkerService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool TestSharedWorkerService::TerminateWorker(
    const GURL& url,
    const std::string& name,
    const url::Origin& constructor_origin) {
  return true;
}

content::SharedWorkerInstance TestSharedWorkerService::StartSharedWorker(
    int worker_process_id) {
  // Create a new SharedWorkerInstance and add it to the map.
  GURL worker_url = GenerateWorkerUrl();
  content::SharedWorkerInstance instance(
      next_shared_worker_instance_id_++, worker_url, "SharedWorker",
      url::Origin::Create(worker_url), "",
      network::mojom::ContentSecurityPolicyType::kReport,
      network::mojom::IPAddressSpace::kPublic,
      blink::mojom::SharedWorkerCreationContextType::kSecure);

  bool inserted = shared_worker_client_frames_.insert({instance, {}}).second;
  DCHECK(inserted);

  // Notify observers.
  for (auto& observer : observer_list_) {
    observer.OnWorkerStarted(instance, worker_process_id,
                             base::UnguessableToken::Create());
  }

  return instance;
}

void TestSharedWorkerService::StopSharedWorker(
    const content::SharedWorkerInstance& instance) {
  auto it = shared_worker_client_frames_.find(instance);
  DCHECK(it != shared_worker_client_frames_.end());

  // A stopping worker should have no clients.
  DCHECK(it->second.empty());

  // Notify observers that the worker is terminating.
  for (auto& observer : observer_list_)
    observer.OnBeforeWorkerTerminated(instance);

  // Remove the worker instance from the map.
  shared_worker_client_frames_.erase(it);
}

void TestSharedWorkerService::AddWorkerClient(
    const content::SharedWorkerInstance& instance,
    int client_process_id,
    int frame_id) {
  // Add the frame to the set of clients for this worker.
  auto it = shared_worker_client_frames_.find(instance);
  DCHECK(it != shared_worker_client_frames_.end());

  base::flat_set<std::pair<int, int>>& client_frames = it->second;
  bool inserted = client_frames.insert({client_process_id, frame_id}).second;
  DCHECK(inserted);

  // Then notify observers.
  for (auto& observer : observer_list_)
    observer.OnClientAdded(instance, client_process_id, frame_id);
}

void TestSharedWorkerService::RemoveWorkerClient(
    const content::SharedWorkerInstance& instance,
    int client_process_id,
    int frame_id) {
  // Notify observers.
  for (auto& observer : observer_list_)
    observer.OnClientRemoved(instance, client_process_id, frame_id);

  // Then remove the frame from the set of clients of this worker.
  auto it = shared_worker_client_frames_.find(instance);
  DCHECK(it != shared_worker_client_frames_.end());

  base::flat_set<std::pair<int, int>>& client_frames = it->second;
  size_t removed =
      client_frames.erase(std::make_pair(client_process_id, frame_id));
  DCHECK_EQ(removed, 1u);
}

// TestProcessNodeSource -------------------------------------------------------

// A test ProcessNodeSource that allows creating process nodes on demand to
// "host" frames and workers.
class TestProcessNodeSource : public ProcessNodeSource {
 public:
  TestProcessNodeSource();
  ~TestProcessNodeSource() override;

  // ProcessNodeSource:
  ProcessNodeImpl* GetProcessNode(int render_process_id) override;

  // Creates a process node and returns its generated render process ID.
  int CreateProcessNode();

 private:
  // Maps render process IDs with their associated process node.
  base::flat_map<int, std::unique_ptr<ProcessNodeImpl>> process_node_map_;

  DISALLOW_COPY_AND_ASSIGN(TestProcessNodeSource);
};

TestProcessNodeSource::TestProcessNodeSource() = default;

TestProcessNodeSource::~TestProcessNodeSource() {
  std::vector<std::unique_ptr<NodeBase>> nodes;
  nodes.reserve(process_node_map_.size());
  for (auto& kv : process_node_map_) {
    std::unique_ptr<ProcessNodeImpl> process_node = std::move(kv.second);
    nodes.push_back(std::move(process_node));
  }
  PerformanceManagerImpl::GetInstance()->BatchDeleteNodes(std::move(nodes));
  process_node_map_.clear();
}

ProcessNodeImpl* TestProcessNodeSource::GetProcessNode(int render_process_id) {
  auto it = process_node_map_.find(render_process_id);
  DCHECK(it != process_node_map_.end());
  return it->second.get();
}

int TestProcessNodeSource::CreateProcessNode() {
  // Generate a render process ID for this process node.
  int render_process_id = GenerateNextId();

  // Create the process node and insert it into the map.
  auto process_node = PerformanceManagerImpl::GetInstance()->CreateProcessNode(
      RenderProcessHostProxy());
  bool inserted =
      process_node_map_.insert({render_process_id, std::move(process_node)})
          .second;
  DCHECK(inserted);

  return render_process_id;
}

// TestFrameNodeSource ---------------------------------------------------------

class TestFrameNodeSource : public FrameNodeSource {
 public:
  TestFrameNodeSource();
  ~TestFrameNodeSource() override;

  // FrameNodeSource:
  FrameNodeImpl* GetFrameNode(int render_process_id, int frame_id) override;
  void SubscribeToFrameNode(int render_process_id,
                            int frame_id,
                            OnbeforeFrameNodeRemovedCallback
                                on_before_frame_node_removed_callback) override;
  void UnsubscribeFromFrameNode(int render_process_id, int frame_id) override;

  // Creates a frame node and returns its generated frame id.
  int CreateFrameNode(int render_process_id, ProcessNodeImpl* process_node);

  // Deletes an existing frame node and notify subscribers.
  void DeleteFrameNode(int render_process_id, int frame_id);

 private:
  // Helper function that invokes the OnBeforeFrameNodeRemovedCallback
  // associated with |frame_node| and removes it from the map.
  void InvokeAndRemoveCallback(FrameNodeImpl* frame_node);

  // The page node that hosts all frames.
  std::unique_ptr<PageNodeImpl> page_node_;

  // Maps each frame's render process id and frame id with their associated
  // frame node.
  base::flat_map<std::pair<int, int>, std::unique_ptr<FrameNodeImpl>>
      frame_node_map_;

  // Maps each observed frame node to their callback.
  base::flat_map<FrameNodeImpl*, OnbeforeFrameNodeRemovedCallback>
      frame_node_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(TestFrameNodeSource);
};

TestFrameNodeSource::TestFrameNodeSource()
    : page_node_(PerformanceManagerImpl::GetInstance()->CreatePageNode(
          WebContentsProxy(),
          "page_node_context_id",
          GURL(),
          false,
          false)) {}

TestFrameNodeSource::~TestFrameNodeSource() {
  std::vector<std::unique_ptr<NodeBase>> nodes;
  nodes.push_back(std::move(page_node_));
  nodes.reserve(frame_node_map_.size());
  for (auto& kv : frame_node_map_)
    nodes.push_back(std::move(kv.second));
  PerformanceManagerImpl::GetInstance()->BatchDeleteNodes(std::move(nodes));
  frame_node_map_.clear();
}

FrameNodeImpl* TestFrameNodeSource::GetFrameNode(int render_process_id,
                                                 int frame_id) {
  auto it = frame_node_map_.find(std::make_pair(render_process_id, frame_id));
  return it != frame_node_map_.end() ? it->second.get() : nullptr;
}
void TestFrameNodeSource::SubscribeToFrameNode(
    int render_process_id,
    int frame_id,
    OnbeforeFrameNodeRemovedCallback on_before_frame_node_removed_callback) {
  FrameNodeImpl* frame_node = GetFrameNode(render_process_id, frame_id);
  DCHECK(frame_node);

  bool inserted =
      frame_node_callbacks_
          .insert(std::make_pair(
              frame_node, std::move(on_before_frame_node_removed_callback)))
          .second;
  DCHECK(inserted);
}

void TestFrameNodeSource::UnsubscribeFromFrameNode(int render_process_id,
                                                   int frame_id) {
  FrameNodeImpl* frame_node = GetFrameNode(render_process_id, frame_id);
  DCHECK(frame_node);

  size_t removed = frame_node_callbacks_.erase(frame_node);
  DCHECK_EQ(removed, 1u);
}

// Creates a frame node and returns its frame id.
int TestFrameNodeSource::CreateFrameNode(int render_process_id,
                                         ProcessNodeImpl* process_node) {
  int frame_id = GenerateNextId();
  auto frame_node = PerformanceManagerImpl::GetInstance()->CreateFrameNode(
      process_node, page_node_.get(), nullptr, 0, frame_id,
      base::UnguessableToken::Null(), 0, 0);

  std::pair<int, int> frame_info(render_process_id, frame_id);
  bool inserted = frame_node_map_
                      .insert({std::make_pair(render_process_id, frame_id),
                               std::move(frame_node)})
                      .second;
  DCHECK(inserted);

  return frame_id;
}

void TestFrameNodeSource::DeleteFrameNode(int render_process_id, int frame_id) {
  auto it = frame_node_map_.find(std::make_pair(render_process_id, frame_id));
  DCHECK(it != frame_node_map_.end());

  FrameNodeImpl* frame_node = it->second.get();

  // Notify the subscriber then delete the node.
  InvokeAndRemoveCallback(frame_node);
  PerformanceManagerImpl::GetInstance()->DeleteNode(std::move(it->second));

  frame_node_map_.erase(it);
}

void TestFrameNodeSource::InvokeAndRemoveCallback(FrameNodeImpl* frame_node) {
  auto it = frame_node_callbacks_.find(frame_node);
  DCHECK(it != frame_node_callbacks_.end());

  std::move(it->second).Run(frame_node);

  frame_node_callbacks_.erase(it);
}

}  // namespace

class SharedWorkerWatcherTest : public testing::Test {
 public:
  SharedWorkerWatcherTest();
  ~SharedWorkerWatcherTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Wraps a |graph_callback| and ensures the task completes before returning.
  void CallOnGraphAndWait(
      PerformanceManagerImpl::GraphImplCallback graph_callback);

  // Retrieves the worker node associated with |instance|.
  WorkerNodeImpl* GetWorkerNode(const content::SharedWorkerInstance& instance);

  PerformanceManagerImpl* performance_manager() {
    return performance_manager_.get();
  }

  TestSharedWorkerService* shared_worker_service() {
    return &shared_worker_service_;
  }

  TestProcessNodeSource* process_node_source() {
    return process_node_source_.get();
  }

  TestFrameNodeSource* frame_node_source() { return frame_node_source_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;

  TestSharedWorkerService shared_worker_service_;

  std::unique_ptr<PerformanceManagerImpl> performance_manager_;
  std::unique_ptr<TestProcessNodeSource> process_node_source_;
  std::unique_ptr<TestFrameNodeSource> frame_node_source_;

  // The SharedWorkerWatcher that's being tested.
  std::unique_ptr<SharedWorkerWatcher> shared_worker_watcher_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerWatcherTest);
};

SharedWorkerWatcherTest::SharedWorkerWatcherTest() = default;

SharedWorkerWatcherTest::~SharedWorkerWatcherTest() = default;

void SharedWorkerWatcherTest::SetUp() {
  performance_manager_ = PerformanceManagerImpl::Create(base::DoNothing());

  process_node_source_ = std::make_unique<TestProcessNodeSource>();
  frame_node_source_ = std::make_unique<TestFrameNodeSource>();

  shared_worker_watcher_ = std::make_unique<SharedWorkerWatcher>(
      "browser_context_id", &shared_worker_service_, process_node_source_.get(),
      frame_node_source_.get());
}

void SharedWorkerWatcherTest::TearDown() {
  // Clean up the performance manager correctly.
  shared_worker_watcher_->TearDown();
  shared_worker_watcher_ = nullptr;

  // Delete the TestFrameNodeSource and the TestProcessNodeSource in
  // that order since they own graph nodes.
  frame_node_source_ = nullptr;
  process_node_source_ = nullptr;
  PerformanceManagerImpl::Destroy(std::move(performance_manager_));
}

void SharedWorkerWatcherTest::CallOnGraphAndWait(
    PerformanceManagerImpl::GraphImplCallback graph_callback) {
  base::RunLoop run_loop;
  performance_manager_->CallOnGraphImpl(
      FROM_HERE,
      base::BindLambdaForTesting(
          [graph_callback = std::move(graph_callback),
           quit_closure = run_loop.QuitClosure()](GraphImpl* graph) mutable {
            std::move(graph_callback).Run(graph);
            quit_closure.Run();
          }));
}

WorkerNodeImpl* SharedWorkerWatcherTest::GetWorkerNode(
    const content::SharedWorkerInstance& instance) {
  return shared_worker_watcher_->GetWorkerNode(instance);
}

// This test creates one worker with one client frame.
TEST_F(SharedWorkerWatcherTest, SimpleWorker) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the frame node.
  int frame_id = frame_node_source()->CreateFrameNode(
      render_process_id,
      process_node_source()->GetProcessNode(render_process_id));

  // Create the worker.
  content::SharedWorkerInstance shared_worker_instance =
      shared_worker_service()->StartSharedWorker(render_process_id);

  // Connect the frame to the worker.
  shared_worker_service()->AddWorkerClient(shared_worker_instance,
                                           render_process_id, frame_id);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       worker_node = GetWorkerNode(shared_worker_instance),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_process_id, frame_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->worker_type(), WorkerNode::WorkerType::kShared);
        EXPECT_EQ(worker_node->process_node(), process_node);
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));
      }));

  // Disconnect and clean up the worker.
  shared_worker_service()->RemoveWorkerClient(shared_worker_instance,
                                              render_process_id, frame_id);
  shared_worker_service()->StopSharedWorker(shared_worker_instance);
}

TEST_F(SharedWorkerWatcherTest, CrossProcess) {
  // Create the frame node.
  int frame_process_id = process_node_source()->CreateProcessNode();
  int frame_id = frame_node_source()->CreateFrameNode(
      frame_process_id,
      process_node_source()->GetProcessNode(frame_process_id));

  // Create the worker in a different process.
  int worker_process_id = process_node_source()->CreateProcessNode();
  content::SharedWorkerInstance shared_worker_instance =
      shared_worker_service()->StartSharedWorker(worker_process_id);

  // Connect the frame to the worker.
  shared_worker_service()->AddWorkerClient(shared_worker_instance,
                                           frame_process_id, frame_id);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [worker_process_node =
           process_node_source()->GetProcessNode(worker_process_id),
       worker_node = GetWorkerNode(shared_worker_instance),
       client_process_node =
           process_node_source()->GetProcessNode(frame_process_id),
       client_frame_node = frame_node_source()->GetFrameNode(
           frame_process_id, frame_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->worker_type(), WorkerNode::WorkerType::kShared);
        EXPECT_EQ(worker_node->process_node(), worker_process_node);
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));
      }));

  // Disconnect and clean up the worker.
  shared_worker_service()->RemoveWorkerClient(shared_worker_instance,
                                              frame_process_id, frame_id);
  shared_worker_service()->StopSharedWorker(shared_worker_instance);
}

TEST_F(SharedWorkerWatcherTest, OneWorkerTwoClients) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the worker.
  content::SharedWorkerInstance shared_worker_instance =
      shared_worker_service()->StartSharedWorker(render_process_id);

  // Create 2 client frame nodes and connect them to the worker.
  int frame_id_1 = frame_node_source()->CreateFrameNode(
      render_process_id,
      process_node_source()->GetProcessNode(render_process_id));
  shared_worker_service()->AddWorkerClient(shared_worker_instance,
                                           render_process_id, frame_id_1);

  int frame_id_2 = frame_node_source()->CreateFrameNode(
      render_process_id,
      process_node_source()->GetProcessNode(render_process_id));
  shared_worker_service()->AddWorkerClient(shared_worker_instance,
                                           render_process_id, frame_id_2);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [worker_node = GetWorkerNode(shared_worker_instance),
       client_frame_node_1 =
           frame_node_source()->GetFrameNode(render_process_id, frame_id_1),
       client_frame_node_2 = frame_node_source()->GetFrameNode(
           render_process_id, frame_id_2)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->worker_type(), WorkerNode::WorkerType::kShared);

        // Check frame 1.
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node_1));

        // Check frame 2.
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node_2));
      }));

  // Disconnect and clean up the worker.
  shared_worker_service()->RemoveWorkerClient(shared_worker_instance,
                                              render_process_id, frame_id_1);
  shared_worker_service()->RemoveWorkerClient(shared_worker_instance,
                                              render_process_id, frame_id_2);
  shared_worker_service()->StopSharedWorker(shared_worker_instance);
}

TEST_F(SharedWorkerWatcherTest, OneClientTwoWorkers) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the frame node.
  int frame_id = frame_node_source()->CreateFrameNode(
      render_process_id,
      process_node_source()->GetProcessNode(render_process_id));

  // Create the 2 workers and connect them to the frame.
  content::SharedWorkerInstance shared_worker_instance_1 =
      shared_worker_service()->StartSharedWorker(render_process_id);
  shared_worker_service()->AddWorkerClient(shared_worker_instance_1,
                                           render_process_id, frame_id);

  content::SharedWorkerInstance shared_worker_instance_2 =
      shared_worker_service()->StartSharedWorker(render_process_id);
  shared_worker_service()->AddWorkerClient(shared_worker_instance_2,
                                           render_process_id, frame_id);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [worker_node_1 = GetWorkerNode(shared_worker_instance_1),
       worker_node_2 = GetWorkerNode(shared_worker_instance_2),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_process_id, frame_id)](GraphImpl* graph) {
        // Check worker 1.
        EXPECT_TRUE(graph->NodeInGraph(worker_node_1));
        EXPECT_EQ(worker_node_1->worker_type(),
                  WorkerNode::WorkerType::kShared);
        EXPECT_TRUE(IsWorkerClient(worker_node_1, client_frame_node));

        // Check worker 2.
        EXPECT_TRUE(graph->NodeInGraph(worker_node_2));
        EXPECT_EQ(worker_node_2->worker_type(),
                  WorkerNode::WorkerType::kShared);
        EXPECT_TRUE(IsWorkerClient(worker_node_2, client_frame_node));
      }));

  // Disconnect and clean up the workers.
  shared_worker_service()->RemoveWorkerClient(shared_worker_instance_1,
                                              render_process_id, frame_id);
  shared_worker_service()->StopSharedWorker(shared_worker_instance_1);

  shared_worker_service()->RemoveWorkerClient(shared_worker_instance_2,
                                              render_process_id, frame_id);
  shared_worker_service()->StopSharedWorker(shared_worker_instance_2);
}

TEST_F(SharedWorkerWatcherTest, FrameDestroyed) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the frame node.
  int frame_id = frame_node_source()->CreateFrameNode(
      render_process_id,
      process_node_source()->GetProcessNode(render_process_id));

  // Create the worker.
  content::SharedWorkerInstance shared_worker_instance =
      shared_worker_service()->StartSharedWorker(render_process_id);

  // Connect the frame to the worker.
  shared_worker_service()->AddWorkerClient(shared_worker_instance,
                                           render_process_id, frame_id);

  // Check that everything is wired up correctly.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [worker_node = GetWorkerNode(shared_worker_instance),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_process_id, frame_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));
      }));

  frame_node_source()->DeleteFrameNode(render_process_id, frame_id);

  // Check that the worker is no longer connected to the deleted frame.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [worker_node = GetWorkerNode(shared_worker_instance)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_TRUE(worker_node->client_frames().empty());
      }));

  // The watcher is still expecting a worker removed notification.
  shared_worker_service()->RemoveWorkerClient(shared_worker_instance,
                                              render_process_id, frame_id);
  shared_worker_service()->StopSharedWorker(shared_worker_instance);
}

}  // namespace performance_manager

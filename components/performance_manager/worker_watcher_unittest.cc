// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/worker_watcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/guid.h"
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
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/shared_worker_service.h"
#include "content/public/test/fake_service_worker_context.h"
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

// Helper function to check that |worker_node| and |client_worker_node| are
// correctly hooked up together.
bool IsWorkerClient(WorkerNodeImpl* worker_node,
                    WorkerNodeImpl* client_worker_node) {
  return base::Contains(worker_node->client_workers(), client_worker_node) &&
         base::Contains(client_worker_node->child_workers(), worker_node);
}

// TestDedicatedWorkerService --------------------------------------------------

// A test DedicatedWorkerService that allows to simulate creating and destroying
// dedicated workers.
class TestDedicatedWorkerService : public content::DedicatedWorkerService {
 public:
  TestDedicatedWorkerService();
  ~TestDedicatedWorkerService() override;

  // content::DedicatedWorkerService
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void EnumerateDedicatedWorkers(Observer* observer) override;

  // Creates a new dedicated worker and returns its ID.
  const blink::DedicatedWorkerToken& CreateDedicatedWorker(
      int worker_process_id,
      content::GlobalFrameRoutingId client_render_frame_host_id);

  // Destroys an existing dedicated worker.
  void DestroyDedicatedWorker(const blink::DedicatedWorkerToken& token);

 private:
  base::ObserverList<Observer> observer_list_;

  // Maps each running worker to its client RenderFrameHost ID.
  base::flat_map<blink::DedicatedWorkerToken, content::GlobalFrameRoutingId>
      dedicated_worker_client_frame_;

  DISALLOW_COPY_AND_ASSIGN(TestDedicatedWorkerService);
};

TestDedicatedWorkerService::TestDedicatedWorkerService() = default;

TestDedicatedWorkerService::~TestDedicatedWorkerService() = default;

void TestDedicatedWorkerService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TestDedicatedWorkerService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void TestDedicatedWorkerService::EnumerateDedicatedWorkers(Observer* observer) {
  // Not implemented.
  ADD_FAILURE();
}

const blink::DedicatedWorkerToken&
TestDedicatedWorkerService::CreateDedicatedWorker(
    int worker_process_id,
    content::GlobalFrameRoutingId client_render_frame_host_id) {
  // Create a new token for the worker and add it to the map, along with its
  // client ID.
  const blink::DedicatedWorkerToken token;

  auto result = dedicated_worker_client_frame_.emplace(
      token, client_render_frame_host_id);
  DCHECK(result.second);  // Check inserted.

  // Notify observers.
  for (auto& observer : observer_list_) {
    observer.OnWorkerCreated(token, worker_process_id,
                             client_render_frame_host_id);
  }

  return result.first->first;
}

void TestDedicatedWorkerService::DestroyDedicatedWorker(
    const blink::DedicatedWorkerToken& token) {
  auto it = dedicated_worker_client_frame_.find(token);
  DCHECK(it != dedicated_worker_client_frame_.end());

  // Notify observers that the worker is being destroyed.
  for (auto& observer : observer_list_)
    observer.OnBeforeWorkerDestroyed(token, it->second);

  // Remove the worker ID from the map.
  dedicated_worker_client_frame_.erase(it);
}

// TestSharedWorkerService -----------------------------------------------------

// A test SharedWorkerService that allows to simulate creating and destroying
// shared workers and adding clients to existing workers.
class TestSharedWorkerService : public content::SharedWorkerService {
 public:
  TestSharedWorkerService();
  ~TestSharedWorkerService() override;

  // content::SharedWorkerService
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void EnumerateSharedWorkers(Observer* observer) override;
  bool TerminateWorker(const GURL& url,
                       const std::string& name,
                       const url::Origin& constructor_origin) override;

  // Creates a new shared worker and returns its token.
  blink::SharedWorkerToken CreateSharedWorker(int worker_process_id);

  // Destroys a running shared worker.
  void DestroySharedWorker(const blink::SharedWorkerToken& shared_worker_token);

  // Adds a new frame client to an existing worker.
  void AddClient(const blink::SharedWorkerToken& shared_worker_token,
                 content::GlobalFrameRoutingId client_render_frame_host_id);

  // Removes an existing frame client from a worker.
  void RemoveClient(const blink::SharedWorkerToken& shared_worker_token,
                    content::GlobalFrameRoutingId client_render_frame_host_id);

 private:
  base::ObserverList<Observer> observer_list_;

  // Contains the set of clients for each running workers.
  base::flat_map<blink::SharedWorkerToken,
                 base::flat_set<content::GlobalFrameRoutingId>>
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

void TestSharedWorkerService::EnumerateSharedWorkers(Observer* observer) {
  // Not implemented.
  ADD_FAILURE();
}

bool TestSharedWorkerService::TerminateWorker(
    const GURL& url,
    const std::string& name,
    const url::Origin& constructor_origin) {
  // Not implemented.
  ADD_FAILURE();
  return false;
}

blink::SharedWorkerToken TestSharedWorkerService::CreateSharedWorker(
    int worker_process_id) {
  // Create a new SharedWorkerToken for the worker and add it to the map.
  const blink::SharedWorkerToken shared_worker_token;

  bool inserted =
      shared_worker_client_frames_.insert({shared_worker_token, {}}).second;
  DCHECK(inserted);

  // Notify observers.
  for (auto& observer : observer_list_) {
    observer.OnWorkerCreated(shared_worker_token, worker_process_id,
                             base::UnguessableToken::Create());
  }

  return shared_worker_token;
}

void TestSharedWorkerService::DestroySharedWorker(
    const blink::SharedWorkerToken& shared_worker_token) {
  auto it = shared_worker_client_frames_.find(shared_worker_token);
  DCHECK(it != shared_worker_client_frames_.end());

  // The worker should no longer have any clients.
  DCHECK(it->second.empty());

  // Notify observers that the worker is being destroyed.
  for (auto& observer : observer_list_)
    observer.OnBeforeWorkerDestroyed(shared_worker_token);

  // Remove the worker ID from the map.
  shared_worker_client_frames_.erase(it);
}

void TestSharedWorkerService::AddClient(
    const blink::SharedWorkerToken& shared_worker_token,
    content::GlobalFrameRoutingId client_render_frame_host_id) {
  // Add the frame to the set of clients for this worker.
  auto it = shared_worker_client_frames_.find(shared_worker_token);
  DCHECK(it != shared_worker_client_frames_.end());

  base::flat_set<content::GlobalFrameRoutingId>& client_frames = it->second;
  bool inserted = client_frames.insert(client_render_frame_host_id).second;
  DCHECK(inserted);

  // Then notify observers.
  for (auto& observer : observer_list_)
    observer.OnClientAdded(shared_worker_token, client_render_frame_host_id);
}

void TestSharedWorkerService::RemoveClient(
    const blink::SharedWorkerToken& shared_worker_token,
    content::GlobalFrameRoutingId client_render_frame_host_id) {
  // Notify observers.
  for (auto& observer : observer_list_)
    observer.OnClientRemoved(shared_worker_token, client_render_frame_host_id);

  // Then remove the frame from the set of clients of this worker.
  auto it = shared_worker_client_frames_.find(shared_worker_token);
  DCHECK(it != shared_worker_client_frames_.end());

  base::flat_set<content::GlobalFrameRoutingId>& client_frames = it->second;
  size_t removed = client_frames.erase(client_render_frame_host_id);
  DCHECK_EQ(removed, 1u);
}

// TestServiceWorkerContext ----------------------------------------------------

// A test ServiceWorkerContext that allows to simulate a worker starting and
// stopping and adding clients to running workers.
//
// Extends content::FakeServiceWorkerContext to avoid reimplementing all the
// unused virtual functions.
class TestServiceWorkerContext : public content::FakeServiceWorkerContext {
 public:
  TestServiceWorkerContext();
  ~TestServiceWorkerContext() override;

  TestServiceWorkerContext(const TestServiceWorkerContext&) = delete;
  TestServiceWorkerContext& operator=(const TestServiceWorkerContext&) = delete;

  // content::FakeServiceWorkerContext:
  void AddObserver(content::ServiceWorkerContextObserver* observer) override;
  void RemoveObserver(content::ServiceWorkerContextObserver* observer) override;

  // Creates a new service worker and returns its version ID.
  int64_t CreateServiceWorker();

  // Deletes an existing service worker.
  void DestroyServiceWorker(int64_t version_id);

  // Starts an existing service worker.
  void StartServiceWorker(int64_t version_id, int worker_process_id);

  // Destroys a service shared worker.
  void StopServiceWorker(int64_t version_id);

  // Adds a new client to an existing service worker and returns its generated
  // client UUID.
  std::string AddClient(int64_t version_id,
                        const content::ServiceWorkerClientInfo& client_info);

  // Removes an existing client from a worker.
  void RemoveClient(int64_t version_id, const std::string& client_uuid);

  // Simulates when the navigation commits, meaning that the RenderFrameHost is
  // now available for a window client. Not valid for worker clients.
  void OnControlleeNavigationCommitted(
      int64_t version_id,
      const std::string& client_uuid,
      content::GlobalFrameRoutingId render_frame_host_id);

 private:
  base::ObserverList<content::ServiceWorkerContextObserver>::Unchecked
      observer_list_;

  // The ID that the next service worker will be assigned.
  int64_t next_service_worker_instance_id_ = 0;

  struct ServiceWorkerInfo {
    bool is_running = false;

    // Contains all the clients
    base::flat_set<std::string /*client_uuid*/> clients;
  };

  base::flat_map<int64_t /*version_id*/, ServiceWorkerInfo>
      service_worker_infos_;
};

TestServiceWorkerContext::TestServiceWorkerContext() = default;

TestServiceWorkerContext::~TestServiceWorkerContext() = default;

void TestServiceWorkerContext::AddObserver(
    content::ServiceWorkerContextObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TestServiceWorkerContext::RemoveObserver(
    content::ServiceWorkerContextObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

int64_t TestServiceWorkerContext::CreateServiceWorker() {
  // Create a new version ID and add it to the map.
  int64_t version_id = next_service_worker_instance_id_++;

  bool inserted = service_worker_infos_.insert({version_id, {}}).second;
  DCHECK(inserted);

  return version_id;
}

void TestServiceWorkerContext::DestroyServiceWorker(int64_t version_id) {
  auto it = service_worker_infos_.find(version_id);
  DCHECK(it != service_worker_infos_.end());
  const ServiceWorkerInfo& info = it->second;

  // Can only delete a service worker that isn't running and has no clients.
  DCHECK(!info.is_running);
  DCHECK(info.clients.empty());

  // Remove the worker instance from the map.
  service_worker_infos_.erase(it);
}

void TestServiceWorkerContext::StartServiceWorker(int64_t version_id,
                                                  int worker_process_id) {
  auto it = service_worker_infos_.find(version_id);
  DCHECK(it != service_worker_infos_.end());
  ServiceWorkerInfo& info = it->second;

  DCHECK(!info.is_running);
  info.is_running = true;

  // Notify observers.
  GURL worker_url = GenerateWorkerUrl();
  GURL scope_url;
  for (auto& observer : observer_list_) {
    observer.OnVersionStartedRunning(
        version_id, content::ServiceWorkerRunningInfo(
                        worker_url, scope_url, worker_process_id,
                        blink::ServiceWorkerToken()));
  }
}

void TestServiceWorkerContext::StopServiceWorker(int64_t version_id) {
  auto it = service_worker_infos_.find(version_id);
  DCHECK(it != service_worker_infos_.end());
  ServiceWorkerInfo& info = it->second;

  DCHECK(info.is_running);
  info.is_running = false;

  // Notify observers that the worker is terminating.
  for (auto& observer : observer_list_)
    observer.OnVersionStoppedRunning(version_id);
}

std::string TestServiceWorkerContext::AddClient(
    int64_t version_id,
    const content::ServiceWorkerClientInfo& client_info) {
  auto it = service_worker_infos_.find(version_id);
  DCHECK(it != service_worker_infos_.end());
  ServiceWorkerInfo& info = it->second;

  std::string client_uuid = base::GenerateGUID();

  bool inserted = info.clients.insert(client_uuid).second;
  DCHECK(inserted);

  for (auto& observer : observer_list_)
    observer.OnControlleeAdded(version_id, client_uuid, client_info);

  return client_uuid;
}

void TestServiceWorkerContext::RemoveClient(int64_t version_id,
                                            const std::string& client_uuid) {
  auto it = service_worker_infos_.find(version_id);
  DCHECK(it != service_worker_infos_.end());
  ServiceWorkerInfo& info = it->second;

  size_t removed = info.clients.erase(client_uuid);
  DCHECK_EQ(removed, 1u);

  for (auto& observer : observer_list_)
    observer.OnControlleeRemoved(version_id, client_uuid);
}

void TestServiceWorkerContext::OnControlleeNavigationCommitted(
    int64_t version_id,
    const std::string& client_uuid,
    content::GlobalFrameRoutingId render_frame_host_id) {
  auto it = service_worker_infos_.find(version_id);
  DCHECK(it != service_worker_infos_.end());
  ServiceWorkerInfo& info = it->second;

  DCHECK(base::Contains(info.clients, client_uuid));

  for (auto& observer : observer_list_) {
    observer.OnControlleeNavigationCommitted(version_id, client_uuid,
                                             render_frame_host_id);
  }
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
  PerformanceManagerImpl::BatchDeleteNodes(std::move(nodes));
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
  auto process_node = PerformanceManagerImpl::CreateProcessNode(
      content::PROCESS_TYPE_RENDERER, RenderProcessHostProxy());
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
  FrameNodeImpl* GetFrameNode(
      content::GlobalFrameRoutingId render_frame_host_id) override;
  void SubscribeToFrameNode(content::GlobalFrameRoutingId render_frame_host_id,
                            OnbeforeFrameNodeRemovedCallback
                                on_before_frame_node_removed_callback) override;
  void UnsubscribeFromFrameNode(
      content::GlobalFrameRoutingId render_frame_host_id) override;

  // Creates a frame node and returns its generated render frame host id.
  content::GlobalFrameRoutingId CreateFrameNode(int render_process_id,
                                                ProcessNodeImpl* process_node);

  // Deletes an existing frame node and notify subscribers.
  void DeleteFrameNode(content::GlobalFrameRoutingId render_frame_host_id);

 private:
  // Helper function that invokes the OnBeforeFrameNodeRemovedCallback
  // associated with |frame_node| and removes it from the map.
  void InvokeAndRemoveCallback(FrameNodeImpl* frame_node);

  // The page node that hosts all frames.
  std::unique_ptr<PageNodeImpl> page_node_;

  // Maps each frame's render frame host id with their associated frame node.
  base::flat_map<content::GlobalFrameRoutingId, std::unique_ptr<FrameNodeImpl>>
      frame_node_map_;

  // Maps each observed frame node to their callback.
  base::flat_map<FrameNodeImpl*, OnbeforeFrameNodeRemovedCallback>
      frame_node_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(TestFrameNodeSource);
};

TestFrameNodeSource::TestFrameNodeSource()
    : page_node_(
          PerformanceManagerImpl::CreatePageNode(WebContentsProxy(),
                                                 "page_node_context_id",
                                                 GURL(),
                                                 false,
                                                 false,
                                                 base::TimeTicks::Now())) {}

TestFrameNodeSource::~TestFrameNodeSource() {
  std::vector<std::unique_ptr<NodeBase>> nodes;
  nodes.push_back(std::move(page_node_));
  nodes.reserve(frame_node_map_.size());
  for (auto& kv : frame_node_map_)
    nodes.push_back(std::move(kv.second));
  PerformanceManagerImpl::BatchDeleteNodes(std::move(nodes));
  frame_node_map_.clear();
}

FrameNodeImpl* TestFrameNodeSource::GetFrameNode(
    content::GlobalFrameRoutingId render_frame_host_id) {
  auto it = frame_node_map_.find(render_frame_host_id);
  return it != frame_node_map_.end() ? it->second.get() : nullptr;
}

void TestFrameNodeSource::SubscribeToFrameNode(
    content::GlobalFrameRoutingId render_frame_host_id,
    OnbeforeFrameNodeRemovedCallback on_before_frame_node_removed_callback) {
  FrameNodeImpl* frame_node = GetFrameNode(render_frame_host_id);
  DCHECK(frame_node);

  bool inserted =
      frame_node_callbacks_
          .emplace(frame_node, std::move(on_before_frame_node_removed_callback))
          .second;
  DCHECK(inserted);
}

void TestFrameNodeSource::UnsubscribeFromFrameNode(
    content::GlobalFrameRoutingId render_frame_host_id) {
  FrameNodeImpl* frame_node = GetFrameNode(render_frame_host_id);
  DCHECK(frame_node);

  size_t removed = frame_node_callbacks_.erase(frame_node);
  DCHECK_EQ(removed, 1u);
}

content::GlobalFrameRoutingId TestFrameNodeSource::CreateFrameNode(
    int render_process_id,
    ProcessNodeImpl* process_node) {
  int frame_id = GenerateNextId();
  content::GlobalFrameRoutingId render_frame_host_id(render_process_id,
                                                     frame_id);
  auto frame_node = PerformanceManagerImpl::CreateFrameNode(
      process_node, page_node_.get(), nullptr, 0, frame_id,
      blink::LocalFrameToken(), 0, 0);

  bool inserted =
      frame_node_map_.insert({render_frame_host_id, std::move(frame_node)})
          .second;
  DCHECK(inserted);

  return render_frame_host_id;
}

void TestFrameNodeSource::DeleteFrameNode(
    content::GlobalFrameRoutingId render_frame_host_id) {
  auto it = frame_node_map_.find(render_frame_host_id);
  DCHECK(it != frame_node_map_.end());

  FrameNodeImpl* frame_node = it->second.get();

  // Notify the subscriber then delete the node.
  InvokeAndRemoveCallback(frame_node);
  PerformanceManagerImpl::DeleteNode(std::move(it->second));

  frame_node_map_.erase(it);
}

void TestFrameNodeSource::InvokeAndRemoveCallback(FrameNodeImpl* frame_node) {
  auto it = frame_node_callbacks_.find(frame_node);
  DCHECK(it != frame_node_callbacks_.end());

  std::move(it->second).Run(frame_node);

  frame_node_callbacks_.erase(it);
}

}  // namespace

class WorkerWatcherTest : public testing::Test {
 public:
  WorkerWatcherTest();
  ~WorkerWatcherTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Wraps a |graph_callback| and ensures the task completes before returning.
  void CallOnGraphAndWait(
      PerformanceManagerImpl::GraphImplCallback graph_callback);

  // Retrieves an existing worker node.
  WorkerNodeImpl* GetDedicatedWorkerNode(
      const blink::DedicatedWorkerToken& token);
  WorkerNodeImpl* GetSharedWorkerNode(
      const blink::SharedWorkerToken& shared_worker_token);
  WorkerNodeImpl* GetServiceWorkerNode(int64_t version_id);

  TestDedicatedWorkerService* dedicated_worker_service() {
    return &dedicated_worker_service_;
  }

  TestSharedWorkerService* shared_worker_service() {
    return &shared_worker_service_;
  }

  TestServiceWorkerContext* service_worker_context() {
    return &service_worker_context_;
  }

  TestProcessNodeSource* process_node_source() {
    return process_node_source_.get();
  }

  TestFrameNodeSource* frame_node_source() { return frame_node_source_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;

  TestDedicatedWorkerService dedicated_worker_service_;
  TestSharedWorkerService shared_worker_service_;
  TestServiceWorkerContext service_worker_context_;

  std::unique_ptr<PerformanceManagerImpl> performance_manager_;
  std::unique_ptr<TestProcessNodeSource> process_node_source_;
  std::unique_ptr<TestFrameNodeSource> frame_node_source_;

  // The WorkerWatcher that's being tested.
  std::unique_ptr<WorkerWatcher> worker_watcher_;

  DISALLOW_COPY_AND_ASSIGN(WorkerWatcherTest);
};

WorkerWatcherTest::WorkerWatcherTest() = default;

WorkerWatcherTest::~WorkerWatcherTest() = default;

void WorkerWatcherTest::SetUp() {
  performance_manager_ = PerformanceManagerImpl::Create(base::DoNothing());

  process_node_source_ = std::make_unique<TestProcessNodeSource>();
  frame_node_source_ = std::make_unique<TestFrameNodeSource>();

  worker_watcher_ = std::make_unique<WorkerWatcher>(
      "browser_context_id", &dedicated_worker_service_, &shared_worker_service_,
      &service_worker_context_, process_node_source_.get(),
      frame_node_source_.get());
}

void WorkerWatcherTest::TearDown() {
  // Clean up the performance manager correctly.
  worker_watcher_->TearDown();
  worker_watcher_ = nullptr;

  // Delete the TestFrameNodeSource and the TestProcessNodeSource in
  // that order since they own graph nodes.
  frame_node_source_ = nullptr;
  process_node_source_ = nullptr;
  PerformanceManagerImpl::Destroy(std::move(performance_manager_));
}

void WorkerWatcherTest::CallOnGraphAndWait(
    PerformanceManagerImpl::GraphImplCallback graph_callback) {
  base::RunLoop run_loop;
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindLambdaForTesting(
          [graph_callback = std::move(graph_callback),
           quit_closure = run_loop.QuitClosure()](GraphImpl* graph) mutable {
            std::move(graph_callback).Run(graph);
            quit_closure.Run();
          }));
  run_loop.Run();
}

WorkerNodeImpl* WorkerWatcherTest::GetDedicatedWorkerNode(
    const blink::DedicatedWorkerToken& token) {
  return worker_watcher_->GetDedicatedWorkerNode(token);
}

WorkerNodeImpl* WorkerWatcherTest::GetSharedWorkerNode(
    const blink::SharedWorkerToken& shared_worker_token) {
  return worker_watcher_->GetSharedWorkerNode(shared_worker_token);
}

WorkerNodeImpl* WorkerWatcherTest::GetServiceWorkerNode(int64_t version_id) {
  return worker_watcher_->GetServiceWorkerNode(version_id);
}

// This test creates one dedicated worker.
TEST_F(WorkerWatcherTest, SimpleDedicatedWorker) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the frame node.
  content::GlobalFrameRoutingId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));

  // Create the worker.
  const blink::DedicatedWorkerToken token =
      dedicated_worker_service()->CreateDedicatedWorker(render_process_id,
                                                        render_frame_host_id);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       worker_node = GetDedicatedWorkerNode(token),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->worker_type(),
                  WorkerNode::WorkerType::kDedicated);
        EXPECT_EQ(worker_node->process_node(), process_node);
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));
      }));

  // Disconnect and clean up the dedicated worker.
  dedicated_worker_service()->DestroyDedicatedWorker(token);
}

// This test creates one shared worker with one client frame.
TEST_F(WorkerWatcherTest, SimpleSharedWorker) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the frame node.
  content::GlobalFrameRoutingId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));

  // Create the worker.
  const blink::SharedWorkerToken shared_worker_token =
      shared_worker_service()->CreateSharedWorker(render_process_id);

  // Connect the frame to the worker.
  shared_worker_service()->AddClient(shared_worker_token, render_frame_host_id);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       worker_node = GetSharedWorkerNode(shared_worker_token),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->worker_type(), WorkerNode::WorkerType::kShared);
        EXPECT_EQ(worker_node->process_node(), process_node);
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));
      }));

  // Disconnect and clean up the shared worker.
  shared_worker_service()->RemoveClient(shared_worker_token,
                                        render_frame_host_id);
  shared_worker_service()->DestroySharedWorker(shared_worker_token);
}

// This test creates one service worker with one client frame.
//
// TODO(pmonette): Enable this test when the WorkerWatcher starts tracking
// service worker clients.
TEST_F(WorkerWatcherTest, DISABLED_ServiceWorkerFrameClient) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create and start the service worker.
  int64_t service_worker_version_id =
      service_worker_context()->CreateServiceWorker();
  service_worker_context()->StartServiceWorker(service_worker_version_id,
                                               render_process_id);

  // Add a frame tree node as a client of the service worker.
  int frame_tree_node_id = GenerateNextId();
  std::string service_worker_client_uuid = service_worker_context()->AddClient(
      service_worker_version_id,
      content::ServiceWorkerClientInfo(frame_tree_node_id));

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       worker_node =
           GetServiceWorkerNode(service_worker_version_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->worker_type(), WorkerNode::WorkerType::kService);
        EXPECT_EQ(worker_node->process_node(), process_node);

        // The frame can not be connected to the service worker until its
        // render frame host is available, which happens when the navigation
        // commits.
        EXPECT_TRUE(worker_node->client_frames().empty());
      }));

  // Now simulate the navigation commit.
  content::GlobalFrameRoutingId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));
  service_worker_context()->OnControlleeNavigationCommitted(
      service_worker_version_id, service_worker_client_uuid,
      render_frame_host_id);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       worker_node = GetServiceWorkerNode(service_worker_version_id),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->worker_type(), WorkerNode::WorkerType::kService);
        EXPECT_EQ(worker_node->process_node(), process_node);

        // Now is it correctly hooked up.
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));
      }));

  // Disconnect and clean up the service worker.
  service_worker_context()->RemoveClient(service_worker_version_id,
                                         service_worker_client_uuid);
  service_worker_context()->StopServiceWorker(service_worker_version_id);
  service_worker_context()->DestroyServiceWorker(service_worker_version_id);
}

// Ensures that the WorkerWatcher handles the case where a frame with a service
// worker is created but it's navigation is never committed before the
// FrameTreeNode is destroyed.
TEST_F(WorkerWatcherTest, ServiceWorkerFrameClientDestroyedBeforeCommit) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create and start the service worker.
  int64_t service_worker_version_id =
      service_worker_context()->CreateServiceWorker();
  service_worker_context()->StartServiceWorker(service_worker_version_id,
                                               render_process_id);

  // Add a frame tree node as a client of the service worker.
  int frame_tree_node_id = GenerateNextId();
  std::string service_worker_client_uuid = service_worker_context()->AddClient(
      service_worker_version_id,
      content::ServiceWorkerClientInfo(frame_tree_node_id));

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       worker_node =
           GetServiceWorkerNode(service_worker_version_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->worker_type(), WorkerNode::WorkerType::kService);
        EXPECT_EQ(worker_node->process_node(), process_node);

        // The frame was never added as a client of the service worker.
        EXPECT_TRUE(worker_node->client_frames().empty());
      }));

  // Disconnect and clean up the service worker.
  service_worker_context()->RemoveClient(service_worker_version_id,
                                         service_worker_client_uuid);
  service_worker_context()->StopServiceWorker(service_worker_version_id);
  service_worker_context()->DestroyServiceWorker(service_worker_version_id);
}

// TODO(pmonette): Enable this test when the WorkerWatcher starts tracking
// service worker clients.
TEST_F(WorkerWatcherTest, DISABLED_AllTypesOfServiceWorkerClients) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create and start the service worker.
  int64_t service_worker_version_id =
      service_worker_context()->CreateServiceWorker();
  service_worker_context()->StartServiceWorker(service_worker_version_id,
                                               render_process_id);

  // Create a client of each type and connect them to the service worker.

  // Frame client.
  int frame_tree_node_id = GenerateNextId();
  std::string frame_client_uuid = service_worker_context()->AddClient(
      service_worker_version_id,
      content::ServiceWorkerClientInfo(frame_tree_node_id));
  content::GlobalFrameRoutingId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));
  service_worker_context()->OnControlleeNavigationCommitted(
      service_worker_version_id, frame_client_uuid, render_frame_host_id);

  // Dedicated worker client.
  blink::DedicatedWorkerToken dedicated_worker_token =
      dedicated_worker_service()->CreateDedicatedWorker(render_process_id,
                                                        render_frame_host_id);
  std::string dedicated_worker_client_uuid =
      service_worker_context()->AddClient(
          service_worker_version_id,
          content::ServiceWorkerClientInfo(dedicated_worker_token));

  // Shared worker client.
  blink::SharedWorkerToken shared_worker_token =
      shared_worker_service()->CreateSharedWorker(render_process_id);
  std::string shared_worker_client_uuid = service_worker_context()->AddClient(
      service_worker_version_id,
      content::ServiceWorkerClientInfo(shared_worker_token));

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       service_worker_node = GetServiceWorkerNode(service_worker_version_id),
       client_frame_node =
           frame_node_source()->GetFrameNode(render_frame_host_id),
       dedicated_worker_node = GetDedicatedWorkerNode(dedicated_worker_token),
       shared_worker_node =
           GetSharedWorkerNode(shared_worker_token)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(service_worker_node));
        EXPECT_TRUE(IsWorkerClient(service_worker_node, client_frame_node));
        EXPECT_TRUE(IsWorkerClient(service_worker_node, dedicated_worker_node));
        EXPECT_TRUE(IsWorkerClient(service_worker_node, shared_worker_node));
      }));

  // Disconnect and clean up the service worker and its clients.
  service_worker_context()->RemoveClient(service_worker_version_id,
                                         shared_worker_client_uuid);
  shared_worker_service()->DestroySharedWorker(shared_worker_token);
  service_worker_context()->RemoveClient(service_worker_version_id,
                                         dedicated_worker_client_uuid);
  dedicated_worker_service()->DestroyDedicatedWorker(dedicated_worker_token);
  service_worker_context()->RemoveClient(service_worker_version_id,
                                         frame_client_uuid);

  service_worker_context()->StopServiceWorker(service_worker_version_id);
  service_worker_context()->DestroyServiceWorker(service_worker_version_id);
}

// Tests that the WorkerWatcher can handle the case where the service worker
// starts after it has been assigned a client. In this case, the clients are not
// connected to the service worker until it starts. It also tests that when the
// service worker stops, its existing clients are also disconnected.
//
// TODO(pmonette): Enable this test when the WorkerWatcher starts tracking
// service worker clients.
TEST_F(WorkerWatcherTest,
       DISABLED_ServiceWorkerStartsAndStopsWithExistingClients) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the worker.
  int64_t service_worker_version_id =
      service_worker_context()->CreateServiceWorker();

  // Create a client of each type and connect them to the service worker.

  // Frame client.
  int frame_tree_node_id = GenerateNextId();
  std::string frame_client_uuid = service_worker_context()->AddClient(
      service_worker_version_id,
      content::ServiceWorkerClientInfo(frame_tree_node_id));
  content::GlobalFrameRoutingId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));
  service_worker_context()->OnControlleeNavigationCommitted(
      service_worker_version_id, frame_client_uuid, render_frame_host_id);

  // Dedicated worker client.
  blink::DedicatedWorkerToken dedicated_worker_token =
      dedicated_worker_service()->CreateDedicatedWorker(render_process_id,
                                                        render_frame_host_id);
  std::string dedicated_worker_client_uuid =
      service_worker_context()->AddClient(
          service_worker_version_id,
          content::ServiceWorkerClientInfo(dedicated_worker_token));

  // Shared worker client.
  blink::SharedWorkerToken shared_worker_token =
      shared_worker_service()->CreateSharedWorker(render_process_id);
  std::string shared_worker_client_uuid = service_worker_context()->AddClient(
      service_worker_version_id,
      content::ServiceWorkerClientInfo(shared_worker_token));

  // The service worker node doesn't even exist yet.
  EXPECT_FALSE(GetServiceWorkerNode(service_worker_version_id));

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       frame_node = frame_node_source()->GetFrameNode(render_frame_host_id),
       dedicated_worker_node = GetDedicatedWorkerNode(dedicated_worker_token),
       shared_worker_node =
           GetSharedWorkerNode(shared_worker_token)](GraphImpl* graph) {
        // The clients exists in the graph but they are not connected to the
        // service worker.
        EXPECT_TRUE(graph->NodeInGraph(frame_node));
        EXPECT_TRUE(graph->NodeInGraph(dedicated_worker_node));
        EXPECT_TRUE(graph->NodeInGraph(shared_worker_node));

        // Note: Because a dedicated worker is always connected to a frame, this
        // frame node actually has |dedicated_worker_node| as its sole client.
        ASSERT_EQ(frame_node->child_worker_nodes().size(), 1u);
        EXPECT_TRUE(base::Contains(frame_node->child_worker_nodes(),
                                   dedicated_worker_node));
        EXPECT_TRUE(dedicated_worker_node->child_workers().empty());
        EXPECT_TRUE(shared_worker_node->child_workers().empty());
      }));

  // Now start the service worker.
  service_worker_context()->StartServiceWorker(service_worker_version_id,
                                               render_process_id);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       service_worker_node = GetServiceWorkerNode(service_worker_version_id),
       frame_node = frame_node_source()->GetFrameNode(render_frame_host_id),
       dedicated_worker_node = GetDedicatedWorkerNode(dedicated_worker_token),
       shared_worker_node =
           GetSharedWorkerNode(shared_worker_token)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(service_worker_node));
        EXPECT_EQ(service_worker_node->worker_type(),
                  WorkerNode::WorkerType::kService);
        EXPECT_EQ(service_worker_node->process_node(), process_node);

        EXPECT_TRUE(graph->NodeInGraph(frame_node));
        EXPECT_TRUE(graph->NodeInGraph(dedicated_worker_node));
        EXPECT_TRUE(graph->NodeInGraph(shared_worker_node));

        // Now is it correctly hooked up.
        EXPECT_TRUE(IsWorkerClient(service_worker_node, frame_node));
        EXPECT_TRUE(IsWorkerClient(service_worker_node, dedicated_worker_node));
        EXPECT_TRUE(IsWorkerClient(service_worker_node, shared_worker_node));
      }));

  // Stop the service worker. All the clients will be disconnected.
  service_worker_context()->StopServiceWorker(service_worker_version_id);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       frame_node = frame_node_source()->GetFrameNode(render_frame_host_id),
       dedicated_worker_node = GetDedicatedWorkerNode(dedicated_worker_token),
       shared_worker_node =
           GetSharedWorkerNode(shared_worker_token)](GraphImpl* graph) {
        // The clients exists in the graph but they are not connected to the
        // service worker.
        EXPECT_TRUE(graph->NodeInGraph(frame_node));
        EXPECT_TRUE(graph->NodeInGraph(dedicated_worker_node));
        EXPECT_TRUE(graph->NodeInGraph(shared_worker_node));

        // Note: Because a dedicated worker is always connected to a frame, this
        // frame node actually has |dedicated_worker_node| as its sole client.
        ASSERT_EQ(frame_node->child_worker_nodes().size(), 1u);
        EXPECT_TRUE(base::Contains(frame_node->child_worker_nodes(),
                                   dedicated_worker_node));
        EXPECT_TRUE(dedicated_worker_node->child_workers().empty());
        EXPECT_TRUE(shared_worker_node->child_workers().empty());
      }));

  // Disconnect and clean up the service worker and its clients
  service_worker_context()->RemoveClient(service_worker_version_id,
                                         shared_worker_client_uuid);
  shared_worker_service()->DestroySharedWorker(shared_worker_token);
  service_worker_context()->RemoveClient(service_worker_version_id,
                                         dedicated_worker_client_uuid);
  dedicated_worker_service()->DestroyDedicatedWorker(dedicated_worker_token);
  service_worker_context()->RemoveClient(service_worker_version_id,
                                         frame_client_uuid);

  service_worker_context()->DestroyServiceWorker(service_worker_version_id);
}

TEST_F(WorkerWatcherTest, SharedWorkerCrossProcessClient) {
  // Create the frame node.
  int frame_process_id = process_node_source()->CreateProcessNode();
  content::GlobalFrameRoutingId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          frame_process_id,
          process_node_source()->GetProcessNode(frame_process_id));

  // Create the worker in a different process.
  int worker_process_id = process_node_source()->CreateProcessNode();
  const blink::SharedWorkerToken& shared_worker_token =
      shared_worker_service()->CreateSharedWorker(worker_process_id);

  // Connect the frame to the worker.
  shared_worker_service()->AddClient(shared_worker_token, render_frame_host_id);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [worker_process_node =
           process_node_source()->GetProcessNode(worker_process_id),
       worker_node = GetSharedWorkerNode(shared_worker_token),
       client_process_node =
           process_node_source()->GetProcessNode(frame_process_id),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->worker_type(), WorkerNode::WorkerType::kShared);
        EXPECT_EQ(worker_node->process_node(), worker_process_node);
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));
      }));

  // Disconnect and clean up the shared worker.
  shared_worker_service()->RemoveClient(shared_worker_token,
                                        render_frame_host_id);
  shared_worker_service()->DestroySharedWorker(shared_worker_token);
}

TEST_F(WorkerWatcherTest, OneSharedWorkerTwoClients) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the worker.
  const blink::SharedWorkerToken& shared_worker_token =
      shared_worker_service()->CreateSharedWorker(render_process_id);

  // Create 2 client frame nodes and connect them to the worker.
  content::GlobalFrameRoutingId render_frame_host_id_1 =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));
  shared_worker_service()->AddClient(shared_worker_token,
                                     render_frame_host_id_1);

  content::GlobalFrameRoutingId render_frame_host_id_2 =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));
  shared_worker_service()->AddClient(shared_worker_token,
                                     render_frame_host_id_2);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [worker_node = GetSharedWorkerNode(shared_worker_token),
       client_frame_node_1 =
           frame_node_source()->GetFrameNode(render_frame_host_id_1),
       client_frame_node_2 = frame_node_source()->GetFrameNode(
           render_frame_host_id_2)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->worker_type(), WorkerNode::WorkerType::kShared);

        // Check frame 1.
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node_1));

        // Check frame 2.
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node_2));
      }));

  // Disconnect and clean up the shared worker.
  shared_worker_service()->RemoveClient(shared_worker_token,
                                        render_frame_host_id_1);
  shared_worker_service()->RemoveClient(shared_worker_token,
                                        render_frame_host_id_2);
  shared_worker_service()->DestroySharedWorker(shared_worker_token);
}

TEST_F(WorkerWatcherTest, OneClientTwoSharedWorkers) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the frame node.
  content::GlobalFrameRoutingId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));

  // Create the 2 workers and connect them to the frame.
  const blink::SharedWorkerToken& shared_worker_token_1 =
      shared_worker_service()->CreateSharedWorker(render_process_id);
  shared_worker_service()->AddClient(shared_worker_token_1,
                                     render_frame_host_id);

  const blink::SharedWorkerToken& shared_worker_token_2 =
      shared_worker_service()->CreateSharedWorker(render_process_id);
  shared_worker_service()->AddClient(shared_worker_token_2,
                                     render_frame_host_id);

  // Check expectations on the graph.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [worker_node_1 = GetSharedWorkerNode(shared_worker_token_1),
       worker_node_2 = GetSharedWorkerNode(shared_worker_token_2),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
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

  // Disconnect and clean up the shared workers.
  shared_worker_service()->RemoveClient(shared_worker_token_1,
                                        render_frame_host_id);
  shared_worker_service()->DestroySharedWorker(shared_worker_token_1);

  shared_worker_service()->RemoveClient(shared_worker_token_2,
                                        render_frame_host_id);
  shared_worker_service()->DestroySharedWorker(shared_worker_token_2);
}

TEST_F(WorkerWatcherTest, FrameDestroyed) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the frame node.
  int frame_tree_node_id = GenerateNextId();
  content::GlobalFrameRoutingId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));

  // Create a worker of each type.
  const blink::DedicatedWorkerToken& dedicated_worker_token =
      dedicated_worker_service()->CreateDedicatedWorker(render_process_id,
                                                        render_frame_host_id);
  const blink::SharedWorkerToken& shared_worker_token =
      shared_worker_service()->CreateSharedWorker(render_process_id);
  int64_t service_worker_version_id =
      service_worker_context()->CreateServiceWorker();
  service_worker_context()->StartServiceWorker(service_worker_version_id,
                                               render_process_id);

  // Connect the frame to the shared worker and the service worker. Note that it
  // is already connected to the dedicated worker.
  shared_worker_service()->AddClient(shared_worker_token, render_frame_host_id);
  std::string service_worker_client_uuid = service_worker_context()->AddClient(
      service_worker_version_id,
      content::ServiceWorkerClientInfo(frame_tree_node_id));
  service_worker_context()->OnControlleeNavigationCommitted(
      service_worker_version_id, service_worker_client_uuid,
      render_frame_host_id);

  // Check that everything is wired up correctly.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [dedicated_worker_node = GetDedicatedWorkerNode(dedicated_worker_token),
       shared_worker_node = GetSharedWorkerNode(shared_worker_token),
       service_worker_node = GetServiceWorkerNode(service_worker_version_id),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(dedicated_worker_node));
        EXPECT_TRUE(graph->NodeInGraph(shared_worker_node));
        EXPECT_TRUE(graph->NodeInGraph(service_worker_node));
        EXPECT_TRUE(IsWorkerClient(dedicated_worker_node, client_frame_node));
        EXPECT_TRUE(IsWorkerClient(shared_worker_node, client_frame_node));
        // TODO(pmonette): Change this to EXPECT_TRUE() when the WorkerWatcher
        // starts tracking service worker clients.
        EXPECT_FALSE(IsWorkerClient(service_worker_node, client_frame_node));
      }));

  frame_node_source()->DeleteFrameNode(render_frame_host_id);

  // Check that the workers are no longer connected to the deleted frame.
  CallOnGraphAndWait(base::BindLambdaForTesting(
      [dedicated_worker_node = GetDedicatedWorkerNode(dedicated_worker_token),
       shared_worker_node = GetSharedWorkerNode(shared_worker_token),
       service_worker_node =
           GetServiceWorkerNode(service_worker_version_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(dedicated_worker_node));
        EXPECT_TRUE(graph->NodeInGraph(shared_worker_node));
        EXPECT_TRUE(graph->NodeInGraph(service_worker_node));
        EXPECT_TRUE(dedicated_worker_node->client_frames().empty());
        EXPECT_TRUE(shared_worker_node->client_frames().empty());
        EXPECT_TRUE(service_worker_node->client_frames().empty());
      }));

  // Clean up. The watcher is still expecting a worker removed notification.
  service_worker_context()->RemoveClient(service_worker_version_id,
                                         service_worker_client_uuid);
  service_worker_context()->StopServiceWorker(service_worker_version_id);
  service_worker_context()->DestroyServiceWorker(service_worker_version_id);
  shared_worker_service()->RemoveClient(shared_worker_token,
                                        render_frame_host_id);
  shared_worker_service()->DestroySharedWorker(shared_worker_token);
  dedicated_worker_service()->DestroyDedicatedWorker(dedicated_worker_token);
}

}  // namespace performance_manager

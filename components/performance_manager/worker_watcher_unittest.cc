// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/worker_watcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "components/performance_manager/frame_node_source.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/process_node_source.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/dedicated_worker_creator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_running_info.h"
#include "content/public/browser/shared_worker_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/fake_service_worker_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager {

namespace {

// Generates a new sequential int ID. Used for things that need a unique ID
// and don't have a more specific generator.
int GenerateNextId() {
  static int next_id = 0;
  return next_id++;
}

// Generates a unique URL for a fake worker node.
GURL GenerateWorkerUrl() {
  return GURL(base::StringPrintf("https://www.foo.org/worker_script_%d.js",
                                 GenerateNextId()));
}

// Generates a URL in a unique domain, which can be used to create identifiable
// origins or scope URL's for service workers.
GURL GenerateUniqueDomainUrl() {
  return GURL(base::StringPrintf("https://www.foo%d.org", GenerateNextId()));
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

  TestDedicatedWorkerService(const TestDedicatedWorkerService&) = delete;
  TestDedicatedWorkerService& operator=(const TestDedicatedWorkerService&) =
      delete;

  ~TestDedicatedWorkerService() override;

  // content::DedicatedWorkerService
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void EnumerateDedicatedWorkers(Observer* observer) override;

  // Creates a new dedicated worker and returns its ID.
  const blink::DedicatedWorkerToken& CreateDedicatedWorker(
      int worker_process_id,
      content::DedicatedWorkerCreator creator,
      const url::Origin& origin = url::Origin());

  // Destroys an existing dedicated worker.
  void DestroyDedicatedWorker(const blink::DedicatedWorkerToken& token);

 private:
  base::ObserverList<Observer> observer_list_;

  // Maps each running worker to its creator.
  base::flat_map<blink::DedicatedWorkerToken, content::DedicatedWorkerCreator>
      dedicated_worker_creators_;
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
    content::DedicatedWorkerCreator creator,
    const url::Origin& origin) {
  // Create a new token for the worker and add it to the map, along with its
  // client ID.
  const blink::DedicatedWorkerToken token;

  auto result = dedicated_worker_creators_.emplace(token, creator);
  DCHECK(result.second);  // Check inserted.

  // Notify observers.
  for (auto& observer : observer_list_) {
    observer.OnWorkerCreated(token, worker_process_id, origin, creator);
  }

  return result.first->first;
}

void TestDedicatedWorkerService::DestroyDedicatedWorker(
    const blink::DedicatedWorkerToken& token) {
  auto it = dedicated_worker_creators_.find(token);
  CHECK(it != dedicated_worker_creators_.end(), base::NotFatalUntil::M130);

  // Notify observers that the worker is being destroyed.
  for (auto& observer : observer_list_)
    observer.OnBeforeWorkerDestroyed(token, it->second);

  // Remove the worker ID from the map.
  dedicated_worker_creators_.erase(it);
}

// TestSharedWorkerService -----------------------------------------------------

// A test SharedWorkerService that allows to simulate creating and destroying
// shared workers and adding clients to existing workers.
class TestSharedWorkerService : public content::SharedWorkerService {
 public:
  TestSharedWorkerService();

  TestSharedWorkerService(const TestSharedWorkerService&) = delete;
  TestSharedWorkerService& operator=(const TestSharedWorkerService&) = delete;

  ~TestSharedWorkerService() override;

  // content::SharedWorkerService
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void EnumerateSharedWorkers(Observer* observer) override;
  bool TerminateWorker(const GURL& url,
                       const std::string& name,
                       const blink::StorageKey& storage_key,
                       const blink::mojom::SharedWorkerSameSiteCookies
                           same_site_cookies) override;
  void Shutdown() override;

  // Creates a new shared worker and returns its token.
  blink::SharedWorkerToken CreateSharedWorker(
      int worker_process_id,
      const url::Origin& origin = url::Origin());

  // Destroys a running shared worker.
  void DestroySharedWorker(const blink::SharedWorkerToken& shared_worker_token);

  // Adds a new frame client to an existing worker.
  void AddClient(const blink::SharedWorkerToken& shared_worker_token,
                 content::GlobalRenderFrameHostId client_render_frame_host_id);

  // Removes an existing frame client from a worker.
  void RemoveClient(
      const blink::SharedWorkerToken& shared_worker_token,
      content::GlobalRenderFrameHostId client_render_frame_host_id);

 private:
  base::ObserverList<Observer> observer_list_;

  // Contains the set of clients for each running workers.
  base::flat_map<blink::SharedWorkerToken,
                 base::flat_set<content::GlobalRenderFrameHostId>>
      shared_worker_client_frames_;
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
    const blink::StorageKey& storage_key,
    const blink::mojom::SharedWorkerSameSiteCookies same_site_cookies) {
  // Not implemented.
  ADD_FAILURE();
  return false;
}

void TestSharedWorkerService::Shutdown() {
  // Not implemented.
  ADD_FAILURE();
}

blink::SharedWorkerToken TestSharedWorkerService::CreateSharedWorker(
    int worker_process_id,
    const url::Origin& origin) {
  // Create a new SharedWorkerToken for the worker and add it to the map.
  const blink::SharedWorkerToken shared_worker_token;

  bool inserted =
      shared_worker_client_frames_.insert({shared_worker_token, {}}).second;
  DCHECK(inserted);

  // Notify observers.
  for (auto& observer : observer_list_) {
    observer.OnWorkerCreated(shared_worker_token, worker_process_id, origin,
                             base::UnguessableToken::Create());
  }

  return shared_worker_token;
}

void TestSharedWorkerService::DestroySharedWorker(
    const blink::SharedWorkerToken& shared_worker_token) {
  auto it = shared_worker_client_frames_.find(shared_worker_token);
  CHECK(it != shared_worker_client_frames_.end(), base::NotFatalUntil::M130);

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
    content::GlobalRenderFrameHostId client_render_frame_host_id) {
  // Add the frame to the set of clients for this worker.
  auto it = shared_worker_client_frames_.find(shared_worker_token);
  CHECK(it != shared_worker_client_frames_.end(), base::NotFatalUntil::M130);

  base::flat_set<content::GlobalRenderFrameHostId>& client_frames = it->second;
  bool inserted = client_frames.insert(client_render_frame_host_id).second;
  DCHECK(inserted);

  // Then notify observers.
  for (auto& observer : observer_list_)
    observer.OnClientAdded(shared_worker_token, client_render_frame_host_id);
}

void TestSharedWorkerService::RemoveClient(
    const blink::SharedWorkerToken& shared_worker_token,
    content::GlobalRenderFrameHostId client_render_frame_host_id) {
  // Notify observers.
  for (auto& observer : observer_list_)
    observer.OnClientRemoved(shared_worker_token, client_render_frame_host_id);

  // Then remove the frame from the set of clients of this worker.
  auto it = shared_worker_client_frames_.find(shared_worker_token);
  CHECK(it != shared_worker_client_frames_.end(), base::NotFatalUntil::M130);

  base::flat_set<content::GlobalRenderFrameHostId>& client_frames = it->second;
  size_t removed = client_frames.erase(client_render_frame_host_id);
  DCHECK_EQ(removed, 1u);
}

// TestServiceWorkerContextAdapter ---------------------------------------------

// A test ServiceWorkerContext that allows to simulate a worker starting and
// stopping and adding clients to running workers.
//
// Extends content::FakeServiceWorkerContext to avoid reimplementing all the
// unused virtual functions.
class TestServiceWorkerContextAdapter : public ServiceWorkerContextAdapter {
 public:
  TestServiceWorkerContextAdapter();
  ~TestServiceWorkerContextAdapter() override;

  TestServiceWorkerContextAdapter(const TestServiceWorkerContextAdapter&) =
      delete;
  TestServiceWorkerContextAdapter& operator=(
      const TestServiceWorkerContextAdapter&) = delete;

  // ServiceWorkerContextAdapter:
  void AddObserver(content::ServiceWorkerContextObserver* observer) override;
  void RemoveObserver(content::ServiceWorkerContextObserver* observer) override;

  // Creates a new service worker and returns its version ID.
  int64_t CreateServiceWorker();

  // Deletes an existing service worker.
  void DestroyServiceWorker(int64_t version_id);

  // Starts an existing service worker.
  void StartServiceWorker(int64_t version_id,
                          int worker_process_id,
                          const GURL& worker_url = GenerateWorkerUrl(),
                          const GURL& scope_url = GURL());

  // Stops a service shared worker.
  void StopServiceWorker(int64_t version_id);

  // Adds a new client to an existing service worker and returns its generated
  // client UUID.
  std::string AddClient(int64_t version_id,
                        const content::ServiceWorkerClientInfo& client_info);

  // Adds a new client to an existing service worker with the provided
  // client UUID. Returns |client_uuid| for convenience.
  std::string AddClientWithClientID(
      int64_t version_id,
      std::string client_uuid,
      const content::ServiceWorkerClientInfo& client_info);

  // Removes an existing client from a worker.
  void RemoveClient(int64_t version_id, const std::string& client_uuid);

  // Simulates when the navigation commits, meaning that the RenderFrameHost is
  // now available for a window client. Not valid for worker clients.
  void OnControlleeNavigationCommitted(
      int64_t version_id,
      const std::string& client_uuid,
      content::GlobalRenderFrameHostId render_frame_host_id);

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

TestServiceWorkerContextAdapter::TestServiceWorkerContextAdapter() = default;

TestServiceWorkerContextAdapter::~TestServiceWorkerContextAdapter() = default;

void TestServiceWorkerContextAdapter::AddObserver(
    content::ServiceWorkerContextObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TestServiceWorkerContextAdapter::RemoveObserver(
    content::ServiceWorkerContextObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

int64_t TestServiceWorkerContextAdapter::CreateServiceWorker() {
  // Create a new version ID and add it to the map.
  int64_t version_id = next_service_worker_instance_id_++;

  bool inserted = service_worker_infos_.insert({version_id, {}}).second;
  DCHECK(inserted);

  return version_id;
}

void TestServiceWorkerContextAdapter::DestroyServiceWorker(int64_t version_id) {
  auto it = service_worker_infos_.find(version_id);
  CHECK(it != service_worker_infos_.end(), base::NotFatalUntil::M130);
  const ServiceWorkerInfo& info = it->second;

  // Can only delete a service worker that isn't running and has no clients.
  DCHECK(!info.is_running);
  DCHECK(info.clients.empty());

  // Remove the worker instance from the map.
  service_worker_infos_.erase(it);
}

void TestServiceWorkerContextAdapter::StartServiceWorker(
    int64_t version_id,
    int worker_process_id,
    const GURL& worker_url,
    const GURL& scope_url) {
  auto it = service_worker_infos_.find(version_id);
  CHECK(it != service_worker_infos_.end(), base::NotFatalUntil::M130);
  ServiceWorkerInfo& info = it->second;

  DCHECK(!info.is_running);
  info.is_running = true;

  // Notify observers.
  for (auto& observer : observer_list_) {
    observer.OnVersionStartedRunning(
        version_id,
        content::ServiceWorkerRunningInfo(
            worker_url, scope_url,
            blink::StorageKey::CreateFirstParty(url::Origin::Create(scope_url)),
            worker_process_id, blink::ServiceWorkerToken(),
            content::ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::
                kActivated));
  }
}

void TestServiceWorkerContextAdapter::StopServiceWorker(int64_t version_id) {
  auto it = service_worker_infos_.find(version_id);
  CHECK(it != service_worker_infos_.end(), base::NotFatalUntil::M130);
  ServiceWorkerInfo& info = it->second;

  DCHECK(info.is_running);
  info.is_running = false;

  // Notify observers that the worker is terminating.
  for (auto& observer : observer_list_)
    observer.OnVersionStoppedRunning(version_id);
}

std::string TestServiceWorkerContextAdapter::AddClient(
    int64_t version_id,
    const content::ServiceWorkerClientInfo& client_info) {
  return AddClientWithClientID(
      version_id, base::Uuid::GenerateRandomV4().AsLowercaseString(),
      client_info);
}

std::string TestServiceWorkerContextAdapter::AddClientWithClientID(
    int64_t version_id,
    std::string client_uuid,
    const content::ServiceWorkerClientInfo& client_info) {
  auto it = service_worker_infos_.find(version_id);
  CHECK(it != service_worker_infos_.end(), base::NotFatalUntil::M130);
  ServiceWorkerInfo& info = it->second;

  bool inserted = info.clients.insert(client_uuid).second;
  DCHECK(inserted);

  for (auto& observer : observer_list_)
    observer.OnControlleeAdded(version_id, client_uuid, client_info);

  return client_uuid;
}

void TestServiceWorkerContextAdapter::RemoveClient(
    int64_t version_id,
    const std::string& client_uuid) {
  auto it = service_worker_infos_.find(version_id);
  CHECK(it != service_worker_infos_.end(), base::NotFatalUntil::M130);
  ServiceWorkerInfo& info = it->second;

  size_t removed = info.clients.erase(client_uuid);
  DCHECK_EQ(removed, 1u);

  for (auto& observer : observer_list_)
    observer.OnControlleeRemoved(version_id, client_uuid);
}

void TestServiceWorkerContextAdapter::OnControlleeNavigationCommitted(
    int64_t version_id,
    const std::string& client_uuid,
    content::GlobalRenderFrameHostId render_frame_host_id) {
  auto it = service_worker_infos_.find(version_id);
  CHECK(it != service_worker_infos_.end(), base::NotFatalUntil::M130);
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

  TestProcessNodeSource(const TestProcessNodeSource&) = delete;
  TestProcessNodeSource& operator=(const TestProcessNodeSource&) = delete;

  ~TestProcessNodeSource() override;

  // ProcessNodeSource:
  ProcessNodeImpl* GetProcessNode(int render_process_id) override;

  // Creates a process node and returns its generated render process ID.
  int CreateProcessNode();

 private:
  // Maps render process IDs with their associated process node.
  base::flat_map<int, std::unique_ptr<ProcessNodeImpl>> process_node_map_;
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
  CHECK(it != process_node_map_.end(), base::NotFatalUntil::M130);
  return it->second.get();
}

int TestProcessNodeSource::CreateProcessNode() {
  // Generate a render process ID for this process node.
  static RenderProcessHostId::Generator id_generator;
  RenderProcessHostId render_process_id = id_generator.GenerateNextId();

  // Create the process node and insert it into the map.
  auto process_node = PerformanceManagerImpl::CreateProcessNode(
      RenderProcessHostProxy::CreateForTesting(render_process_id),
      base::TaskPriority::HIGHEST);
  bool inserted =
      process_node_map_
          .insert({render_process_id.value(), std::move(process_node)})
          .second;
  DCHECK(inserted);

  return render_process_id.value();
}

// TestFrameNodeSource ---------------------------------------------------------

class TestFrameNodeSource : public FrameNodeSource {
 public:
  TestFrameNodeSource();

  TestFrameNodeSource(const TestFrameNodeSource&) = delete;
  TestFrameNodeSource& operator=(const TestFrameNodeSource&) = delete;

  ~TestFrameNodeSource() override;

  // FrameNodeSource:
  FrameNodeImpl* GetFrameNode(
      content::GlobalRenderFrameHostId render_frame_host_id) override;
  void SubscribeToFrameNode(
      content::GlobalRenderFrameHostId render_frame_host_id,
      OnbeforeFrameNodeRemovedCallback on_before_frame_node_removed_callback)
      override;
  void UnsubscribeFromFrameNode(
      content::GlobalRenderFrameHostId render_frame_host_id) override;

  // Creates a frame node and returns its generated RenderFrameHost id.
  content::GlobalRenderFrameHostId CreateFrameNode(
      int render_process_id,
      ProcessNodeImpl* process_node);

  // Deletes an existing frame node and notify subscribers.
  void DeleteFrameNode(content::GlobalRenderFrameHostId render_frame_host_id);

 private:
  // Helper function that invokes the OnBeforeFrameNodeRemovedCallback
  // associated with |frame_node| and removes it from the map.
  void InvokeAndRemoveCallback(FrameNodeImpl* frame_node);

  // The page node that hosts all frames.
  std::unique_ptr<PageNodeImpl> page_node_;

  // Maps each frame's RenderFrameHost id with their associated frame node.
  base::flat_map<content::GlobalRenderFrameHostId,
                 std::unique_ptr<FrameNodeImpl>>
      frame_node_map_;

  // Maps each observed frame node to their callback.
  base::flat_map<FrameNodeImpl*, OnbeforeFrameNodeRemovedCallback>
      frame_node_callbacks_;
};

TestFrameNodeSource::TestFrameNodeSource()
    : page_node_(
          PerformanceManagerImpl::CreatePageNode(nullptr,
                                                 "page_node_context_id",
                                                 GURL(),
                                                 PagePropertyFlags{},
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
    content::GlobalRenderFrameHostId render_frame_host_id) {
  auto it = frame_node_map_.find(render_frame_host_id);
  return it != frame_node_map_.end() ? it->second.get() : nullptr;
}

void TestFrameNodeSource::SubscribeToFrameNode(
    content::GlobalRenderFrameHostId render_frame_host_id,
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
    content::GlobalRenderFrameHostId render_frame_host_id) {
  FrameNodeImpl* frame_node = GetFrameNode(render_frame_host_id);
  DCHECK(frame_node);

  size_t removed = frame_node_callbacks_.erase(frame_node);
  DCHECK_EQ(removed, 1u);
}

content::GlobalRenderFrameHostId TestFrameNodeSource::CreateFrameNode(
    int render_process_id,
    ProcessNodeImpl* process_node) {
  int frame_id = GenerateNextId();
  content::GlobalRenderFrameHostId render_frame_host_id(render_process_id,
                                                        frame_id);
  auto frame_node = PerformanceManagerImpl::CreateFrameNode(
      process_node, page_node_.get(), /*parent_frame_node=*/nullptr,
      /*outer_document_for_fenced_frame*/ nullptr, frame_id,
      blink::LocalFrameToken(), content::BrowsingInstanceId(0),
      content::SiteInstanceGroupId(0), /*is_current=*/true);

  bool inserted =
      frame_node_map_.insert({render_frame_host_id, std::move(frame_node)})
          .second;
  DCHECK(inserted);

  return render_frame_host_id;
}

void TestFrameNodeSource::DeleteFrameNode(
    content::GlobalRenderFrameHostId render_frame_host_id) {
  auto it = frame_node_map_.find(render_frame_host_id);
  CHECK(it != frame_node_map_.end(), base::NotFatalUntil::M130);

  FrameNodeImpl* frame_node = it->second.get();

  // Notify the subscriber then delete the node.
  InvokeAndRemoveCallback(frame_node);
  PerformanceManagerImpl::DeleteNode(std::move(it->second));

  frame_node_map_.erase(it);
}

void TestFrameNodeSource::InvokeAndRemoveCallback(FrameNodeImpl* frame_node) {
  auto it = frame_node_callbacks_.find(frame_node);
  CHECK(it != frame_node_callbacks_.end(), base::NotFatalUntil::M130);

  std::move(it->second).Run(frame_node);

  frame_node_callbacks_.erase(it);
}

}  // namespace

class WorkerWatcherTest : public testing::Test {
 public:
  WorkerWatcherTest();

  WorkerWatcherTest(const WorkerWatcherTest&) = delete;
  WorkerWatcherTest& operator=(const WorkerWatcherTest&) = delete;

  ~WorkerWatcherTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Retrieves an existing worker node.
  WorkerNodeImpl* GetDedicatedWorkerNode(
      const blink::DedicatedWorkerToken& token);
  WorkerNodeImpl* GetSharedWorkerNode(
      const blink::SharedWorkerToken& shared_worker_token);
  WorkerNodeImpl* GetServiceWorkerNode(int64_t version_id);

  WorkerWatcher* worker_watcher() { return worker_watcher_.get(); }

  TestDedicatedWorkerService* dedicated_worker_service() {
    return &dedicated_worker_service_;
  }

  TestSharedWorkerService* shared_worker_service() {
    return &shared_worker_service_;
  }

  TestServiceWorkerContextAdapter* service_worker_context_adapter() {
    return &service_worker_context_adapter_;
  }

  TestProcessNodeSource* process_node_source() {
    return process_node_source_.get();
  }

  TestFrameNodeSource* frame_node_source() { return frame_node_source_.get(); }

 protected:
  // Test the frame destroyed case with or without service worker relationship
  void TestFrameDestroyed(bool enable_service_worker_relationships);

 private:
  content::BrowserTaskEnvironment task_environment_;

  TestDedicatedWorkerService dedicated_worker_service_;
  TestSharedWorkerService shared_worker_service_;
  TestServiceWorkerContextAdapter service_worker_context_adapter_;

  std::unique_ptr<PerformanceManagerImpl> performance_manager_;
  std::unique_ptr<TestProcessNodeSource> process_node_source_;
  std::unique_ptr<TestFrameNodeSource> frame_node_source_;

  // The WorkerWatcher that's being tested.
  std::unique_ptr<WorkerWatcher> worker_watcher_;
};

WorkerWatcherTest::WorkerWatcherTest() = default;

WorkerWatcherTest::~WorkerWatcherTest() = default;

void WorkerWatcherTest::SetUp() {
  performance_manager_ = PerformanceManagerImpl::Create(base::DoNothing());

  process_node_source_ = std::make_unique<TestProcessNodeSource>();
  frame_node_source_ = std::make_unique<TestFrameNodeSource>();

  worker_watcher_ = std::make_unique<WorkerWatcher>(
      "browser_context_id", &dedicated_worker_service_, &shared_worker_service_,
      &service_worker_context_adapter_, process_node_source_.get(),
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

// This test creates one dedicated worker with a frame client.
TEST_F(WorkerWatcherTest, SimpleDedicatedWorker) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the frame node.
  content::GlobalRenderFrameHostId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));

  // Create the worker.
  const auto origin = url::Origin::Create(GenerateUniqueDomainUrl());
  const blink::DedicatedWorkerToken token =
      dedicated_worker_service()->CreateDedicatedWorker(
          render_process_id, render_frame_host_id, origin);

  // Check expectations on the graph.
  WorkerNodeImpl* worker_node = GetDedicatedWorkerNode(token);
  RunInGraph([&,
              process_node =
                  process_node_source()->GetProcessNode(render_process_id),
              client_frame_node = frame_node_source()->GetFrameNode(
                  render_frame_host_id)](GraphImpl* graph) {
    EXPECT_TRUE(graph->NodeInGraph(worker_node));
    EXPECT_EQ(worker_node->GetWorkerType(), WorkerNode::WorkerType::kDedicated);
    EXPECT_EQ(worker_node->process_node(), process_node);
    EXPECT_EQ(worker_node->GetOrigin(), origin);
    // Script URL not available until script loads.
    EXPECT_FALSE(worker_node->GetURL().is_valid());
    EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));
  });

  EXPECT_EQ(worker_watcher()->FindWorkerNodeForToken(token), worker_node);

  // Disconnect and clean up the dedicated worker.
  dedicated_worker_service()->DestroyDedicatedWorker(token);

  EXPECT_EQ(worker_watcher()->FindWorkerNodeForToken(token), nullptr);
}

TEST_F(WorkerWatcherTest, NestedDedicatedWorker) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the ancestor frame node.
  content::GlobalRenderFrameHostId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));

  // Create the parent worker.
  const auto parent_origin = url::Origin::Create(GenerateUniqueDomainUrl());
  const blink::DedicatedWorkerToken parent_worker_token =
      dedicated_worker_service()->CreateDedicatedWorker(
          render_process_id, render_frame_host_id, parent_origin);

  // Create the nested worker with an opaque origin derived from the parent
  // origin.
  const auto nested_origin = url::Origin::Resolve(GURL(), parent_origin);
  const blink::DedicatedWorkerToken nested_worker_token =
      dedicated_worker_service()->CreateDedicatedWorker(
          render_process_id, parent_worker_token, nested_origin);

  // Check expectations on the graph.
  RunInGraph(
      [&,
       process_node = process_node_source()->GetProcessNode(render_process_id),
       parent_worker_node = GetDedicatedWorkerNode(parent_worker_token),
       nested_worker_node = GetDedicatedWorkerNode(nested_worker_token),
       ancestor_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(nested_worker_node));
        EXPECT_EQ(nested_worker_node->GetWorkerType(),
                  WorkerNode::WorkerType::kDedicated);
        EXPECT_EQ(nested_worker_node->process_node(), process_node);
        EXPECT_EQ(nested_worker_node->GetOrigin(), nested_origin);
        // Script URL not available until script loads.
        EXPECT_FALSE(nested_worker_node->GetURL().is_valid());
        // The ancestor frame is not directly a client of the nested worker.
        EXPECT_FALSE(IsWorkerClient(nested_worker_node, ancestor_frame_node));
        EXPECT_TRUE(IsWorkerClient(nested_worker_node, parent_worker_node));
      });

  // Disconnect and clean up the dedicated workers.
  dedicated_worker_service()->DestroyDedicatedWorker(nested_worker_token);
  dedicated_worker_service()->DestroyDedicatedWorker(parent_worker_token);
}

// This test creates one shared worker with one client frame.
TEST_F(WorkerWatcherTest, SimpleSharedWorker) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the frame node.
  content::GlobalRenderFrameHostId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));

  // Create the worker.
  const auto origin = url::Origin::Create(GenerateUniqueDomainUrl());
  const blink::SharedWorkerToken shared_worker_token =
      shared_worker_service()->CreateSharedWorker(render_process_id, origin);

  // Connect the frame to the worker.
  shared_worker_service()->AddClient(shared_worker_token, render_frame_host_id);

  // Check expectations on the graph.
  WorkerNodeImpl* worker_node = GetSharedWorkerNode(shared_worker_token);
  RunInGraph([&,
              process_node =
                  process_node_source()->GetProcessNode(render_process_id),
              client_frame_node = frame_node_source()->GetFrameNode(
                  render_frame_host_id)](GraphImpl* graph) {
    EXPECT_TRUE(graph->NodeInGraph(worker_node));
    EXPECT_EQ(worker_node->GetWorkerType(), WorkerNode::WorkerType::kShared);
    EXPECT_EQ(worker_node->process_node(), process_node);
    EXPECT_EQ(worker_node->GetOrigin(), origin);
    // Script URL not available until script loads.
    EXPECT_FALSE(worker_node->GetURL().is_valid());
    EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));
  });

  EXPECT_EQ(worker_watcher()->FindWorkerNodeForToken(shared_worker_token),
            worker_node);

  // Disconnect and clean up the shared worker.
  shared_worker_service()->RemoveClient(shared_worker_token,
                                        render_frame_host_id);
  shared_worker_service()->DestroySharedWorker(shared_worker_token);

  EXPECT_EQ(worker_watcher()->FindWorkerNodeForToken(shared_worker_token),
            nullptr);
}

// This test creates one service worker with one client frame.
TEST_F(WorkerWatcherTest, ServiceWorkerFrameClient) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create and start the service worker.
  int64_t service_worker_version_id =
      service_worker_context_adapter()->CreateServiceWorker();
  const GURL worker_url = GenerateWorkerUrl();
  const GURL scope_url = GenerateUniqueDomainUrl();
  // Make sure origins created from `worker_url` and `scope_url` can't be
  // confused.
  EXPECT_NE(url::Origin::Create(worker_url), url::Origin::Create(scope_url));

  service_worker_context_adapter()->StartServiceWorker(
      service_worker_version_id, render_process_id, worker_url, scope_url);

  // Add a window client of the service worker.
  std::string service_worker_client_uuid =
      service_worker_context_adapter()->AddClient(
          service_worker_version_id, content::ServiceWorkerClientInfo());

  // Check expectations on the graph.
  const WorkerNodeImpl* worker_node =
      GetServiceWorkerNode(service_worker_version_id);
  blink::WorkerToken token;
  RunInGraph([&, process_node = process_node_source()->GetProcessNode(
                     render_process_id)](GraphImpl* graph) {
    EXPECT_TRUE(graph->NodeInGraph(worker_node));
    EXPECT_EQ(worker_node->GetWorkerType(), WorkerNode::WorkerType::kService);
    EXPECT_EQ(worker_node->process_node(), process_node);
    EXPECT_EQ(worker_node->GetOrigin(), url::Origin::Create(scope_url));
    EXPECT_EQ(worker_node->GetURL(), worker_url);

    // The frame can not be connected to the service worker until its
    // RenderFrameHost is available, which happens when the navigation
    // commits.
    EXPECT_TRUE(worker_node->client_frames().empty());

    // Save the token for the FindWorkerNodeForToken() test.
    token = worker_node->GetWorkerToken();
  });

  // Now simulate the navigation commit.
  content::GlobalRenderFrameHostId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));
  service_worker_context_adapter()->OnControlleeNavigationCommitted(
      service_worker_version_id, service_worker_client_uuid,
      render_frame_host_id);

  // Check expectations on the graph.
  RunInGraph(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       worker_node = GetServiceWorkerNode(service_worker_version_id),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->GetWorkerType(),
                  WorkerNode::WorkerType::kService);
        EXPECT_EQ(worker_node->process_node(), process_node);
        EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));
      });

  EXPECT_EQ(worker_watcher()->FindWorkerNodeForToken(token), worker_node);

  // Disconnect and clean up the service worker.
  service_worker_context_adapter()->RemoveClient(service_worker_version_id,
                                                 service_worker_client_uuid);
  service_worker_context_adapter()->StopServiceWorker(
      service_worker_version_id);
  service_worker_context_adapter()->DestroyServiceWorker(
      service_worker_version_id);

  EXPECT_EQ(worker_watcher()->FindWorkerNodeForToken(token), nullptr);
}

// Ensures that the WorkerWatcher handles the case where a frame with a service
// worker is (briefly?) an uncommitted client of two versions. This presumably
// happens on version update or some such, or perhaps when a frame is a
// bona-fide client of two service workers. Apparently this happens quite
// rarely in the field.
TEST_F(WorkerWatcherTest, ServiceWorkerFrameClientOfTwoWorkers) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create and start both service workers.
  int64_t first_service_worker_version_id =
      service_worker_context_adapter()->CreateServiceWorker();
  const GURL first_worker_url = GenerateWorkerUrl();
  const GURL first_scope_url = GenerateUniqueDomainUrl();
  service_worker_context_adapter()->StartServiceWorker(
      first_service_worker_version_id, render_process_id, first_worker_url,
      first_scope_url);
  int64_t second_service_worker_version_id =
      service_worker_context_adapter()->CreateServiceWorker();
  const GURL second_worker_url = GenerateWorkerUrl();
  const GURL second_scope_url = GenerateUniqueDomainUrl();
  service_worker_context_adapter()->StartServiceWorker(
      second_service_worker_version_id, render_process_id, second_worker_url,
      second_scope_url);

  // Make sure origins created from `worker_url` and `scope_url` can't be
  // confused.
  EXPECT_NE(url::Origin::Create(first_worker_url),
            url::Origin::Create(first_scope_url));
  EXPECT_NE(url::Origin::Create(second_worker_url),
            url::Origin::Create(second_scope_url));

  // Add a window client of the service worker.
  std::string service_worker_client_uuid =
      service_worker_context_adapter()->AddClient(
          first_service_worker_version_id, content::ServiceWorkerClientInfo());
  service_worker_context_adapter()->AddClientWithClientID(
      second_service_worker_version_id, service_worker_client_uuid,
      content::ServiceWorkerClientInfo());

  // Check expectations on the graph.
  RunInGraph(
      [&,
       process_node = process_node_source()->GetProcessNode(render_process_id),
       first_worker_node =
           GetServiceWorkerNode(first_service_worker_version_id),
       second_worker_node = GetServiceWorkerNode(
           second_service_worker_version_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(first_worker_node));
        EXPECT_EQ(first_worker_node->GetWorkerType(),
                  WorkerNode::WorkerType::kService);
        EXPECT_EQ(first_worker_node->process_node(), process_node);
        EXPECT_EQ(first_worker_node->GetOrigin(),
                  url::Origin::Create(first_scope_url));
        EXPECT_EQ(first_worker_node->GetURL(), first_worker_url);
        // The frame was never added as a client of the service worker.
        EXPECT_TRUE(first_worker_node->client_frames().empty());

        EXPECT_TRUE(graph->NodeInGraph(second_worker_node));
        EXPECT_EQ(second_worker_node->GetWorkerType(),
                  WorkerNode::WorkerType::kService);
        EXPECT_EQ(second_worker_node->process_node(), process_node);
        EXPECT_EQ(second_worker_node->GetOrigin(),
                  url::Origin::Create(second_scope_url));
        EXPECT_EQ(second_worker_node->GetURL(), second_worker_url);
        // The frame was never added as a client of the service worker.
        EXPECT_TRUE(second_worker_node->client_frames().empty());
      });

  // Disconnect and clean up the service worker.
  service_worker_context_adapter()->RemoveClient(
      first_service_worker_version_id, service_worker_client_uuid);
  service_worker_context_adapter()->StopServiceWorker(
      first_service_worker_version_id);
  service_worker_context_adapter()->DestroyServiceWorker(
      first_service_worker_version_id);

  service_worker_context_adapter()->RemoveClient(
      second_service_worker_version_id, service_worker_client_uuid);
  service_worker_context_adapter()->StopServiceWorker(
      second_service_worker_version_id);
  service_worker_context_adapter()->DestroyServiceWorker(
      second_service_worker_version_id);
}

// Ensures that the WorkerWatcher handles the case where a frame with a service
// worker has a double client relationship with a service worker.
// This appears to be happening out in the real world, if quite rarely.
// See https://crbug.com/1143281#c33.
TEST_F(WorkerWatcherTest, ServiceWorkerTwoFrameClientRelationships) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create and start a service worker.
  int64_t service_worker_version_id =
      service_worker_context_adapter()->CreateServiceWorker();
  service_worker_context_adapter()->StartServiceWorker(
      service_worker_version_id, render_process_id);

  // Add a window client of the service worker.
  std::string first_client_uuid = service_worker_context_adapter()->AddClient(
      service_worker_version_id, content::ServiceWorkerClientInfo());

  // Check expectations on the graph.
  RunInGraph(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       worker_node =
           GetServiceWorkerNode(service_worker_version_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->GetWorkerType(),
                  WorkerNode::WorkerType::kService);
        // The frame was not yet added as a client.
        EXPECT_TRUE(worker_node->client_frames().empty());
      });

  // Add a second client relationship between the same two entities.
  std::string second_client_uuid = service_worker_context_adapter()->AddClient(
      service_worker_version_id, content::ServiceWorkerClientInfo());

  // Now simulate the navigation commit.
  content::GlobalRenderFrameHostId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));
  service_worker_context_adapter()->OnControlleeNavigationCommitted(
      service_worker_version_id, first_client_uuid, render_frame_host_id);

  RunInGraph(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       service_worker_node = GetServiceWorkerNode(service_worker_version_id),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(service_worker_node));
        EXPECT_EQ(service_worker_node->GetWorkerType(),
                  WorkerNode::WorkerType::kService);
        EXPECT_EQ(1u, service_worker_node->client_frames().size());
        EXPECT_TRUE(IsWorkerClient(service_worker_node, client_frame_node));
      });

  // Commit the second controllee navigation.
  service_worker_context_adapter()->OnControlleeNavigationCommitted(
      service_worker_version_id, second_client_uuid, render_frame_host_id);
  // Verify that the graph is still the same.
  RunInGraph(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       service_worker_node = GetServiceWorkerNode(service_worker_version_id),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(service_worker_node));
        EXPECT_EQ(service_worker_node->GetWorkerType(),
                  WorkerNode::WorkerType::kService);
        EXPECT_EQ(1u, service_worker_node->client_frames().size());
        EXPECT_TRUE(IsWorkerClient(service_worker_node, client_frame_node));
      });

  // Remove the first client relationship.
  service_worker_context_adapter()->RemoveClient(service_worker_version_id,
                                                 first_client_uuid);
  // Verify that the graph is still the same.
  RunInGraph(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       service_worker_node = GetServiceWorkerNode(service_worker_version_id),
       client_frame_node = frame_node_source()->GetFrameNode(
           render_frame_host_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(service_worker_node));
        EXPECT_EQ(service_worker_node->GetWorkerType(),
                  WorkerNode::WorkerType::kService);
        EXPECT_EQ(1u, service_worker_node->client_frames().size());
        EXPECT_TRUE(IsWorkerClient(service_worker_node, client_frame_node));
      });

  // Teardown.
  service_worker_context_adapter()->RemoveClient(service_worker_version_id,
                                                 second_client_uuid);
  service_worker_context_adapter()->StopServiceWorker(
      service_worker_version_id);
  service_worker_context_adapter()->DestroyServiceWorker(
      service_worker_version_id);
}

// Ensures that the WorkerWatcher handles the case where a frame with a service
// worker is created but it's navigation is never committed before the
// FrameTreeNode is destroyed.
TEST_F(WorkerWatcherTest, ServiceWorkerFrameClientDestroyedBeforeCommit) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create and start the service worker.
  int64_t service_worker_version_id =
      service_worker_context_adapter()->CreateServiceWorker();
  service_worker_context_adapter()->StartServiceWorker(
      service_worker_version_id, render_process_id);

  // Add a window client of the service worker.
  std::string service_worker_client_uuid =
      service_worker_context_adapter()->AddClient(
          service_worker_version_id, content::ServiceWorkerClientInfo());

  // Check expectations on the graph.
  RunInGraph(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       worker_node =
           GetServiceWorkerNode(service_worker_version_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(worker_node));
        EXPECT_EQ(worker_node->GetWorkerType(),
                  WorkerNode::WorkerType::kService);
        EXPECT_EQ(worker_node->process_node(), process_node);

        // The frame was never added as a client of the service worker.
        EXPECT_TRUE(worker_node->client_frames().empty());
      });

  // Disconnect and clean up the service worker.
  service_worker_context_adapter()->RemoveClient(service_worker_version_id,
                                                 service_worker_client_uuid);
  service_worker_context_adapter()->StopServiceWorker(
      service_worker_version_id);
  service_worker_context_adapter()->DestroyServiceWorker(
      service_worker_version_id);
}

TEST_F(WorkerWatcherTest, AllTypesOfServiceWorkerClients) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create and start the service worker.
  int64_t service_worker_version_id =
      service_worker_context_adapter()->CreateServiceWorker();
  service_worker_context_adapter()->StartServiceWorker(
      service_worker_version_id, render_process_id);

  // Create a client of each type and connect them to the service worker.

  // Frame client.
  std::string frame_client_uuid = service_worker_context_adapter()->AddClient(
      service_worker_version_id, content::ServiceWorkerClientInfo());
  content::GlobalRenderFrameHostId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));
  service_worker_context_adapter()->OnControlleeNavigationCommitted(
      service_worker_version_id, frame_client_uuid, render_frame_host_id);

  // Dedicated worker client.
  blink::DedicatedWorkerToken dedicated_worker_token =
      dedicated_worker_service()->CreateDedicatedWorker(render_process_id,
                                                        render_frame_host_id);
  std::string dedicated_worker_client_uuid =
      service_worker_context_adapter()->AddClient(
          service_worker_version_id,
          content::ServiceWorkerClientInfo(dedicated_worker_token));

  // Shared worker client.
  blink::SharedWorkerToken shared_worker_token =
      shared_worker_service()->CreateSharedWorker(render_process_id);
  std::string shared_worker_client_uuid =
      service_worker_context_adapter()->AddClient(
          service_worker_version_id,
          content::ServiceWorkerClientInfo(shared_worker_token));

  // Check expectations on the graph.
  RunInGraph(
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
      });

  // Disconnect and clean up the service worker and its clients.
  service_worker_context_adapter()->RemoveClient(service_worker_version_id,
                                                 shared_worker_client_uuid);
  shared_worker_service()->DestroySharedWorker(shared_worker_token);
  service_worker_context_adapter()->RemoveClient(service_worker_version_id,
                                                 dedicated_worker_client_uuid);
  dedicated_worker_service()->DestroyDedicatedWorker(dedicated_worker_token);
  service_worker_context_adapter()->RemoveClient(service_worker_version_id,
                                                 frame_client_uuid);

  service_worker_context_adapter()->StopServiceWorker(
      service_worker_version_id);
  service_worker_context_adapter()->DestroyServiceWorker(
      service_worker_version_id);
}

// Tests that the WorkerWatcher can handle the case where the service worker
// starts after it has been assigned a client. In this case, the clients are not
// connected to the service worker until it starts. It also tests that when the
// service worker stops, its existing clients are also disconnected.
TEST_F(WorkerWatcherTest, ServiceWorkerStartsAndStopsWithExistingClients) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the worker.
  int64_t service_worker_version_id =
      service_worker_context_adapter()->CreateServiceWorker();

  // Create a client of each type and connect them to the service worker.

  // Frame client.
  std::string frame_client_uuid = service_worker_context_adapter()->AddClient(
      service_worker_version_id, content::ServiceWorkerClientInfo());
  content::GlobalRenderFrameHostId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));
  service_worker_context_adapter()->OnControlleeNavigationCommitted(
      service_worker_version_id, frame_client_uuid, render_frame_host_id);

  // Dedicated worker client.
  blink::DedicatedWorkerToken dedicated_worker_token =
      dedicated_worker_service()->CreateDedicatedWorker(render_process_id,
                                                        render_frame_host_id);
  std::string dedicated_worker_client_uuid =
      service_worker_context_adapter()->AddClient(
          service_worker_version_id,
          content::ServiceWorkerClientInfo(dedicated_worker_token));

  // Shared worker client.
  blink::SharedWorkerToken shared_worker_token =
      shared_worker_service()->CreateSharedWorker(render_process_id);
  std::string shared_worker_client_uuid =
      service_worker_context_adapter()->AddClient(
          service_worker_version_id,
          content::ServiceWorkerClientInfo(shared_worker_token));

  // The service worker node doesn't even exist yet.
  EXPECT_FALSE(GetServiceWorkerNode(service_worker_version_id));

  // Check expectations on the graph.
  RunInGraph(
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
      });

  // Now start the service worker.
  service_worker_context_adapter()->StartServiceWorker(
      service_worker_version_id, render_process_id);

  // Check expectations on the graph.
  RunInGraph(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       service_worker_node = GetServiceWorkerNode(service_worker_version_id),
       frame_node = frame_node_source()->GetFrameNode(render_frame_host_id),
       dedicated_worker_node = GetDedicatedWorkerNode(dedicated_worker_token),
       shared_worker_node =
           GetSharedWorkerNode(shared_worker_token)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(service_worker_node));
        EXPECT_EQ(service_worker_node->GetWorkerType(),
                  WorkerNode::WorkerType::kService);
        EXPECT_EQ(service_worker_node->process_node(), process_node);

        EXPECT_TRUE(graph->NodeInGraph(frame_node));
        EXPECT_TRUE(graph->NodeInGraph(dedicated_worker_node));
        EXPECT_TRUE(graph->NodeInGraph(shared_worker_node));

        // Now is it correctly hooked up.
        EXPECT_TRUE(IsWorkerClient(service_worker_node, frame_node));
        EXPECT_TRUE(IsWorkerClient(service_worker_node, dedicated_worker_node));
        EXPECT_TRUE(IsWorkerClient(service_worker_node, shared_worker_node));
      });

  // Stop the service worker. All the clients will be disconnected.
  service_worker_context_adapter()->StopServiceWorker(
      service_worker_version_id);

  // Check expectations on the graph.
  RunInGraph(
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
      });

  // Disconnect and clean up the service worker and its clients
  service_worker_context_adapter()->RemoveClient(service_worker_version_id,
                                                 shared_worker_client_uuid);
  shared_worker_service()->DestroySharedWorker(shared_worker_token);
  service_worker_context_adapter()->RemoveClient(service_worker_version_id,
                                                 dedicated_worker_client_uuid);
  dedicated_worker_service()->DestroyDedicatedWorker(dedicated_worker_token);
  service_worker_context_adapter()->RemoveClient(service_worker_version_id,
                                                 frame_client_uuid);

  service_worker_context_adapter()->DestroyServiceWorker(
      service_worker_version_id);
}

TEST_F(WorkerWatcherTest, SharedWorkerCrossProcessClient) {
  // Create the frame node.
  int frame_process_id = process_node_source()->CreateProcessNode();
  content::GlobalRenderFrameHostId render_frame_host_id =
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
  RunInGraph([worker_process_node =
                  process_node_source()->GetProcessNode(worker_process_id),
              worker_node = GetSharedWorkerNode(shared_worker_token),
              client_process_node =
                  process_node_source()->GetProcessNode(frame_process_id),
              client_frame_node = frame_node_source()->GetFrameNode(
                  render_frame_host_id)](GraphImpl* graph) {
    EXPECT_TRUE(graph->NodeInGraph(worker_node));
    EXPECT_EQ(worker_node->GetWorkerType(), WorkerNode::WorkerType::kShared);
    EXPECT_EQ(worker_node->process_node(), worker_process_node);
    EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node));
  });

  // Disconnect and clean up the shared worker.
  shared_worker_service()->RemoveClient(shared_worker_token,
                                        render_frame_host_id);
  shared_worker_service()->DestroySharedWorker(shared_worker_token);
}

// Tests that the WorkerWatcher can handle the case where the service worker
// starts after it has been assigned a worker client, but the client has
// already died by the time the service worker starts.
TEST_F(WorkerWatcherTest, SharedWorkerStartsWithDeadWorkerClients) {
  int render_process_id = process_node_source()->CreateProcessNode();
  content::GlobalRenderFrameHostId render_frame_host_id =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));

  // Create the worker.
  int64_t service_worker_version_id =
      service_worker_context_adapter()->CreateServiceWorker();

  // Create a worker client of each type and connect them to the service worker.
  // Dedicated worker client.
  blink::DedicatedWorkerToken dedicated_worker_token =
      dedicated_worker_service()->CreateDedicatedWorker(render_process_id,
                                                        render_frame_host_id);
  std::string dedicated_worker_client_uuid =
      service_worker_context_adapter()->AddClient(
          service_worker_version_id,
          content::ServiceWorkerClientInfo(dedicated_worker_token));

  // Shared worker client.
  blink::SharedWorkerToken shared_worker_token =
      shared_worker_service()->CreateSharedWorker(render_process_id);
  std::string shared_worker_client_uuid =
      service_worker_context_adapter()->AddClient(
          service_worker_version_id,
          content::ServiceWorkerClientInfo(shared_worker_token));

  // Destroy the workers before the service worker starts.
  shared_worker_service()->DestroySharedWorker(shared_worker_token);
  dedicated_worker_service()->DestroyDedicatedWorker(dedicated_worker_token);

  // Now start the service worker.
  service_worker_context_adapter()->StartServiceWorker(
      service_worker_version_id, render_process_id);

  // Check expectations on the graph.
  RunInGraph(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       service_worker_node =
           GetServiceWorkerNode(service_worker_version_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(service_worker_node));
        EXPECT_EQ(service_worker_node->GetWorkerType(),
                  WorkerNode::WorkerType::kService);
        EXPECT_EQ(service_worker_node->process_node(), process_node);
        EXPECT_TRUE(service_worker_node->child_workers().empty());
      });

  // Disconnect the non-existent clients.
  service_worker_context_adapter()->RemoveClient(service_worker_version_id,
                                                 shared_worker_client_uuid);
  service_worker_context_adapter()->RemoveClient(service_worker_version_id,
                                                 dedicated_worker_client_uuid);

  // No changes in the graph.
  RunInGraph(
      [process_node = process_node_source()->GetProcessNode(render_process_id),
       service_worker_node =
           GetServiceWorkerNode(service_worker_version_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(service_worker_node));
        EXPECT_EQ(service_worker_node->GetWorkerType(),
                  WorkerNode::WorkerType::kService);
        EXPECT_EQ(service_worker_node->process_node(), process_node);
        EXPECT_TRUE(service_worker_node->child_workers().empty());
      });

  // Stop and destroy the service worker.
  service_worker_context_adapter()->StopServiceWorker(
      service_worker_version_id);
  service_worker_context_adapter()->DestroyServiceWorker(
      service_worker_version_id);
}

TEST_F(WorkerWatcherTest, SharedWorkerDiesAsServiceWorkerClient) {
  // Create the shared and service workers.
  int render_process_id = process_node_source()->CreateProcessNode();
  const blink::SharedWorkerToken& shared_worker_token =
      shared_worker_service()->CreateSharedWorker(render_process_id);
  int64_t service_worker_version_id =
      service_worker_context_adapter()->CreateServiceWorker();

  std::string service_worker_client_uuid =
      service_worker_context_adapter()->AddClient(
          service_worker_version_id,
          content::ServiceWorkerClientInfo(shared_worker_token));
  service_worker_context_adapter()->StartServiceWorker(
      service_worker_version_id, render_process_id);

  // Check expectations on the graph.
  RunInGraph(
      [service_worker_node = GetServiceWorkerNode(service_worker_version_id),
       shared_worker_node =
           GetSharedWorkerNode(shared_worker_token)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(service_worker_node));
        EXPECT_EQ(service_worker_node->GetWorkerType(),
                  WorkerNode::WorkerType::kService);
        EXPECT_TRUE(graph->NodeInGraph(shared_worker_node));
        EXPECT_EQ(shared_worker_node->GetWorkerType(),
                  WorkerNode::WorkerType::kShared);
        EXPECT_TRUE(IsWorkerClient(service_worker_node, shared_worker_node));
      });

  // Destroy the shared worker while it still has a client registration
  // against the service worker.
  shared_worker_service()->DestroySharedWorker(shared_worker_token);

  // Check expectations on the graph again.
  RunInGraph(
      [service_worker_node =
           GetServiceWorkerNode(service_worker_version_id)](GraphImpl* graph) {
        EXPECT_TRUE(graph->NodeInGraph(service_worker_node));
        EXPECT_EQ(service_worker_node->GetWorkerType(),
                  WorkerNode::WorkerType::kService);
        EXPECT_TRUE(service_worker_node->client_workers().empty());
      });

  // Issue the trailing service worker client removal.
  service_worker_context_adapter()->RemoveClient(service_worker_version_id,
                                                 service_worker_client_uuid);
}

TEST_F(WorkerWatcherTest, OneSharedWorkerTwoClients) {
  int render_process_id = process_node_source()->CreateProcessNode();

  // Create the worker.
  const blink::SharedWorkerToken& shared_worker_token =
      shared_worker_service()->CreateSharedWorker(render_process_id);

  // Create 2 client frame nodes and connect them to the worker.
  content::GlobalRenderFrameHostId render_frame_host_id_1 =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));
  shared_worker_service()->AddClient(shared_worker_token,
                                     render_frame_host_id_1);

  content::GlobalRenderFrameHostId render_frame_host_id_2 =
      frame_node_source()->CreateFrameNode(
          render_process_id,
          process_node_source()->GetProcessNode(render_process_id));
  shared_worker_service()->AddClient(shared_worker_token,
                                     render_frame_host_id_2);

  // Check expectations on the graph.
  RunInGraph([worker_node = GetSharedWorkerNode(shared_worker_token),
              client_frame_node_1 =
                  frame_node_source()->GetFrameNode(render_frame_host_id_1),
              client_frame_node_2 = frame_node_source()->GetFrameNode(
                  render_frame_host_id_2)](GraphImpl* graph) {
    EXPECT_TRUE(graph->NodeInGraph(worker_node));
    EXPECT_EQ(worker_node->GetWorkerType(), WorkerNode::WorkerType::kShared);

    // Check frame 1.
    EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node_1));

    // Check frame 2.
    EXPECT_TRUE(IsWorkerClient(worker_node, client_frame_node_2));
  });

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
  content::GlobalRenderFrameHostId render_frame_host_id =
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
  RunInGraph([worker_node_1 = GetSharedWorkerNode(shared_worker_token_1),
              worker_node_2 = GetSharedWorkerNode(shared_worker_token_2),
              client_frame_node = frame_node_source()->GetFrameNode(
                  render_frame_host_id)](GraphImpl* graph) {
    // Check worker 1.
    EXPECT_TRUE(graph->NodeInGraph(worker_node_1));
    EXPECT_EQ(worker_node_1->GetWorkerType(), WorkerNode::WorkerType::kShared);
    EXPECT_TRUE(IsWorkerClient(worker_node_1, client_frame_node));

    // Check worker 2.
    EXPECT_TRUE(graph->NodeInGraph(worker_node_2));
    EXPECT_EQ(worker_node_2->GetWorkerType(), WorkerNode::WorkerType::kShared);
    EXPECT_TRUE(IsWorkerClient(worker_node_2, client_frame_node));
  });

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
  content::GlobalRenderFrameHostId render_frame_host_id =
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
      service_worker_context_adapter()->CreateServiceWorker();
  service_worker_context_adapter()->StartServiceWorker(
      service_worker_version_id, render_process_id);

  // Connect the frame to the shared worker and the service worker. Note that it
  // is already connected to the dedicated worker.
  shared_worker_service()->AddClient(shared_worker_token, render_frame_host_id);
  std::string service_worker_client_uuid =
      service_worker_context_adapter()->AddClient(
          service_worker_version_id, content::ServiceWorkerClientInfo());
  service_worker_context_adapter()->OnControlleeNavigationCommitted(
      service_worker_version_id, service_worker_client_uuid,
      render_frame_host_id);

  // Check that everything is wired up correctly.
  RunInGraph(
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
        EXPECT_TRUE(IsWorkerClient(service_worker_node, client_frame_node));
      });

  frame_node_source()->DeleteFrameNode(render_frame_host_id);

  // Check that the workers are no longer connected to the deleted frame.
  RunInGraph(
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
      });

  // Clean up. The watcher is still expecting a worker removed notification.
  service_worker_context_adapter()->RemoveClient(service_worker_version_id,
                                                 service_worker_client_uuid);
  service_worker_context_adapter()->StopServiceWorker(
      service_worker_version_id);
  service_worker_context_adapter()->DestroyServiceWorker(
      service_worker_version_id);
  shared_worker_service()->RemoveClient(shared_worker_token,
                                        render_frame_host_id);
  shared_worker_service()->DestroySharedWorker(shared_worker_token);
  dedicated_worker_service()->DestroyDedicatedWorker(dedicated_worker_token);
}

}  // namespace performance_manager

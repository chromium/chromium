// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/performance_manager_test_harness.h"

#include <memory>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "components/performance_manager/performance_manager_registry_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/worker_watcher.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/dedicated_worker_service.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/service_worker_running_info.h"
#include "content/public/browser/shared_worker_service.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager {

// PerformanceManagerTestHarness -----------------------------------------------

PerformanceManagerTestHarness::PerformanceManagerTestHarness() {
  // Ensure the helper is available at construction so that graph features and
  // callbacks can be configured.
  helper_ = std::make_unique<PerformanceManagerTestHarnessHelper>();
}

PerformanceManagerTestHarness::~PerformanceManagerTestHarness() = default;

void PerformanceManagerTestHarness::SetUp() {
  DCHECK(helper_);
  Super::SetUp();
  helper_->SetGraphImplCallback(base::BindOnce(
      &PerformanceManagerTestHarness::OnGraphCreated, base::Unretained(this)));
  helper_->SetUp();
  helper_->OnBrowserContextAdded(GetBrowserContext());

  WorkerWatcher* worker_watcher =
      PerformanceManagerRegistryImpl::GetInstance()->GetWorkerWatcherForTesting(
          GetBrowserContext());
  CHECK(worker_watcher);

  dedicated_worker_factory_ =
      std::make_unique<DedicatedWorkerFactory>(worker_watcher);
  shared_worker_factory_ =
      std::make_unique<SharedWorkerFactory>(worker_watcher);
  service_worker_factory_ =
      std::make_unique<ServiceWorkerFactory>(worker_watcher);
}

void PerformanceManagerTestHarness::TearDown() {
  TearDownNow();
  Super::TearDown();
}

std::unique_ptr<content::WebContents>
PerformanceManagerTestHarness::CreateTestWebContents() {
  DCHECK(helper_);
  std::unique_ptr<content::WebContents> contents =
      Super::CreateTestWebContents();
  helper_->OnWebContentsCreated(contents.get());
  return contents;
}

void PerformanceManagerTestHarness::TearDownNow() {
  service_worker_factory_.reset();
  shared_worker_factory_.reset();
  dedicated_worker_factory_.reset();
  if (helper_) {
    helper_->OnBrowserContextRemoved(GetBrowserContext());
    helper_->TearDown();
    helper_.reset();
  }
}

// DedicatedWorkerFactory ------------------------------------------------------

PerformanceManagerTestHarness::DedicatedWorkerFactory::DedicatedWorkerFactory(
    content::DedicatedWorkerService::Observer* observer)
    : observer_(observer) {}

PerformanceManagerTestHarness::DedicatedWorkerFactory::
    ~DedicatedWorkerFactory() {
  CHECK(dedicated_worker_creators_.empty());
}

const blink::DedicatedWorkerToken&
PerformanceManagerTestHarness::DedicatedWorkerFactory::CreateDedicatedWorker(
    const ProcessNode* process_node,
    const FrameNode* frame_node,
    const url::Origin& origin) {
  int worker_process_id =
      process_node->GetRenderProcessHostId().GetUnsafeValue();
  content::GlobalRenderFrameHostId render_frame_host_id =
      frame_node->GetRenderFrameHostProxy().global_frame_routing_id();

  // Create a new token for the worker and add it to the map, along with its
  // client ID.
  const blink::DedicatedWorkerToken token;

  auto result = dedicated_worker_creators_.emplace(token, render_frame_host_id);
  DCHECK(result.second);  // Check inserted.

  // Notify observers.
  observer_->OnWorkerCreated(token, worker_process_id, origin,
                             render_frame_host_id);

  return result.first->first;
}

const blink::DedicatedWorkerToken&
PerformanceManagerTestHarness::DedicatedWorkerFactory::CreateDedicatedWorker(
    const ProcessNode* process_node,
    const WorkerNode* parent_dedicated_worker_node,
    const url::Origin& origin) {
  int worker_process_id =
      process_node->GetRenderProcessHostId().GetUnsafeValue();

  // Create a new token for the worker and add it to the map, along with its
  // client ID.
  const blink::DedicatedWorkerToken token;

  const blink::DedicatedWorkerToken parent_token =
      parent_dedicated_worker_node->GetWorkerToken()
          .GetAs<blink::DedicatedWorkerToken>();

  auto result = dedicated_worker_creators_.emplace(token, parent_token);
  DCHECK(result.second);  // Check inserted.

  // Notify observers.
  observer_->OnWorkerCreated(token, worker_process_id, origin, parent_token);

  return result.first->first;
}

void PerformanceManagerTestHarness::DedicatedWorkerFactory::
    DestroyDedicatedWorker(const blink::DedicatedWorkerToken& token) {
  auto it = dedicated_worker_creators_.find(token);
  CHECK(it != dedicated_worker_creators_.end());

  // Notify observers that the worker is being destroyed.
  observer_->OnBeforeWorkerDestroyed(token, it->second);

  // Remove the worker ID from the map.
  dedicated_worker_creators_.erase(it);
}

// SharedWorkerFactory ---------------------------------------------------------

PerformanceManagerTestHarness::SharedWorkerFactory::SharedWorkerFactory(
    content::SharedWorkerService::Observer* observer)
    : observer_(observer) {}

PerformanceManagerTestHarness::SharedWorkerFactory::~SharedWorkerFactory() {
  CHECK(shared_worker_client_frames_.empty());
}

blink::SharedWorkerToken
PerformanceManagerTestHarness::SharedWorkerFactory::CreateSharedWorker(
    const ProcessNode* process_node,
    const url::Origin& origin) {
  int worker_process_id =
      process_node->GetRenderProcessHostId().GetUnsafeValue();

  // Create a new SharedWorkerToken for the worker and add it to the map.
  const blink::SharedWorkerToken shared_worker_token;

  bool inserted =
      shared_worker_client_frames_.insert({shared_worker_token, {}}).second;
  DCHECK(inserted);

  // Notify observer.
  observer_->OnWorkerCreated(shared_worker_token, worker_process_id, origin,
                             base::UnguessableToken::Create());

  return shared_worker_token;
}

void PerformanceManagerTestHarness::SharedWorkerFactory::DestroySharedWorker(
    const blink::SharedWorkerToken& shared_worker_token) {
  auto it = shared_worker_client_frames_.find(shared_worker_token);
  CHECK(it != shared_worker_client_frames_.end());

  // The worker should no longer have any clients.
  DCHECK(it->second.empty());

  // Notify observers that the worker is being destroyed.
  observer_->OnBeforeWorkerDestroyed(shared_worker_token);

  // Remove the worker ID from the map.
  shared_worker_client_frames_.erase(it);
}

void PerformanceManagerTestHarness::SharedWorkerFactory::AddClient(
    const blink::SharedWorkerToken& shared_worker_token,
    content::GlobalRenderFrameHostId client_render_frame_host_id) {
  // Add the frame to the set of clients for this worker.
  auto it = shared_worker_client_frames_.find(shared_worker_token);
  CHECK(it != shared_worker_client_frames_.end());

  base::flat_set<content::GlobalRenderFrameHostId>& client_frames = it->second;
  bool inserted = client_frames.insert(client_render_frame_host_id).second;
  DCHECK(inserted);

  // Then notify observer.
  observer_->OnClientAdded(shared_worker_token, client_render_frame_host_id);
}

void PerformanceManagerTestHarness::SharedWorkerFactory::RemoveClient(
    const blink::SharedWorkerToken& shared_worker_token,
    content::GlobalRenderFrameHostId client_render_frame_host_id) {
  // Notify observers.
  observer_->OnClientRemoved(shared_worker_token, client_render_frame_host_id);

  // Then remove the frame from the set of clients of this worker.
  auto it = shared_worker_client_frames_.find(shared_worker_token);
  CHECK(it != shared_worker_client_frames_.end());

  base::flat_set<content::GlobalRenderFrameHostId>& client_frames = it->second;
  size_t removed = client_frames.erase(client_render_frame_host_id);
  DCHECK_EQ(removed, 1u);
}

// ServiceWorkerFactory --------------------------------------------------------

PerformanceManagerTestHarness::ServiceWorkerFactory::ServiceWorkerFactory(
    content::ServiceWorkerContextObserver* observer)
    : observer_(observer) {}

PerformanceManagerTestHarness::ServiceWorkerFactory::~ServiceWorkerFactory() {
  CHECK(service_worker_infos_.empty());
}

// static
GURL PerformanceManagerTestHarness::ServiceWorkerFactory::GenerateWorkerUrl() {
  static int next_id = 0;
  return GURL(
      base::StringPrintf("https://www.foo.org/worker_script_%d.js", next_id++));
}

// static
std::string
PerformanceManagerTestHarness::ServiceWorkerFactory::GenerateClientUuid() {
  return base::Uuid::GenerateRandomV4().AsLowercaseString();
}

int64_t
PerformanceManagerTestHarness::ServiceWorkerFactory::CreateServiceWorker() {
  // Create a new version ID and add it to the map.
  int64_t version_id = next_service_worker_instance_id_++;

  bool inserted =
      service_worker_infos_.insert({version_id, ServiceWorkerInfo()}).second;
  DCHECK(inserted);

  return version_id;
}

void PerformanceManagerTestHarness::ServiceWorkerFactory::DestroyServiceWorker(
    int64_t version_id) {
  auto it = service_worker_infos_.find(version_id);
  CHECK(it != service_worker_infos_.end());
  const ServiceWorkerInfo& info = it->second;

  // Can only delete a service worker that isn't running and has no clients.
  DCHECK(!info.is_running);
  DCHECK(info.clients.empty());

  // Remove the worker instance from the map.
  service_worker_infos_.erase(it);
}

blink::ServiceWorkerToken
PerformanceManagerTestHarness::ServiceWorkerFactory::StartServiceWorker(
    int64_t version_id,
    const ProcessNode* process_node,
    const GURL& worker_url,
    const GURL& scope_url) {
  int worker_process_id =
      process_node->GetRenderProcessHostId().GetUnsafeValue();

  // Create a new token for the worker.
  blink::ServiceWorkerToken token;

  auto it = service_worker_infos_.find(version_id);
  CHECK(it != service_worker_infos_.end());
  ServiceWorkerInfo& info = it->second;

  DCHECK(!info.is_running);
  info.is_running = true;

  // Notify observer.
  observer_->OnVersionStartedRunning(
      version_id,
      content::ServiceWorkerRunningInfo(
          worker_url, scope_url,
          blink::StorageKey::CreateFirstParty(url::Origin::Create(scope_url)),
          worker_process_id, token,
          content::ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::
              kActivated));

  return token;
}

void PerformanceManagerTestHarness::ServiceWorkerFactory::StopServiceWorker(
    int64_t version_id) {
  auto it = service_worker_infos_.find(version_id);
  CHECK(it != service_worker_infos_.end());
  ServiceWorkerInfo& info = it->second;

  DCHECK(info.is_running);
  info.is_running = false;

  // Notify the observer that the worker is terminating.
  observer_->OnVersionStoppedRunning(version_id);
}

std::string PerformanceManagerTestHarness::ServiceWorkerFactory::AddClient(
    int64_t version_id,
    const FrameNode* frame_node,
    std::string client_uuid) {
  AddClientImpl(
      version_id, client_uuid,
      frame_node->GetRenderFrameHostProxy().global_frame_routing_id());
  return client_uuid;
}

std::string PerformanceManagerTestHarness::ServiceWorkerFactory::AddClient(
    int64_t version_id,
    const WorkerNode* worker_node,
    std::string client_uuid) {
  // Get the worker-type specific token. Service workers can't be clients of
  // shared workers.
  std::optional<content::ServiceWorkerClientInfo> service_worker_client_info =
      worker_node->GetWorkerToken().Visit(absl::Overload(
          [](const blink::ServiceWorkerToken& service_worker_token)
              -> std::optional<content::ServiceWorkerClientInfo> {
            return std::nullopt;
          },
          [](const auto& token)
              -> std::optional<content::ServiceWorkerClientInfo> {
            return token;
          }));
  if (!service_worker_client_info.has_value()) {
    return std::string();
  }
  AddClientImpl(version_id, client_uuid, service_worker_client_info.value());
  return client_uuid;
}

void PerformanceManagerTestHarness::ServiceWorkerFactory::RemoveClient(
    int64_t version_id,
    const std::string& client_uuid) {
  auto it = service_worker_infos_.find(version_id);
  CHECK(it != service_worker_infos_.end());
  ServiceWorkerInfo& info = it->second;

  size_t removed = info.clients.erase(client_uuid);
  DCHECK_EQ(removed, 1u);

  observer_->OnControlleeRemoved(version_id, client_uuid);
}

void PerformanceManagerTestHarness::ServiceWorkerFactory::
    OnControlleeNavigationCommitted(int64_t version_id,
                                    const std::string& client_uuid,
                                    const FrameNode* frame_node) {
  content::GlobalRenderFrameHostId render_frame_host_id =
      frame_node->GetRenderFrameHostProxy().global_frame_routing_id();

  auto it = service_worker_infos_.find(version_id);
  CHECK(it != service_worker_infos_.end());
  ServiceWorkerInfo& info = it->second;

  DCHECK(base::Contains(info.clients, client_uuid));

  observer_->OnControlleeNavigationCommitted(version_id, client_uuid,
                                             render_frame_host_id);
}

void PerformanceManagerTestHarness::ServiceWorkerFactory::AddClientImpl(
    int64_t version_id,
    std::string client_uuid,
    const content::ServiceWorkerClientInfo& client_info) {
  auto it = service_worker_infos_.find(version_id);
  CHECK(it != service_worker_infos_.end());
  ServiceWorkerInfo& info = it->second;

  bool inserted = info.clients.insert(client_uuid).second;
  DCHECK(inserted);

  observer_->OnControlleeAdded(version_id, client_uuid, client_info);
}

PerformanceManagerTestHarness::ServiceWorkerFactory::ServiceWorkerInfo::
    ServiceWorkerInfo() = default;

PerformanceManagerTestHarness::ServiceWorkerFactory::ServiceWorkerInfo::
    ~ServiceWorkerInfo() = default;

PerformanceManagerTestHarness::ServiceWorkerFactory::ServiceWorkerInfo::
    ServiceWorkerInfo(ServiceWorkerInfo&& other) = default;

PerformanceManagerTestHarness::ServiceWorkerFactory::ServiceWorkerInfo&
PerformanceManagerTestHarness::ServiceWorkerFactory::ServiceWorkerInfo::
operator=(ServiceWorkerInfo&& other) = default;

}  // namespace performance_manager

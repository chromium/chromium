// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/service_worker_context_adapter.h"

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"

namespace performance_manager {

// ServiceWorkerContextAdapter::RunningServiceWorker ---------------------------

// Observes when the render process of a running service worker exits and
// notifies its owner.
class ServiceWorkerContextAdapter::RunningServiceWorker
    : content::RenderProcessHostObserver {
 public:
  RunningServiceWorker(int64_t version_id,
                       ServiceWorkerContextAdapter* adapter);
  ~RunningServiceWorker() override;

  void Subscribe(content::RenderProcessHost* worker_process_host);
  void Unsubscribe();

  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

 private:
  // The version ID of the service worker.
  int const version_id_;

  // The adapter that owns |this|. Notified when RenderProcessExited() is
  // called.
  const raw_ptr<ServiceWorkerContextAdapter> adapter_;

  base::ScopedObservation<content::RenderProcessHost,
                          content::RenderProcessHostObserver>
      scoped_observation_{this};
};

ServiceWorkerContextAdapter::RunningServiceWorker::RunningServiceWorker(
    int64_t version_id,
    ServiceWorkerContextAdapter* adapter)
    : version_id_(version_id), adapter_(adapter) {}

ServiceWorkerContextAdapter::RunningServiceWorker::~RunningServiceWorker() {
  DCHECK(!scoped_observation_.IsObserving());
}

void ServiceWorkerContextAdapter::RunningServiceWorker::Subscribe(
    content::RenderProcessHost* worker_process_host) {
  DCHECK(!scoped_observation_.IsObserving());
  scoped_observation_.Observe(worker_process_host);
}

void ServiceWorkerContextAdapter::RunningServiceWorker::Unsubscribe() {
  DCHECK(scoped_observation_.IsObserving());

  scoped_observation_.Reset();
}

void ServiceWorkerContextAdapter::RunningServiceWorker::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  adapter_->OnRenderProcessExited(version_id_);

  /* This object is deleted inside the above, don't touch "this". */
}

void ServiceWorkerContextAdapter::RunningServiceWorker::
    RenderProcessHostDestroyed(content::RenderProcessHost* host) {
  NOTREACHED();
}

// ServiceWorkerContextAdapter::RunningServiceWorker ---------------------------

ServiceWorkerContextAdapter::ServiceWorkerContextAdapter(
    content::ServiceWorkerContext* underlying_context) {
  scoped_underlying_context_observation_.Observe(underlying_context);
}

ServiceWorkerContextAdapter::~ServiceWorkerContextAdapter() {
  // Clean up any outstanding running service worker process subscriptions.
  for (const auto& item : running_service_workers_)
    item.second->Unsubscribe();
  running_service_workers_.clear();
}

void ServiceWorkerContextAdapter::AddObserver(
    content::ServiceWorkerContextObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ServiceWorkerContextAdapter::RemoveObserver(
    content::ServiceWorkerContextObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void ServiceWorkerContextAdapter::RegisterServiceWorker(
    const GURL& script_url,
    const blink::StorageKey& key,
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    StatusCodeCallback callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::UnregisterServiceWorker(
    const GURL& scope,
    const blink::StorageKey& key,
    ResultCallback callback) {
  NOTIMPLEMENTED();
}

content::ServiceWorkerExternalRequestResult
ServiceWorkerContextAdapter::StartingExternalRequest(
    int64_t service_worker_version_id,
    content::ServiceWorkerExternalRequestTimeoutType timeout_type,
    const base::Uuid& request_uuid) {
  NOTIMPLEMENTED();
  return content::ServiceWorkerExternalRequestResult::kOk;
}

content::ServiceWorkerExternalRequestResult
ServiceWorkerContextAdapter::FinishedExternalRequest(
    int64_t service_worker_version_id,
    const base::Uuid& request_uuid) {
  NOTIMPLEMENTED();
  return content::ServiceWorkerExternalRequestResult::kOk;
}

size_t ServiceWorkerContextAdapter::CountExternalRequestsForTest(
    const blink::StorageKey& key) {
  NOTIMPLEMENTED();
  return 0u;
}

bool ServiceWorkerContextAdapter::ExecuteScriptForTest(
    const std::string& script,
    int64_t version_id,
    content::ServiceWorkerScriptExecutionCallback callback) {
  NOTIMPLEMENTED();
  return false;
}

bool ServiceWorkerContextAdapter::MaybeHasRegistrationForStorageKey(
    const blink::StorageKey& key) {
  NOTIMPLEMENTED();
  return false;
}

void ServiceWorkerContextAdapter::GetAllStorageKeysInfo(
    GetUsageInfoCallback callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::DeleteForStorageKey(
    const blink::StorageKey& key,
    ResultCallback callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::CheckHasServiceWorker(
    const GURL& url,
    const blink::StorageKey& key,
    CheckHasServiceWorkerCallback callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::CheckOfflineCapability(
    const GURL& url,
    const blink::StorageKey& key,
    CheckOfflineCapabilityCallback callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::ClearAllServiceWorkersForTest(
    base::OnceClosure callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::StartWorkerForScope(
    const GURL& scope,
    const blink::StorageKey& key,
    StartWorkerCallback info_callback,
    StatusCodeCallback status_callback) {
  NOTIMPLEMENTED();
}

bool ServiceWorkerContextAdapter::IsLiveRunningServiceWorker(
    int64_t service_worker_version_id) {
  NOTIMPLEMENTED();
  return false;
}

service_manager::InterfaceProvider&
ServiceWorkerContextAdapter::GetRemoteInterfaces(
    int64_t service_worker_version_id) {
  NOTIMPLEMENTED();
  static service_manager::InterfaceProvider interface_provider(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  return interface_provider;
}

void ServiceWorkerContextAdapter::StartServiceWorkerAndDispatchMessage(
    const GURL& scope,
    const blink::StorageKey& key,
    blink::TransferableMessage message,
    ResultCallback result_callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::StartServiceWorkerForNavigationHint(
    const GURL& document_url,
    const blink::StorageKey& key,
    StartServiceWorkerForNavigationHintCallback callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::StopAllServiceWorkersForStorageKey(
    const blink::StorageKey& key) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::StopAllServiceWorkers(
    base::OnceClosure callback) {
  NOTIMPLEMENTED();
}

const base::flat_map<int64_t /* version_id */,
                     content::ServiceWorkerRunningInfo>&
ServiceWorkerContextAdapter::GetRunningServiceWorkerInfos() {
  NOTIMPLEMENTED();
  static base::flat_map<int64_t /* version_id */,
                        content::ServiceWorkerRunningInfo>
      unused;
  return unused;
}

void ServiceWorkerContextAdapter::OnRegistrationCompleted(const GURL& scope) {
  for (auto& observer : observer_list_)
    observer.OnRegistrationCompleted(scope);
}

void ServiceWorkerContextAdapter::OnRegistrationStored(int64_t registration_id,
                                                       const GURL& scope) {
  for (auto& observer : observer_list_)
    observer.OnRegistrationStored(registration_id, scope);
}

void ServiceWorkerContextAdapter::OnVersionActivated(int64_t version_id,
                                                     const GURL& scope) {
  for (auto& observer : observer_list_)
    observer.OnVersionActivated(version_id, scope);
}

void ServiceWorkerContextAdapter::OnVersionRedundant(int64_t version_id,
                                                     const GURL& scope) {
  for (auto& observer : observer_list_)
    observer.OnVersionRedundant(version_id, scope);
}

void ServiceWorkerContextAdapter::OnVersionStartedRunning(
    int64_t version_id,
    const content::ServiceWorkerRunningInfo& running_info) {
  content::RenderProcessHost* worker_process_host =
      content::RenderProcessHost::FromID(running_info.render_process_id);

  // It's possible that the renderer is already gone since the notification
  // comes asynchronously. Ignore this service worker.
  if (!worker_process_host || !worker_process_host->IsInitializedAndNotDead()) {
#if DCHECK_IS_ON()
    // A OnVersionStoppedRunning() notification is still expected to be sent.
    bool inserted = stopped_service_workers_.insert(version_id).second;
    DCHECK(inserted);
#endif  // DCHECK_IS_ON()
    return;
  }

  AddRunningServiceWorker(version_id, worker_process_host);
  for (auto& observer : observer_list_)
    observer.OnVersionStartedRunning(version_id, running_info);
}

void ServiceWorkerContextAdapter::OnVersionStoppedRunning(int64_t version_id) {
  bool removed = MaybeRemoveRunningServiceWorker(version_id);
  if (!removed) {
#if DCHECK_IS_ON()
    // If this service worker could not be found, then it must be because its
    // render process exited early.
    size_t removed_count = stopped_service_workers_.erase(version_id);
    DCHECK_EQ(removed_count, 1u);
#endif  // DCHECK_IS_ON()
    return;
  }

  for (auto& observer : observer_list_)
    observer.OnVersionStoppedRunning(version_id);
}

void ServiceWorkerContextAdapter::OnControlleeAdded(
    int64_t version_id,
    const std::string& client_uuid,
    const content::ServiceWorkerClientInfo& client_info) {
  // If |client_uuid| is already marked as a client of |version_id|, the
  // notification is dropped.
  bool inserted =
      service_worker_clients_[version_id].insert(client_uuid).second;
  if (!inserted) {
    NOTREACHED();
    return;
  }

  for (auto& observer : observer_list_)
    observer.OnControlleeAdded(version_id, client_uuid, client_info);
}

void ServiceWorkerContextAdapter::OnControlleeRemoved(
    int64_t version_id,
    const std::string& client_uuid) {
  // If |client_uuid| is not already marked as a client of |version_id|, the
  // notification is dropped.
  auto it = service_worker_clients_.find(version_id);
  if (it == service_worker_clients_.end()) {
    NOTREACHED();
    return;
  }

  size_t removed = it->second.erase(client_uuid);
  if (!removed) {
    NOTREACHED();
    return;
  }

  // If a service worker no longer has any clients, it is removed entirely from
  // |service_worker_clients_|.
  if (it->second.empty())
    service_worker_clients_.erase(it);

  for (auto& observer : observer_list_)
    observer.OnControlleeRemoved(version_id, client_uuid);
}

void ServiceWorkerContextAdapter::OnNoControllees(int64_t version_id,
                                                  const GURL& scope) {
  for (auto& observer : observer_list_)
    observer.OnNoControllees(version_id, scope);
}

void ServiceWorkerContextAdapter::OnControlleeNavigationCommitted(
    int64_t version_id,
    const std::string& client_uuid,
    content::GlobalRenderFrameHostId render_frame_host_id) {
  // The navigation committed notification should not be sent if the frame is
  // not already a client of |version_id|.
  auto it = service_worker_clients_.find(version_id);
  if (it == service_worker_clients_.end()) {
    NOTREACHED();
    return;
  }

  if (it->second.find(client_uuid) == it->second.end()) {
    NOTREACHED();
    return;
  }

  for (auto& observer : observer_list_)
    observer.OnControlleeNavigationCommitted(version_id, client_uuid,
                                             render_frame_host_id);
}

void ServiceWorkerContextAdapter::OnReportConsoleMessage(
    int64_t version_id,
    const GURL& scope,
    const content::ConsoleMessage& message) {
  for (auto& observer : observer_list_)
    observer.OnReportConsoleMessage(version_id, scope, message);
}

void ServiceWorkerContextAdapter::OnDestruct(ServiceWorkerContext* context) {
  for (auto& observer : observer_list_)
    observer.OnDestruct(context);
}

void ServiceWorkerContextAdapter::OnRenderProcessExited(int64_t version_id) {
  bool removed = MaybeRemoveRunningServiceWorker(version_id);
  DCHECK(removed);

  for (auto& observer : observer_list_)
    observer.OnVersionStoppedRunning(version_id);

#if DCHECK_IS_ON()
  // Now expect that OnVersionStoppedRunning() will be called for that
  // version_id.
  bool inserted = stopped_service_workers_.insert(version_id).second;
  DCHECK(inserted);
#endif  // DCHECK_IS_ON()
}

void ServiceWorkerContextAdapter::AddRunningServiceWorker(
    int64_t version_id,
    content::RenderProcessHost* worker_process_host) {
  std::unique_ptr<ServiceWorkerContextAdapter::RunningServiceWorker>
      running_service_worker =
          std::make_unique<RunningServiceWorker>(version_id, this);

  running_service_worker->Subscribe(worker_process_host);
  bool inserted = running_service_workers_
                      .emplace(version_id, std::move(running_service_worker))
                      .second;
  DCHECK(inserted);
}

bool ServiceWorkerContextAdapter::MaybeRemoveRunningServiceWorker(
    int64_t version_id) {
  auto it = running_service_workers_.find(version_id);
  if (it == running_service_workers_.end())
    return false;

  it->second->Unsubscribe();
  running_service_workers_.erase(it);

  return true;
}

}  // namespace performance_manager

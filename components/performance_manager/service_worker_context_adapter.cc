// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/service_worker_context_adapter.h"

#include "base/check_op.h"
#include "base/notreached.h"
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
                       content::RenderProcessHost* worker_process_host,
                       ServiceWorkerContextAdapter* adapter);
  ~RunningServiceWorker() override;

  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

 private:
  // The version ID of the service worker.
  int version_id_;

  // The adapter that owns |this|. Notified when RenderProcessExited() is
  // called.
  ServiceWorkerContextAdapter* const adapter_;

  ScopedObserver<content::RenderProcessHost, content::RenderProcessHostObserver>
      scoped_render_process_host_observer_{this};
};

ServiceWorkerContextAdapter::RunningServiceWorker::RunningServiceWorker(
    int64_t version_id,
    content::RenderProcessHost* worker_process_host,
    ServiceWorkerContextAdapter* adapter)
    : version_id_(version_id), adapter_(adapter) {
  scoped_render_process_host_observer_.Add(worker_process_host);
}

ServiceWorkerContextAdapter::RunningServiceWorker::~RunningServiceWorker() =
    default;

void ServiceWorkerContextAdapter::RunningServiceWorker::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  adapter_->OnRenderProcessExited(version_id_);
}

void ServiceWorkerContextAdapter::RunningServiceWorker::
    RenderProcessHostDestroyed(content::RenderProcessHost* host) {
  NOTREACHED();
}

// ServiceWorkerContextAdapter::RunningServiceWorker ---------------------------

ServiceWorkerContextAdapter::ServiceWorkerContextAdapter(
    content::ServiceWorkerContext* underlying_context) {
  scoped_underlying_context_observer_.Add(underlying_context);
}

ServiceWorkerContextAdapter::~ServiceWorkerContextAdapter() = default;

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
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    ResultCallback callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::UnregisterServiceWorker(
    const GURL& scope,
    ResultCallback callback) {
  NOTIMPLEMENTED();
}

content::ServiceWorkerExternalRequestResult
ServiceWorkerContextAdapter::StartingExternalRequest(
    int64_t service_worker_version_id,
    const std::string& request_uuid) {
  NOTIMPLEMENTED();
  return content::ServiceWorkerExternalRequestResult::kOk;
}

content::ServiceWorkerExternalRequestResult
ServiceWorkerContextAdapter::FinishedExternalRequest(
    int64_t service_worker_version_id,
    const std::string& request_uuid) {
  NOTIMPLEMENTED();
  return content::ServiceWorkerExternalRequestResult::kOk;
}

void ServiceWorkerContextAdapter::CountExternalRequestsForTest(
    const url::Origin& origin,
    CountExternalRequestsCallback callback) {
  NOTIMPLEMENTED();
}

bool ServiceWorkerContextAdapter::MaybeHasRegistrationForOrigin(
    const url::Origin& origin) {
  NOTIMPLEMENTED();
  return false;
}

void ServiceWorkerContextAdapter::GetInstalledRegistrationOrigins(
    base::Optional<std::string> host_filter,
    GetInstalledRegistrationOriginsCallback callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::GetAllOriginsInfo(
    GetUsageInfoCallback callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::DeleteForOrigin(const url::Origin& origin_url,
                                                  ResultCallback callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::PerformStorageCleanup(
    base::OnceClosure callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::CheckHasServiceWorker(
    const GURL& url,
    CheckHasServiceWorkerCallback callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::CheckOfflineCapability(
    const GURL& url,
    CheckOfflineCapabilityCallback callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::ClearAllServiceWorkersForTest(
    base::OnceClosure callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::StartWorkerForScope(
    const GURL& scope,
    StartWorkerCallback info_callback,
    base::OnceClosure failure_callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::StartServiceWorkerAndDispatchMessage(
    const GURL& scope,
    blink::TransferableMessage message,
    ResultCallback result_callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::StartServiceWorkerForNavigationHint(
    const GURL& document_url,
    StartServiceWorkerForNavigationHintCallback callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextAdapter::StopAllServiceWorkersForOrigin(
    const url::Origin& origin) {
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
  auto* worker_process_host =
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

  bool inserted =
      running_service_workers_
          .emplace(version_id, std::make_unique<RunningServiceWorker>(
                                   version_id, worker_process_host, this))
          .second;
  DCHECK(inserted);

  for (auto& observer : observer_list_)
    observer.OnVersionStartedRunning(version_id, running_info);
}

void ServiceWorkerContextAdapter::OnVersionStoppedRunning(int64_t version_id) {
  size_t removed = running_service_workers_.erase(version_id);
  if (!removed) {
#if DCHECK_IS_ON()
    // If this service worker could not be found, then it must be because its
    // render process exited early.
    size_t removed = stopped_service_workers_.erase(version_id);
    DCHECK_EQ(removed, 1u);
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
    content::GlobalFrameRoutingId render_frame_host_id) {
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
  size_t removed = running_service_workers_.erase(version_id);
  DCHECK_EQ(removed, 1u);

  for (auto& observer : observer_list_)
    observer.OnVersionStoppedRunning(version_id);

#if DCHECK_IS_ON()
  // Now expect that OnVersionStoppedRunning() will be called for that
  // version_id.
  bool inserted = stopped_service_workers_.insert(version_id).second;
  DCHECK(inserted);
#endif  // DCHECK_IS_ON()
}

}  // namespace performance_manager

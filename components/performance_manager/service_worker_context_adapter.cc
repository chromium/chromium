// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/service_worker_context_adapter.h"

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace performance_manager {

// ServiceWorkerContextAdapterImpl::RunningServiceWorker -----------------------

// Observes when the render process of a running service worker exits and
// notifies its owner.
class ServiceWorkerContextAdapterImpl::RunningServiceWorker
    : content::RenderProcessHostObserver {
 public:
  RunningServiceWorker(int64_t version_id,
                       ServiceWorkerContextAdapterImpl* adapter);
  ~RunningServiceWorker() override;

  void Subscribe(content::RenderProcessHost* worker_process_host);
  void Unsubscribe();

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void InProcessRendererExiting(content::RenderProcessHost* host) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

 private:
  // The version ID of the service worker.
  int const version_id_;

  // The adapter that owns |this|. Notified when RenderProcessExited() is
  // called.
  const raw_ptr<ServiceWorkerContextAdapterImpl> adapter_;

  base::ScopedObservation<content::RenderProcessHost,
                          content::RenderProcessHostObserver>
      scoped_observation_{this};
};

ServiceWorkerContextAdapterImpl::RunningServiceWorker::RunningServiceWorker(
    int64_t version_id,
    ServiceWorkerContextAdapterImpl* adapter)
    : version_id_(version_id), adapter_(adapter) {}

ServiceWorkerContextAdapterImpl::RunningServiceWorker::~RunningServiceWorker() {
  DCHECK(!scoped_observation_.IsObserving());
}

void ServiceWorkerContextAdapterImpl::RunningServiceWorker::Subscribe(
    content::RenderProcessHost* worker_process_host) {
  DCHECK(!scoped_observation_.IsObserving());
  scoped_observation_.Observe(worker_process_host);
}

void ServiceWorkerContextAdapterImpl::RunningServiceWorker::Unsubscribe() {
  DCHECK(scoped_observation_.IsObserving());

  scoped_observation_.Reset();
}

void ServiceWorkerContextAdapterImpl::RunningServiceWorker::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  adapter_->OnRenderProcessExited(version_id_);

  /* This object is deleted inside the above, don't touch "this". */
}

void ServiceWorkerContextAdapterImpl::RunningServiceWorker::
    InProcessRendererExiting(content::RenderProcessHost* host) {
  CHECK(content::RenderProcessHost::run_renderer_in_process());
  adapter_->OnRenderProcessExited(version_id_);

  /* This object is deleted inside the above, don't touch "this". */
}

void ServiceWorkerContextAdapterImpl::RunningServiceWorker::
    RenderProcessHostDestroyed(content::RenderProcessHost* host) {
  NOTREACHED();
}

// ServiceWorkerContextAdapterImpl ---------------------------------------------

ServiceWorkerContextAdapterImpl::ServiceWorkerContextAdapterImpl(
    content::ServiceWorkerContext* underlying_context) {
  scoped_underlying_context_observation_.Observe(underlying_context);
}

ServiceWorkerContextAdapterImpl::~ServiceWorkerContextAdapterImpl() {
  // Clean up any outstanding running service worker process subscriptions.
  for (const auto& item : running_service_workers_)
    item.second->Unsubscribe();
  running_service_workers_.clear();
}

void ServiceWorkerContextAdapterImpl::AddObserver(
    content::ServiceWorkerContextObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ServiceWorkerContextAdapterImpl::RemoveObserver(
    content::ServiceWorkerContextObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void ServiceWorkerContextAdapterImpl::OnVersionStartedRunning(
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

void ServiceWorkerContextAdapterImpl::OnVersionStoppedRunning(
    int64_t version_id) {
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

void ServiceWorkerContextAdapterImpl::OnControlleeAdded(
    int64_t version_id,
    const std::string& client_uuid,
    const content::ServiceWorkerClientInfo& client_info) {
  // If |client_uuid| is already marked as a client of |version_id|, the
  // notification is dropped.
  bool inserted =
      service_worker_clients_[version_id].insert(client_uuid).second;
  if (!inserted) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  for (auto& observer : observer_list_)
    observer.OnControlleeAdded(version_id, client_uuid, client_info);
}

void ServiceWorkerContextAdapterImpl::OnControlleeRemoved(
    int64_t version_id,
    const std::string& client_uuid) {
  // If |client_uuid| is not already marked as a client of |version_id|, the
  // notification is dropped.
  auto it = service_worker_clients_.find(version_id);
  if (it == service_worker_clients_.end()) {
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  size_t removed = it->second.erase(client_uuid);
  if (!removed) {
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  // If a service worker no longer has any clients, it is removed entirely from
  // |service_worker_clients_|.
  if (it->second.empty())
    service_worker_clients_.erase(it);

  for (auto& observer : observer_list_)
    observer.OnControlleeRemoved(version_id, client_uuid);
}

void ServiceWorkerContextAdapterImpl::OnControlleeNavigationCommitted(
    int64_t version_id,
    const std::string& client_uuid,
    content::GlobalRenderFrameHostId render_frame_host_id) {
  // The navigation committed notification should not be sent if the frame is
  // not already a client of |version_id|.
  auto it = service_worker_clients_.find(version_id);
  if (it == service_worker_clients_.end()) {
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  if (it->second.find(client_uuid) == it->second.end()) {
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  for (auto& observer : observer_list_)
    observer.OnControlleeNavigationCommitted(version_id, client_uuid,
                                             render_frame_host_id);
}

void ServiceWorkerContextAdapterImpl::OnRenderProcessExited(
    int64_t version_id) {
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

void ServiceWorkerContextAdapterImpl::AddRunningServiceWorker(
    int64_t version_id,
    content::RenderProcessHost* worker_process_host) {
  std::unique_ptr<ServiceWorkerContextAdapterImpl::RunningServiceWorker>
      running_service_worker =
          std::make_unique<RunningServiceWorker>(version_id, this);

  running_service_worker->Subscribe(worker_process_host);
  bool inserted = running_service_workers_
                      .emplace(version_id, std::move(running_service_worker))
                      .second;
  DCHECK(inserted);
}

bool ServiceWorkerContextAdapterImpl::MaybeRemoveRunningServiceWorker(
    int64_t version_id) {
  auto it = running_service_workers_.find(version_id);
  if (it == running_service_workers_.end())
    return false;

  it->second->Unsubscribe();
  running_service_workers_.erase(it);

  return true;
}

}  // namespace performance_manager

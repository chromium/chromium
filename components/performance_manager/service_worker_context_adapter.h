// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SERVICE_WORKER_CONTEXT_ADAPTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SERVICE_WORKER_CONTEXT_ADAPTER_H_

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"

namespace content {
class RenderProcessHost;
}

namespace performance_manager {

// Interface that makes testing easier. The only practical implementaion is the
// class below.
class ServiceWorkerContextAdapter {
 public:
  virtual ~ServiceWorkerContextAdapter() = default;

  // Adds/removes an observer.
  virtual void AddObserver(content::ServiceWorkerContextObserver* observer) = 0;
  virtual void RemoveObserver(
      content::ServiceWorkerContextObserver* observer) = 0;
};

// This class adapts an existing ServiceWorkerContext to ensure that the
// OnVersionStoppedRunning() notifications are sent as soon as the render
// process of a running service worker exits.
//
// It observes the underlying context so that it can receive the original
// notifications and control when they are sent to the observers.
//
// Lives on the UI thread. Must outlive |underlying_context|.
//
// Note: This is a temporary class that can be removed when the representation
//       of a worker in the content/ layer (ServiceWorkerVersion) is moved to
//       the UI thread. At that point, it'll be able to observe its associated
//       RenderProcessHost itself. See https://crbug.com/824858.
class ServiceWorkerContextAdapterImpl
    : public ServiceWorkerContextAdapter,
      public content::ServiceWorkerContextObserver {
 public:
  explicit ServiceWorkerContextAdapterImpl(
      content::ServiceWorkerContext* underlying_context);
  ~ServiceWorkerContextAdapterImpl() override;

  // ServiceWorkerContextAdapter:
  // Note that this is a minimal implementation for the use case of the
  // PerformanceManager. Only AddObserver/RemoveObserver are implemented.
  void AddObserver(content::ServiceWorkerContextObserver* observer) override;
  void RemoveObserver(content::ServiceWorkerContextObserver* observer) override;

  // content::ServiceWorkerContextObserver:
  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override;
  void OnVersionStoppedRunning(int64_t version_id) override;
  void OnControlleeAdded(
      int64_t version_id,
      const std::string& client_uuid,
      const content::ServiceWorkerClientInfo& client_info) override;
  void OnControlleeRemoved(int64_t version_id,
                           const std::string& client_uuid) override;
  void OnControlleeNavigationCommitted(
      int64_t version_id,
      const std::string& uuid,
      content::GlobalRenderFrameHostId render_frame_host_id) override;

 private:
  class RunningServiceWorker;

  // Invoked by a RunningServiceWorker when it observes that the render process
  // has exited.
  void OnRenderProcessExited(int64_t version_id);

  // Adds a registration to |worker_process_host| that will result in
  // |OnRenderProcessExited| with |version_id| when it exits.
  void AddRunningServiceWorker(int64_t version_id,
                               content::RenderProcessHost* worker_process_host);

  // Removes a registration made by |AddRunningServiceWorker| if one exists,
  // returns true if a registration existed, false otherwise.
  bool MaybeRemoveRunningServiceWorker(int64_t version_id);

  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      scoped_underlying_context_observation_{this};

  base::ObserverList<content::ServiceWorkerContextObserver, true, false>::
      Unchecked observer_list_;

  // For each running service worker, tracks when their render process exits.
  base::flat_map<int64_t /*version_id*/, std::unique_ptr<RunningServiceWorker>>
      running_service_workers_;

  // Tracks the OnControlleeAdded and OnControlleeRemoved notification for each
  // service worker, with the goal of cleaning up duplicate notifications for
  // observers of this class.
  // TODO(crbug.com/40653867): Fix the underlying code in
  // content/browser/service_worker so
  //                that duplicate notifications are no longer sent.
  base::flat_map<int64_t /*version_id*/,
                 base::flat_set<std::string /*client_uuid*/>>
      service_worker_clients_;

#if DCHECK_IS_ON()
  // Keeps track of service worker whose render process exited early.
  base::flat_set<int64_t> stopped_service_workers_;
#endif  // DCHECK_IS_ON()
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SERVICE_WORKER_CONTEXT_ADAPTER_H_

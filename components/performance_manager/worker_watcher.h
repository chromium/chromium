// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_WORKER_WATCHER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_WORKER_WATCHER_H_

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/service_worker_client.h"
#include "content/public/browser/dedicated_worker_service.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/shared_worker_service.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {

class FrameNodeImpl;
class FrameNodeSource;
class ProcessNodeSource;
class WorkerNodeImpl;

// This class keeps track of running workers of all types for a single browser
// context and handles the ownership of the worker nodes.
//
// Most of the complexity of this class is tracking every worker's clients. Each
// type of worker handles them a bit differently.
//
// The simplest case is dedicated workers, where each worker always has exactly
// one frame client. Technically, it is possible to create a nested dedicated
// worker, but for now they are treated as child of the ancestor frame.
// TODO(1128645): Expose nested dedicated workers correctly.
//
// Shared workers are quite similar to dedicated workers but they can have any
// number of clients. Also, a shared worker can temporarily appear to have no
// clients shortly after being created and just before being destroyed.
//
// Service workers are more complicated to handle. They also can have any number
// of clients, but they aren't only frames. They could also be dedicated worker
// and shared worker clients. These different types of clients are tracked using
// the ServiceWorkerClient class. Also, because of the important role the
// service worker plays with frame navigations, the service worker can be
// created before its first client's navigation has committed to a
// RenderFrameHost. So when a OnControlleeAdded() notification is received for
// a client frame, it is necessary to wait until the render frame host was
// determined.
class WorkerWatcher : public content::DedicatedWorkerService::Observer,
                      public content::SharedWorkerService::Observer,
                      public content::ServiceWorkerContextObserver {
 public:
  WorkerWatcher(const std::string& browser_context_id,
                content::DedicatedWorkerService* dedicated_worker_service,
                content::SharedWorkerService* shared_worker_service,
                content::ServiceWorkerContext* service_worker_context,
                ProcessNodeSource* process_node_source,
                FrameNodeSource* frame_node_source);
  ~WorkerWatcher() override;

  // Cleans up this instance and ensures shared worker nodes are correctly
  // destroyed on the PM graph.
  void TearDown();

  // content::DedicatedWorkerService::Observer:
  void OnWorkerCreated(
      const blink::DedicatedWorkerToken& dedicated_worker_token,
      int worker_process_id,
      content::GlobalFrameRoutingId ancestor_render_frame_host_id) override;
  void OnBeforeWorkerDestroyed(
      const blink::DedicatedWorkerToken& dedicated_worker_token,
      content::GlobalFrameRoutingId ancestor_render_frame_host_id) override;
  void OnFinalResponseURLDetermined(
      const blink::DedicatedWorkerToken& dedicated_worker_token,
      const GURL& url) override;

  // content::SharedWorkerService::Observer:
  void OnWorkerCreated(const blink::SharedWorkerToken& shared_worker_token,
                       int worker_process_id,
                       const base::UnguessableToken& dev_tools_token) override;
  void OnBeforeWorkerDestroyed(
      const blink::SharedWorkerToken& shared_worker_token) override;
  void OnFinalResponseURLDetermined(
      const blink::SharedWorkerToken& shared_worker_token,
      const GURL& url) override;
  void OnClientAdded(
      const blink::SharedWorkerToken& shared_worker_token,
      content::GlobalFrameRoutingId render_frame_host_id) override;
  void OnClientRemoved(
      const blink::SharedWorkerToken& shared_worker_token,
      content::GlobalFrameRoutingId render_frame_host_id) override;

  // content::ServiceWorkerContextObserver:
  // Note: If you add a new function here, make sure it is also added to
  // ServiceWorkerContextAdapter.
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
      const std::string& client_uuid,
      content::GlobalFrameRoutingId render_frame_host_id) override;

 private:
  friend class WorkerWatcherTest;

  // Posts a task to the PM graph to connect/disconnect |worker_node| with the
  // frame node associated to |client_render_frame_host_id|.
  void ConnectFrameClient(
      WorkerNodeImpl* worker_node,
      content::GlobalFrameRoutingId client_render_frame_host_id);
  void DisconnectFrameClient(
      WorkerNodeImpl* worker_node,
      content::GlobalFrameRoutingId client_render_frame_host_id);

  // Posts a task to the PM graph to connect/disconnect |worker_node| with the
  // dedicated worker node associated to |client_dedicated_worker_token|.
  void ConnectDedicatedWorkerClient(
      WorkerNodeImpl* worker_node,
      blink::DedicatedWorkerToken client_dedicated_worker_token);
  void DisconnectDedicatedWorkerClient(
      WorkerNodeImpl* worker_node,
      blink::DedicatedWorkerToken client_dedicated_worker_token);

  // Posts a task to the PM graph to connect/disconnect |worker_node| with the
  // shared worker node associated to |client_shared_worker_id|.
  void ConnectSharedWorkerClient(
      WorkerNodeImpl* worker_node,
      blink::SharedWorkerToken client_shared_worker_token);
  void DisconnectSharedWorkerClient(
      WorkerNodeImpl* worker_node,
      blink::SharedWorkerToken client_shared_worker_token);

  // Posts a task to the PM graph to connect/disconnect |service_worker_node|
  // with all of its existing clients. Called when a service worker starts/stops
  // running.
  void ConnectAllServiceWorkerClients(WorkerNodeImpl* service_worker_node,
                                      int64_t version_id);
  void DisconnectAllServiceWorkerClients(WorkerNodeImpl* service_worker_node,
                                         int64_t version_id);

  void OnBeforeFrameNodeRemoved(
      content::GlobalFrameRoutingId render_frame_host_id,
      FrameNodeImpl* frame_node);

  // Inserts/removes |child_worker_node| into the set of child workers of a
  // frame. Returns true if this is the first child added to that frame.
  bool AddChildWorker(content::GlobalFrameRoutingId render_frame_host_id,
                      WorkerNodeImpl* child_worker_node);
  bool RemoveChildWorker(content::GlobalFrameRoutingId render_frame_host_id,
                         WorkerNodeImpl* child_worker_node);

  // Helper functions to retrieve an existing worker node.
  WorkerNodeImpl* GetDedicatedWorkerNode(
      const blink::DedicatedWorkerToken& dedicated_worker_token);
  WorkerNodeImpl* GetSharedWorkerNode(
      const blink::SharedWorkerToken& shared_worker_token);
  WorkerNodeImpl* GetServiceWorkerNode(int64_t version_id);

  SEQUENCE_CHECKER(sequence_checker_);

  // The ID of the BrowserContext who owns the shared worker service.
  const std::string browser_context_id_;

  // Observes the DedicatedWorkerService for this browser context.
  ScopedObserver<content::DedicatedWorkerService,
                 content::DedicatedWorkerService::Observer>
      dedicated_worker_service_observer_{this};

  // Observes the SharedWorkerService for this browser context.
  ScopedObserver<content::SharedWorkerService,
                 content::SharedWorkerService::Observer>
      shared_worker_service_observer_{this};

  ScopedObserver<content::ServiceWorkerContext,
                 content::ServiceWorkerContextObserver>
      service_worker_context_observer_{this};

  // Used to retrieve an existing process node from its render process ID.
  ProcessNodeSource* const process_node_source_;

  // Used to retrieve an existing frame node from its render process ID and
  // frame ID. Also allows to subscribe to a frame's deletion notification.
  FrameNodeSource* const frame_node_source_;

  // Maps each dedicated worker ID to its worker node.
  base::flat_map<blink::DedicatedWorkerToken, std::unique_ptr<WorkerNodeImpl>>
      dedicated_worker_nodes_;

  // Maps each shared worker ID to its worker node.
  base::flat_map<blink::SharedWorkerToken, std::unique_ptr<WorkerNodeImpl>>
      shared_worker_nodes_;

  // Maps each service worker version ID to its worker node.
  base::flat_map<int64_t /*version_id*/, std::unique_ptr<WorkerNodeImpl>>
      service_worker_nodes_;

  // Keeps track of frame clients that are awaiting the navigation commit
  // notification. Used for service workers only.
  base::flat_set<std::string /*client_uuid*/> client_frames_awaiting_commit_;

  // Maps each service worker to its clients.
  base::flat_map<
      int64_t /*version_id*/,
      base::flat_map<std::string /*client_uuid*/, ServiceWorkerClient>>
      service_worker_clients_;

  // Maps each frame to the workers that this frame is a client of in the graph.
  // This is used when a frame is torn down before the
  // OnBeforeWorkerTerminated() is received, to ensure the deletion of the
  // worker nodes in the right order (workers before frames).
  base::flat_map<content::GlobalFrameRoutingId, base::flat_set<WorkerNodeImpl*>>
      frame_node_child_workers_;

  // Maps each dedicated worker to all its child workers.
  base::flat_map<blink::DedicatedWorkerToken, base::flat_set<WorkerNodeImpl*>>
      dedicated_worker_child_workers_;

  // Maps each shared worker to all its child workers.
  base::flat_map<blink::SharedWorkerToken, base::flat_set<WorkerNodeImpl*>>
      shared_worker_child_workers_;

#if DCHECK_IS_ON()
  // Keeps track of how many OnClientRemoved() calls are expected for an
  // existing worker. This happens when OnBeforeFrameNodeRemoved() is invoked
  // before OnClientRemoved(), or when it wasn't possible to initially attach
  // a client frame node to a worker.
  base::flat_map<WorkerNodeImpl*, int> detached_frame_count_per_worker_;
#endif  // DCHECK_IS_ON()

  DISALLOW_COPY_AND_ASSIGN(WorkerWatcher);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_WORKER_WATCHER_H_

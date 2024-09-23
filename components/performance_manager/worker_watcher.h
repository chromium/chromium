// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_WORKER_WATCHER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_WORKER_WATCHER_H_

#include <map>
#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/service_worker_context_adapter.h"
#include "content/public/browser/dedicated_worker_service.h"
#include "content/public/browser/global_routing_id.h"
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
// TODO(crbug.com/40149051): Expose nested dedicated workers correctly.
//
// Shared workers are quite similar to dedicated workers but they can have any
// number of clients. Also, a shared worker can temporarily appear to have no
// clients shortly after being created and just before being destroyed.
//
// Service workers are more complicated to handle. They also can have any number
// of clients, but they aren't only frames. They could also be dedicated worker
// and shared worker clients. These different types of clients are tracked using
// the ServiceWorkerClientInfo type. Also, because of the important role the
// service worker plays with frame navigations, the service worker can be
// created before its first client's navigation has committed to a
// RenderFrameHost. So when a OnControlleeAdded() notification is received for
// a client frame, it is necessary to wait until the RenderFrameHost was
// determined.
class WorkerWatcher : public content::DedicatedWorkerService::Observer,
                      public content::SharedWorkerService::Observer,
                      public content::ServiceWorkerContextObserver {
 public:
  WorkerWatcher(const std::string& browser_context_id,
                content::DedicatedWorkerService* dedicated_worker_service,
                content::SharedWorkerService* shared_worker_service,
                ServiceWorkerContextAdapter* service_worker_context_adapter,
                ProcessNodeSource* process_node_source,
                FrameNodeSource* frame_node_source);

  WorkerWatcher(const WorkerWatcher&) = delete;
  WorkerWatcher& operator=(const WorkerWatcher&) = delete;

  ~WorkerWatcher() override;

  // Cleans up this instance and ensures shared worker nodes are correctly
  // destroyed on the PM graph.
  void TearDown();

  // content::DedicatedWorkerService::Observer:
  void OnWorkerCreated(
      const blink::DedicatedWorkerToken& dedicated_worker_token,
      int worker_process_id,
      const url::Origin& security_origin,
      content::DedicatedWorkerCreator creator) override;
  void OnBeforeWorkerDestroyed(
      const blink::DedicatedWorkerToken& dedicated_worker_token,
      content::DedicatedWorkerCreator creator) override;
  void OnFinalResponseURLDetermined(
      const blink::DedicatedWorkerToken& dedicated_worker_token,
      const GURL& url) override;

  // content::SharedWorkerService::Observer:
  void OnWorkerCreated(const blink::SharedWorkerToken& shared_worker_token,
                       int worker_process_id,
                       const url::Origin& security_origin,
                       const base::UnguessableToken& dev_tools_token) override;
  void OnBeforeWorkerDestroyed(
      const blink::SharedWorkerToken& shared_worker_token) override;
  void OnFinalResponseURLDetermined(
      const blink::SharedWorkerToken& shared_worker_token,
      const GURL& url) override;
  void OnClientAdded(
      const blink::SharedWorkerToken& shared_worker_token,
      content::GlobalRenderFrameHostId render_frame_host_id) override;
  void OnClientRemoved(
      const blink::SharedWorkerToken& shared_worker_token,
      content::GlobalRenderFrameHostId render_frame_host_id) override;

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
      const std::string& client_uuid,
      content::GlobalRenderFrameHostId render_frame_host_id) override;

  // Searches all node maps for a WorkerNode matching the given `token`.
  // Exposed so that accessors performance_manager.h can look up WorkerNodes on
  // the UI thread.
  WorkerNodeImpl* FindWorkerNodeForToken(const blink::WorkerToken& token) const;

 private:
  friend class WorkerWatcherTest;

  // Adds a connection between |worker_node| and the frame node represented by
  // |client_render_frame_host_id|. Connects them in the graph when the first
  // connection is added.
  void AddFrameClientConnection(
      WorkerNodeImpl* worker_node,
      content::GlobalRenderFrameHostId client_render_frame_host_id);
  // Removes a connection between |worker_node| and the frame node represented
  // by |client_render_frame_host_id|. Disconnects them in the graph when the
  // last connection is removed.
  void RemoveFrameClientConnection(
      WorkerNodeImpl* worker_node,
      content::GlobalRenderFrameHostId client_render_frame_host_id);

  // If a node with |client_dedicated_worker_token| exists, posts a task to
  // the PM graph to connect/disconnect |worker_node| with the
  // dedicated worker node associated to |client_dedicated_worker_token|.
  void ConnectDedicatedWorkerClient(
      WorkerNodeImpl* worker_node,
      blink::DedicatedWorkerToken client_dedicated_worker_token);
  void DisconnectDedicatedWorkerClient(
      WorkerNodeImpl* worker_node,
      blink::DedicatedWorkerToken client_dedicated_worker_token);

  // If a node with |client_shared_worker_token| exists, posts a task to
  // the PM graph to connect/disconnect |worker_node| with the
  // dedicated worker node associated to |client_shared_worker_token|.
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
      content::GlobalRenderFrameHostId render_frame_host_id,
      FrameNodeImpl* frame_node);

  // Adds/removes a connection to |child_worker_node| in the set of child
  // workers of a frame.
  // On exit |is_first_child_worker| is true if this is the first child worker
  // added to the frame and |is_first_child_worker_connection| is true if
  // this was the first connection from the frame and |child_worker_node|.
  // Conversely |was_last_child_worker| is true if this was the last client
  // worker removed, and |was_last_child_worker_connection| is true if this
  // removed the last connection between the frame and |child_worker_node|.
  void AddChildWorkerConnection(
      content::GlobalRenderFrameHostId render_frame_host_id,
      WorkerNodeImpl* child_worker_node,
      bool* is_first_child_worker,
      bool* is_first_child_worker_connection);
  void RemoveChildWorkerConnection(
      content::GlobalRenderFrameHostId render_frame_host_id,
      WorkerNodeImpl* child_worker_node,
      bool* was_last_child_worker,
      bool* was_last_child_worker_connection);

  // Helper functions to retrieve an existing worker node.
  // Return the requested node, or nullptr if no such node registered.
  WorkerNodeImpl* GetDedicatedWorkerNode(
      const blink::DedicatedWorkerToken& dedicated_worker_token) const;
  WorkerNodeImpl* GetSharedWorkerNode(
      const blink::SharedWorkerToken& shared_worker_token) const;
  WorkerNodeImpl* GetServiceWorkerNode(int64_t version_id) const;

#if DCHECK_IS_ON()
  bool IsServiceWorkerNode(WorkerNodeImpl* worker_node);
#endif

  SEQUENCE_CHECKER(sequence_checker_);

  // The ID of the BrowserContext who owns the shared worker service.
  const std::string browser_context_id_;

  // Observes the DedicatedWorkerService for this browser context.
  base::ScopedObservation<content::DedicatedWorkerService,
                          content::DedicatedWorkerService::Observer>
      dedicated_worker_service_observation_{this};

  // Observes the SharedWorkerService for this browser context.
  base::ScopedObservation<content::SharedWorkerService,
                          content::SharedWorkerService::Observer>
      shared_worker_service_observation_{this};

  base::ScopedObservation<ServiceWorkerContextAdapter,
                          content::ServiceWorkerContextObserver>
      service_worker_context_adapter_observation_{this};

  // Used to retrieve an existing process node from its render process ID.
  const raw_ptr<ProcessNodeSource> process_node_source_;

  // Used to retrieve an existing frame node from its render process ID and
  // frame ID. Also allows to subscribe to a frame's deletion notification.
  const raw_ptr<FrameNodeSource> frame_node_source_;

  // Maps each dedicated worker ID to its worker node.
  base::flat_map<blink::DedicatedWorkerToken, std::unique_ptr<WorkerNodeImpl>>
      dedicated_worker_nodes_;

  // Maps each shared worker ID to its worker node.
  base::flat_map<blink::SharedWorkerToken, std::unique_ptr<WorkerNodeImpl>>
      shared_worker_nodes_;

  // Maps each service worker version ID to its worker node.
  base::flat_map<int64_t /*version_id*/, std::unique_ptr<WorkerNodeImpl>>
      service_worker_nodes_;

  // Maps service worker tokens to a version ID that can be looked up in
  // `service_worker_nodes_`.
  std::map<blink::ServiceWorkerToken, int64_t /*version_id*/>
      service_worker_ids_by_token_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Keeps track of frame clients that are awaiting the navigation commit
  // notification. Used for service workers only.
  using AwaitingKey =
      std::pair<int64_t /*version_id*/, std::string /*client_uuid*/>;
  base::flat_set<AwaitingKey> client_frames_awaiting_commit_;

  // Maps each service worker to its clients.
  base::flat_map<int64_t /*version_id*/,
                 base::flat_map<std::string /*client_uuid*/,
                                content::ServiceWorkerClientInfo>>
      service_worker_clients_;

  // Maps each frame to the number of connections to each worker that this frame
  // is a client of in the graph. Note that normally there's a single connection
  // from a frame to a worker, except in rare circumstances where it appears
  // that a single frame can have multiple "controllee" relationships to the
  // same service worker. This is represented as a single edge in the PM graph.
  using WorkerNodeConnections = base::flat_map<WorkerNodeImpl*, size_t>;
  base::flat_map<content::GlobalRenderFrameHostId, WorkerNodeConnections>
      frame_node_child_worker_connections_;

  // Maps each dedicated worker to all its child workers.
  using WorkerNodeSet =
      base::flat_set<raw_ptr<WorkerNodeImpl, CtnExperimental>>;
  base::flat_map<blink::DedicatedWorkerToken, WorkerNodeSet>
      dedicated_worker_child_workers_;

  // Maps each shared worker to all its child workers.
  base::flat_map<blink::SharedWorkerToken, WorkerNodeSet>
      shared_worker_child_workers_;

#if DCHECK_IS_ON()
  // Keeps track of how many OnClientRemoved() calls are expected for an
  // existing worker. This happens when OnBeforeFrameNodeRemoved() is invoked
  // before OnClientRemoved(), or when it wasn't possible to initially attach
  // a client frame node to a worker.
  base::flat_map<WorkerNodeImpl*, int> detached_frame_count_per_worker_;

  // Keeps track of shared and dedicated workers that were not present when
  // a service worker tried to add a client relationship for them.
  base::flat_map<WorkerNodeImpl*,
                 base::flat_set<content::ServiceWorkerClientInfo>>
      missing_service_worker_clients_;
#endif  // DCHECK_IS_ON()
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_WORKER_WATCHER_H_

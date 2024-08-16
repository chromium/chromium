// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/worker_watcher.h"

#include <map>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/overloaded.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "components/performance_manager/frame_node_source.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/process_node_source.h"
#include "components/performance_manager/public/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager {

using WorkerNodeSet = base::flat_set<raw_ptr<WorkerNodeImpl, CtnExperimental>>;

namespace {

// Helper function to add |client_frame_node| as a client of |worker_node| on
// the PM sequence.
void ConnectClientFrameOnGraph(WorkerNodeImpl* worker_node,
                               FrameNodeImpl* client_frame_node) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&WorkerNodeImpl::AddClientFrame,
                     base::Unretained(worker_node), client_frame_node));
}

// Helper function to remove |client_frame_node| as a client of |worker_node|
// on the PM sequence.
void DisconnectClientFrameOnGraph(WorkerNodeImpl* worker_node,
                                  FrameNodeImpl* client_frame_node) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&WorkerNodeImpl::RemoveClientFrame,
                     base::Unretained(worker_node), client_frame_node));
}

// Helper function to add |client_worker_node| as a client of |worker_node| on
// the PM sequence.
void ConnectClientWorkerOnGraph(WorkerNodeImpl* worker_node,
                                WorkerNodeImpl* client_worker_node) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&WorkerNodeImpl::AddClientWorker,
                     base::Unretained(worker_node), client_worker_node));
}

// Helper function to remove |client_worker_node| as a client of |worker_node|
// on the PM sequence.
void DisconnectClientWorkerOnGraph(WorkerNodeImpl* worker_node,
                                   WorkerNodeImpl* client_worker_node) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&WorkerNodeImpl::RemoveClientWorker,
                     base::Unretained(worker_node), client_worker_node));
}

// Helper function to remove |client_frame_node| as a client of all worker nodes
// in |worker_nodes| on the PM sequence.
void DisconnectClientsOnGraph(WorkerNodeSet worker_nodes,
                              FrameNodeImpl* client_frame_node) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(
          [](WorkerNodeSet worker_nodes, FrameNodeImpl* client_frame_node) {
            for (WorkerNodeImpl* worker_node : worker_nodes) {
              worker_node->RemoveClientFrame(client_frame_node);
            }
          },
          std::move(worker_nodes), client_frame_node));
}

void DisconnectClientsOnGraph(
    base::flat_map<WorkerNodeImpl*, size_t> worker_node_connections,
    FrameNodeImpl* client_frame_node) {
  WorkerNodeSet::container_type client_workers;
  for (auto& kv : worker_node_connections)
    client_workers.push_back(kv.first);

  DisconnectClientsOnGraph(WorkerNodeSet(base::sorted_unique, client_workers),
                           client_frame_node);
}

// Helper function to remove |client_worker_node| as a client of all worker
// nodes in |worker_nodes| on the PM sequence.
void DisconnectClientsOnGraph(WorkerNodeSet worker_nodes,
                              WorkerNodeImpl* client_worker_node) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(
          [](WorkerNodeSet worker_nodes, WorkerNodeImpl* client_worker_node) {
            for (WorkerNodeImpl* worker_node : worker_nodes) {
              worker_node->RemoveClientWorker(client_worker_node);
            }
          },
          std::move(worker_nodes), client_worker_node));
}

// Helper function that posts a task on the PM sequence that will invoke
// OnFinalResponseURLDetermined() on |worker_node|.
void SetFinalResponseURL(WorkerNodeImpl* worker_node, const GURL& url) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&WorkerNodeImpl::OnFinalResponseURLDetermined,
                                base::Unretained(worker_node), url));
}

}  // namespace

WorkerWatcher::WorkerWatcher(
    const std::string& browser_context_id,
    content::DedicatedWorkerService* dedicated_worker_service,
    content::SharedWorkerService* shared_worker_service,
    ServiceWorkerContextAdapter* service_worker_context_adapter,
    ProcessNodeSource* process_node_source,
    FrameNodeSource* frame_node_source)
    : browser_context_id_(browser_context_id),
      process_node_source_(process_node_source),
      frame_node_source_(frame_node_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(dedicated_worker_service);
  DCHECK(shared_worker_service);
  DCHECK(service_worker_context_adapter);
  DCHECK(process_node_source_);
  DCHECK(frame_node_source_);

  dedicated_worker_service_observation_.Observe(dedicated_worker_service);
  shared_worker_service_observation_.Observe(shared_worker_service);
  service_worker_context_adapter_observation_.Observe(
      service_worker_context_adapter);
}

WorkerWatcher::~WorkerWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_node_child_worker_connections_.empty());
  DCHECK(dedicated_worker_nodes_.empty());
  DCHECK(!dedicated_worker_service_observation_.IsObserving());
  DCHECK(shared_worker_nodes_.empty());
  DCHECK(!shared_worker_service_observation_.IsObserving());
  DCHECK(service_worker_nodes_.empty());
  CHECK(service_worker_ids_by_token_.empty());
  DCHECK(!service_worker_context_adapter_observation_.IsObserving());
}

void WorkerWatcher::TearDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // First clear client-child connections between frames and workers.
  for (auto& kv : frame_node_child_worker_connections_) {
    const content::GlobalRenderFrameHostId& render_frame_host_id = kv.first;
    WorkerNodeConnections& child_worker_connections = kv.second;
    DCHECK(!child_worker_connections.empty());

    frame_node_source_->UnsubscribeFromFrameNode(render_frame_host_id);

    // Disconnect all child workers from |frame_node|.
    FrameNodeImpl* frame_node =
        frame_node_source_->GetFrameNode(render_frame_host_id);
    DCHECK(frame_node);
    DisconnectClientsOnGraph(std::move(child_worker_connections), frame_node);
  }
  frame_node_child_worker_connections_.clear();

  // Then clear client-child connections for dedicated workers.
  for (auto& kv : dedicated_worker_child_workers_) {
    const blink::DedicatedWorkerToken& dedicated_worker_token = kv.first;
    WorkerNodeSet& child_workers = kv.second;
    DCHECK(!child_workers.empty());

    // Disconnect all child workers from |dedicated_worker_token|.
    WorkerNodeImpl* dedicated_worker_node =
        GetDedicatedWorkerNode(dedicated_worker_token);
    DisconnectClientsOnGraph(std::move(child_workers), dedicated_worker_node);
  }
  dedicated_worker_child_workers_.clear();

  // Finally, clear client-child connections for shared workers.
  for (auto& kv : shared_worker_child_workers_) {
    const blink::SharedWorkerToken& shared_worker_token = kv.first;
    WorkerNodeSet& child_workers = kv.second;
    DCHECK(!child_workers.empty());

    // Disconnect all child workers from |shared_worker_token|.
    WorkerNodeImpl* shared_worker_node =
        GetSharedWorkerNode(shared_worker_token);
    DisconnectClientsOnGraph(std::move(child_workers), shared_worker_node);
  }
  shared_worker_child_workers_.clear();

  // Then clean all the worker nodes.
  std::vector<std::unique_ptr<NodeBase>> nodes;
  nodes.reserve(dedicated_worker_nodes_.size() + shared_worker_nodes_.size() +
                service_worker_nodes_.size());

  for (auto& node : dedicated_worker_nodes_)
    nodes.push_back(std::move(node.second));
  dedicated_worker_nodes_.clear();

  for (auto& node : shared_worker_nodes_)
    nodes.push_back(std::move(node.second));
  shared_worker_nodes_.clear();

  for (auto& node : service_worker_nodes_)
    nodes.push_back(std::move(node.second));
  service_worker_nodes_.clear();
  service_worker_ids_by_token_.clear();

  PerformanceManagerImpl::BatchDeleteNodes(std::move(nodes));

  DCHECK(dedicated_worker_service_observation_.IsObserving());
  dedicated_worker_service_observation_.Reset();
  DCHECK(shared_worker_service_observation_.IsObserving());
  shared_worker_service_observation_.Reset();
  DCHECK(service_worker_context_adapter_observation_.IsObserving());
  service_worker_context_adapter_observation_.Reset();
}

void WorkerWatcher::OnWorkerCreated(
    const blink::DedicatedWorkerToken& dedicated_worker_token,
    int worker_process_id,
    const url::Origin& security_origin,
    content::DedicatedWorkerCreator creator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto worker_node = PerformanceManagerImpl::CreateWorkerNode(
      browser_context_id_, WorkerNode::WorkerType::kDedicated,
      process_node_source_->GetProcessNode(worker_process_id),
      dedicated_worker_token, security_origin);
  auto insertion_result = dedicated_worker_nodes_.emplace(
      dedicated_worker_token, std::move(worker_node));
  DCHECK(insertion_result.second);

  absl::visit(
      base::Overloaded(
          [&,
           this](const content::GlobalRenderFrameHostId& render_frame_host_id) {
            AddFrameClientConnection(insertion_result.first->second.get(),
                                     render_frame_host_id);
          },
          [&, this](blink::DedicatedWorkerToken dedicated_worker_token) {
            ConnectDedicatedWorkerClient(insertion_result.first->second.get(),
                                         dedicated_worker_token);
          }),
      creator);
}

void WorkerWatcher::OnBeforeWorkerDestroyed(
    const blink::DedicatedWorkerToken& dedicated_worker_token,
    content::DedicatedWorkerCreator creator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = dedicated_worker_nodes_.find(dedicated_worker_token);
  CHECK(it != dedicated_worker_nodes_.end(), base::NotFatalUntil::M130);

  auto worker_node = std::move(it->second);

  // First disconnect the creator's node from this worker node.

  absl::visit(
      base::Overloaded(
          [&,
           this](const content::GlobalRenderFrameHostId& render_frame_host_id) {
            RemoveFrameClientConnection(worker_node.get(),
                                        render_frame_host_id);
          },
          [&, this](blink::DedicatedWorkerToken dedicated_worker_token) {
            DisconnectDedicatedWorkerClient(worker_node.get(),
                                            dedicated_worker_token);
          }),
      creator);

  // Disconnect all child workers before destroying the node.
  auto child_it = dedicated_worker_child_workers_.find(dedicated_worker_token);
  if (child_it != dedicated_worker_child_workers_.end()) {
    const WorkerNodeSet& child_workers = child_it->second;
    DisconnectClientsOnGraph(child_workers, worker_node.get());

#if DCHECK_IS_ON()
    for (WorkerNodeImpl* worker : child_workers) {
      // If this is a service worker client, mark it as a missing client.
      if (IsServiceWorkerNode(worker)) {
        DCHECK(missing_service_worker_clients_[worker]
                   .insert(dedicated_worker_token)
                   .second);
      }
    }
#endif

    dedicated_worker_child_workers_.erase(child_it);
  }

#if DCHECK_IS_ON()
  DCHECK(!base::Contains(detached_frame_count_per_worker_, worker_node.get()));
#endif  // DCHECK_IS_ON()
  PerformanceManagerImpl::DeleteNode(std::move(worker_node));

  dedicated_worker_nodes_.erase(it);
}

void WorkerWatcher::OnFinalResponseURLDetermined(
    const blink::DedicatedWorkerToken& dedicated_worker_token,
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetFinalResponseURL(GetDedicatedWorkerNode(dedicated_worker_token), url);
}

void WorkerWatcher::OnWorkerCreated(
    const blink::SharedWorkerToken& shared_worker_token,
    int worker_process_id,
    const url::Origin& security_origin,
    const base::UnguessableToken& /* dev_tools_token */) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto worker_node = PerformanceManagerImpl::CreateWorkerNode(
      browser_context_id_, WorkerNode::WorkerType::kShared,
      process_node_source_->GetProcessNode(worker_process_id),
      shared_worker_token, security_origin);

  bool inserted =
      shared_worker_nodes_.emplace(shared_worker_token, std::move(worker_node))
          .second;
  DCHECK(inserted);
}

void WorkerWatcher::OnBeforeWorkerDestroyed(
    const blink::SharedWorkerToken& shared_worker_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = shared_worker_nodes_.find(shared_worker_token);
  CHECK(it != shared_worker_nodes_.end(), base::NotFatalUntil::M130);

  auto worker_node = std::move(it->second);

  // Disconnect all child workers before destroying the node.
  auto child_it = shared_worker_child_workers_.find(shared_worker_token);
  if (child_it != shared_worker_child_workers_.end()) {
    const WorkerNodeSet& child_workers = child_it->second;
    DisconnectClientsOnGraph(child_workers, worker_node.get());

#if DCHECK_IS_ON()
    for (WorkerNodeImpl* worker : child_workers) {
      // If this is a service worker client, mark it as a missing client.
      if (IsServiceWorkerNode(worker)) {
        DCHECK(missing_service_worker_clients_[worker]
                   .insert(shared_worker_token)
                   .second);
      }
    }
#endif

    shared_worker_child_workers_.erase(child_it);
  }

#if DCHECK_IS_ON()
  DCHECK(!base::Contains(detached_frame_count_per_worker_, worker_node.get()));
#endif  // DCHECK_IS_ON()
  PerformanceManagerImpl::DeleteNode(std::move(worker_node));

  shared_worker_nodes_.erase(it);
}

void WorkerWatcher::OnFinalResponseURLDetermined(
    const blink::SharedWorkerToken& shared_worker_token,
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetFinalResponseURL(GetSharedWorkerNode(shared_worker_token), url);
}

void WorkerWatcher::OnClientAdded(
    const blink::SharedWorkerToken& shared_worker_token,
    content::GlobalRenderFrameHostId render_frame_host_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AddFrameClientConnection(GetSharedWorkerNode(shared_worker_token),
                           render_frame_host_id);
}

void WorkerWatcher::OnClientRemoved(
    const blink::SharedWorkerToken& shared_worker_token,
    content::GlobalRenderFrameHostId render_frame_host_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RemoveFrameClientConnection(GetSharedWorkerNode(shared_worker_token),
                              render_frame_host_id);
}

void WorkerWatcher::OnVersionStartedRunning(
    int64_t version_id,
    const content::ServiceWorkerRunningInfo& running_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& [it, node_inserted] = service_worker_nodes_.emplace(
      version_id,
      PerformanceManagerImpl::CreateWorkerNode(
          browser_context_id_, WorkerNode::WorkerType::kService,
          process_node_source_->GetProcessNode(running_info.render_process_id),
          running_info.token, running_info.key.origin()));
  DCHECK(node_inserted);
  WorkerNodeImpl* worker_node = it->second.get();

  const auto& [_, token_inserted] =
      service_worker_ids_by_token_.emplace(running_info.token, version_id);
  CHECK(token_inserted);

  // Exclusively for service workers, some notifications for clients
  // (OnControlleeAdded) may have been received before the worker started.
  // Add those clients to the service worker on the PM graph.
  ConnectAllServiceWorkerClients(worker_node, version_id);

  // Unlike other workers, the service worker script url is already set when its
  // added to the graph.
  SetFinalResponseURL(worker_node, running_info.script_url);
}

void WorkerWatcher::OnVersionStoppedRunning(int64_t version_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = service_worker_nodes_.find(version_id);
  CHECK(it != service_worker_nodes_.end(), base::NotFatalUntil::M130);

  auto service_worker_node = std::move(it->second);

  // First, disconnect all current clients of this service worker.
  DisconnectAllServiceWorkerClients(service_worker_node.get(), version_id);

#if DCHECK_IS_ON()
  DCHECK(!base::Contains(detached_frame_count_per_worker_,
                         service_worker_node.get()));
#endif  // DCHECK_IS_ON()
  PerformanceManagerImpl::DeleteNode(std::move(service_worker_node));

  service_worker_nodes_.erase(it);
  size_t erased = std::erase_if(
      service_worker_ids_by_token_,
      [version_id](const auto& entry) { return entry.second == version_id; });
  CHECK_EQ(erased, 1u);
}

void WorkerWatcher::OnControlleeAdded(
    int64_t version_id,
    const std::string& client_uuid,
    const content::ServiceWorkerClientInfo& client_info) {
  absl::visit(
      base::Overloaded(
          [&, this](content::GlobalRenderFrameHostId render_frame_host_id) {
            // For window clients, it is necessary to wait until the navigation
            // has committed to a RenderFrameHost.
            bool inserted = client_frames_awaiting_commit_
                                .insert(AwaitingKey(version_id, client_uuid))
                                .second;
            DCHECK(inserted);
          },
          [&, this](blink::DedicatedWorkerToken dedicated_worker_token) {
            bool inserted = service_worker_clients_[version_id]
                                .emplace(client_uuid, dedicated_worker_token)
                                .second;
            DCHECK(inserted);

            // If the service worker is already started, connect it to the
            // client.
            WorkerNodeImpl* service_worker_node =
                GetServiceWorkerNode(version_id);
            if (service_worker_node) {
              ConnectDedicatedWorkerClient(service_worker_node,
                                           dedicated_worker_token);
            }
          },
          [&, this](blink::SharedWorkerToken shared_worker_token) {
            bool inserted = service_worker_clients_[version_id]
                                .emplace(client_uuid, shared_worker_token)
                                .second;
            DCHECK(inserted);

            // If the service worker is already started, connect it to the
            // client.
            WorkerNodeImpl* service_worker_node =
                GetServiceWorkerNode(version_id);
            if (service_worker_node) {
              ConnectSharedWorkerClient(service_worker_node,
                                        shared_worker_token);
            }
          }),
      client_info);
}

void WorkerWatcher::OnControlleeRemoved(int64_t version_id,
                                        const std::string& client_uuid) {
  // Nothing to do for a frame client whose navigation never committed.
  size_t removed = client_frames_awaiting_commit_.erase(
      AwaitingKey(version_id, client_uuid));
  if (removed) {
#if DCHECK_IS_ON()
    // |client_uuid| should not be part of this service worker's clients.
    auto it = service_worker_clients_.find(version_id);
    if (it != service_worker_clients_.end())
      DCHECK(!base::Contains(it->second, client_uuid));
#endif  // DCHECK_IS_ON()
    return;
  }

  // First get clients for this worker.
  auto it = service_worker_clients_.find(version_id);
  CHECK(it != service_worker_clients_.end(), base::NotFatalUntil::M130);

  base::flat_map<std::string /*client_uuid*/, content::ServiceWorkerClientInfo>&
      clients = it->second;

  auto it2 = clients.find(client_uuid);
  CHECK(it2 != clients.end(), base::NotFatalUntil::M130);
  const content::ServiceWorkerClientInfo client = it2->second;
  clients.erase(it2);

  if (clients.empty())
    service_worker_clients_.erase(it);

  // Now disconnect the client if the service worker is still running.
  WorkerNodeImpl* worker_node = GetServiceWorkerNode(version_id);
  if (!worker_node)
    return;

  absl::visit(
      base::Overloaded(
          [&, this](content::GlobalRenderFrameHostId render_frame_host_id) {
            RemoveFrameClientConnection(worker_node, render_frame_host_id);
          },
          [&, this](blink::DedicatedWorkerToken dedicated_worker_token) {
            DisconnectDedicatedWorkerClient(worker_node,
                                            dedicated_worker_token);
          },
          [&, this](blink::SharedWorkerToken shared_worker_token) {
            DisconnectSharedWorkerClient(worker_node, shared_worker_token);
          }),
      client);
}

void WorkerWatcher::OnControlleeNavigationCommitted(
    int64_t version_id,
    const std::string& client_uuid,
    content::GlobalRenderFrameHostId render_frame_host_id) {
  size_t removed = client_frames_awaiting_commit_.erase(
      AwaitingKey(version_id, client_uuid));
  DCHECK_EQ(removed, 1u);

  bool inserted = service_worker_clients_[version_id]
                      .emplace(client_uuid, render_frame_host_id)
                      .second;
  DCHECK(inserted);

  WorkerNodeImpl* service_worker_node = GetServiceWorkerNode(version_id);
  if (service_worker_node)
    AddFrameClientConnection(service_worker_node, render_frame_host_id);
}

WorkerNodeImpl* WorkerWatcher::FindWorkerNodeForToken(
    const blink::WorkerToken& token) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (token.Is<blink::DedicatedWorkerToken>()) {
    return GetDedicatedWorkerNode(token.GetAs<blink::DedicatedWorkerToken>());
  }
  if (token.Is<blink::SharedWorkerToken>()) {
    return GetSharedWorkerNode(token.GetAs<blink::SharedWorkerToken>());
  }
  if (token.Is<blink::ServiceWorkerToken>()) {
    // Service workers are keyed by version ID, not token.
    const auto it = service_worker_ids_by_token_.find(
        token.GetAs<blink::ServiceWorkerToken>());
    if (it == service_worker_ids_by_token_.end()) {
      return nullptr;
    }
    // at() asserts that the id is in `service_worker_nodes_`.
    return service_worker_nodes_.at(it->second).get();
  }
  NOTREACHED();
}

void WorkerWatcher::AddFrameClientConnection(
    WorkerNodeImpl* worker_node,
    content::GlobalRenderFrameHostId client_render_frame_host_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(worker_node);

  FrameNodeImpl* frame_node =
      frame_node_source_->GetFrameNode(client_render_frame_host_id);
  // TODO(crbug.com/40129396): The client frame's node should always be
  // accessible. If it isn't, this means there is a missing
  // CreatePageNodeForWebContents() somewhere.
  if (!frame_node) {
#if DCHECK_IS_ON()
    // A call to RemoveFrameClientConnection() is still expected to be received
    // for this worker and frame pair.
    detached_frame_count_per_worker_[worker_node]++;
#endif  // DCHECK_IS_ON()
    return;
  }

  // Keep track of the workers that this frame is a client to.
  bool is_first_child_worker = false;
  bool is_first_child_worker_connection = false;
  AddChildWorkerConnection(client_render_frame_host_id, worker_node,
                           &is_first_child_worker,
                           &is_first_child_worker_connection);

  if (is_first_child_worker) {
    frame_node_source_->SubscribeToFrameNode(
        client_render_frame_host_id,
        base::BindOnce(&WorkerWatcher::OnBeforeFrameNodeRemoved,
                       base::Unretained(this), client_render_frame_host_id));
  }

  if (is_first_child_worker_connection) {
    // Connect the nodes on the graph only on the 0->1 transition.
    ConnectClientFrameOnGraph(worker_node, frame_node);
  }
}

void WorkerWatcher::RemoveFrameClientConnection(
    WorkerNodeImpl* worker_node,
    content::GlobalRenderFrameHostId client_render_frame_host_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(worker_node);

  FrameNodeImpl* frame_node =
      frame_node_source_->GetFrameNode(client_render_frame_host_id);

  // It's possible that the frame was destroyed before receiving the
  // OnClientRemoved() for all of its child shared worker. Nothing to do in
  // that case because OnBeforeFrameNodeRemoved() took care of removing this
  // client from its child worker nodes.
  //
  // TODO(crbug.com/40129396): A second possibility is that it wasn't
  // possible to connect a worker to its client frame.
  if (!frame_node) {
#if DCHECK_IS_ON()
    // These debug only checks are used to ensure that this
    // RemoveFrameClientConnection() call was still expected even though the
    // client frame node no longer exist.
    auto it = detached_frame_count_per_worker_.find(worker_node);
    CHECK(it != detached_frame_count_per_worker_.end(),
          base::NotFatalUntil::M130);

    int& count = it->second;
    DCHECK_GT(count, 0);
    --count;

    if (count == 0)
      detached_frame_count_per_worker_.erase(it);
#endif  // DCHECK_IS_ON()
    return;
  }
  // Remove |worker_node| from the set of workers that this frame is a client
  // of.
  bool was_last_child_worker = false;
  bool was_last_child_worker_connection = false;
  RemoveChildWorkerConnection(client_render_frame_host_id, worker_node,
                              &was_last_child_worker,
                              &was_last_child_worker_connection);

  if (was_last_child_worker)
    frame_node_source_->UnsubscribeFromFrameNode(client_render_frame_host_id);

  if (was_last_child_worker_connection) {
    // Disconnect the nodes on the graph only on the 1->0 transition.
    DisconnectClientFrameOnGraph(worker_node, frame_node);
  }
}

void WorkerWatcher::ConnectDedicatedWorkerClient(
    WorkerNodeImpl* worker_node,
    blink::DedicatedWorkerToken client_dedicated_worker_token) {
  DCHECK(worker_node);

  WorkerNodeImpl* client_dedicated_worker_node =
      GetDedicatedWorkerNode(client_dedicated_worker_token);
  if (!client_dedicated_worker_node) {
#if DCHECK_IS_ON()
    if (IsServiceWorkerNode(worker_node)) {
      bool inserted = missing_service_worker_clients_[worker_node]
                          .insert(client_dedicated_worker_token)
                          .second;
      DCHECK(inserted);
    }
#endif
    return;
  }

  ConnectClientWorkerOnGraph(worker_node, client_dedicated_worker_node);

  // Remember that |worker_node| is a child worker of this dedicated worker.
  bool inserted = dedicated_worker_child_workers_[client_dedicated_worker_token]
                      .insert(worker_node)
                      .second;
  DCHECK(inserted);
}

void WorkerWatcher::DisconnectDedicatedWorkerClient(
    WorkerNodeImpl* worker_node,
    blink::DedicatedWorkerToken client_dedicated_worker_token) {
  DCHECK(worker_node);

  WorkerNodeImpl* client_dedicated_worker =
      GetDedicatedWorkerNode(client_dedicated_worker_token);
  if (!client_dedicated_worker) {
#if DCHECK_IS_ON()
    if (IsServiceWorkerNode(worker_node)) {
      auto it = missing_service_worker_clients_.find(worker_node);
      CHECK(it != missing_service_worker_clients_.end(),
            base::NotFatalUntil::M130);
      DCHECK_EQ(1u, it->second.erase(content::ServiceWorkerClientInfo(
                        client_dedicated_worker_token)));
      if (it->second.empty()) {
        missing_service_worker_clients_.erase(it);
      }
    }
#endif
    return;
  }

  // Remove |worker_node| from the set of child workers of this dedicated
  // worker.
  auto it = dedicated_worker_child_workers_.find(client_dedicated_worker_token);
  CHECK(it != dedicated_worker_child_workers_.end(), base::NotFatalUntil::M130);
  auto& child_workers = it->second;

  size_t removed = child_workers.erase(worker_node);
  DCHECK_EQ(removed, 1u);

  if (child_workers.empty())
    dedicated_worker_child_workers_.erase(it);

  DisconnectClientWorkerOnGraph(worker_node, client_dedicated_worker);
}

void WorkerWatcher::ConnectSharedWorkerClient(
    WorkerNodeImpl* worker_node,
    blink::SharedWorkerToken client_shared_worker_token) {
  DCHECK(worker_node);

  WorkerNodeImpl* client_shared_worker_node =
      GetSharedWorkerNode(client_shared_worker_token);
  if (!client_shared_worker_node) {
#if DCHECK_IS_ON()
    DCHECK(IsServiceWorkerNode(worker_node));
    bool inserted = missing_service_worker_clients_[worker_node]
                        .insert(client_shared_worker_token)
                        .second;
    DCHECK(inserted);
#endif
    return;
  }

  ConnectClientWorkerOnGraph(worker_node, client_shared_worker_node);

  // Remember that |worker_node| is a child worker of this shared worker.
  bool inserted = shared_worker_child_workers_[client_shared_worker_token]
                      .insert(worker_node)
                      .second;
  DCHECK(inserted);
}

void WorkerWatcher::DisconnectSharedWorkerClient(
    WorkerNodeImpl* worker_node,
    blink::SharedWorkerToken client_shared_worker_token) {
  DCHECK(worker_node);

  // This notification may arrive after the client worker has been deleted,
  // in which case the relationship has already been cleaned up.
  auto worker_it = shared_worker_nodes_.find(client_shared_worker_token);
  if (worker_it == shared_worker_nodes_.end()) {
    // Make sure there aren't any child relationships for this worker.
    DCHECK(shared_worker_child_workers_.find(client_shared_worker_token) ==
           shared_worker_child_workers_.end());

#if DCHECK_IS_ON()
    DCHECK(IsServiceWorkerNode(worker_node));
    auto it = missing_service_worker_clients_.find(worker_node);
    CHECK(it != missing_service_worker_clients_.end(),
          base::NotFatalUntil::M130);
    DCHECK_EQ(1u, it->second.erase(content::ServiceWorkerClientInfo(
                      client_shared_worker_token)));
    if (it->second.empty())
      missing_service_worker_clients_.erase(it);
#endif
    return;
  }

  // Remove |worker_node| from the set of child workers of this shared worker.
  auto child_it = shared_worker_child_workers_.find(client_shared_worker_token);
  CHECK(child_it != shared_worker_child_workers_.end(),
        base::NotFatalUntil::M130);
  auto& child_workers = child_it->second;

  size_t removed = child_workers.erase(worker_node);
  DCHECK_EQ(removed, 1u);

  if (child_workers.empty())
    shared_worker_child_workers_.erase(child_it);

  DisconnectClientWorkerOnGraph(
      worker_node, GetSharedWorkerNode(client_shared_worker_token));
}

void WorkerWatcher::ConnectAllServiceWorkerClients(
    WorkerNodeImpl* service_worker_node,
    int64_t version_id) {
  // Nothing to do if there are no clients.
  auto it = service_worker_clients_.find(version_id);
  if (it == service_worker_clients_.end())
    return;

  for (const auto& kv : it->second) {
    absl::visit(
        base::Overloaded(
            [&, this](content::GlobalRenderFrameHostId render_frame_host_id) {
              AddFrameClientConnection(service_worker_node,
                                       render_frame_host_id);
            },
            [&, this](blink::DedicatedWorkerToken dedicated_worker_token) {
              ConnectDedicatedWorkerClient(service_worker_node,
                                           dedicated_worker_token);
            },
            [&, this](blink::SharedWorkerToken shared_worker_token) {
              ConnectSharedWorkerClient(service_worker_node,
                                        shared_worker_token);
            }),
        kv.second);
  }
}

void WorkerWatcher::DisconnectAllServiceWorkerClients(
    WorkerNodeImpl* service_worker_node,
    int64_t version_id) {
  // Nothing to do if there are no clients.
  auto it = service_worker_clients_.find(version_id);
  if (it == service_worker_clients_.end())
    return;

  for (const auto& kv : it->second) {
    absl::visit(
        base::Overloaded(
            [&, this](
                const content::GlobalRenderFrameHostId& render_frame_host_id) {
              RemoveFrameClientConnection(service_worker_node,
                                          render_frame_host_id);
            },
            [&,
             this](const blink::DedicatedWorkerToken& dedicated_worker_token) {
              DisconnectDedicatedWorkerClient(service_worker_node,
                                              dedicated_worker_token);
            },
            [&, this](const blink::SharedWorkerToken& shared_worker_token) {
              DisconnectSharedWorkerClient(service_worker_node,
                                           shared_worker_token);
            }),
        kv.second);
  }
}

void WorkerWatcher::OnBeforeFrameNodeRemoved(
    content::GlobalRenderFrameHostId render_frame_host_id,
    FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = frame_node_child_worker_connections_.find(render_frame_host_id);
  CHECK(it != frame_node_child_worker_connections_.end(),
        base::NotFatalUntil::M130);

  // Clean up all child workers of this frame node.
  WorkerNodeConnections child_worker_connections = std::move(it->second);
  frame_node_child_worker_connections_.erase(it);

  // Disconnect all child workers from |frame_node|.
  DCHECK(!child_worker_connections.empty());
  DisconnectClientsOnGraph(child_worker_connections, frame_node);

#if DCHECK_IS_ON()
  for (auto kv : child_worker_connections) {
    // A call to RemoveFrameClientConnection() is still expected to be received
    // for this frame for each connection workers in |child_worker_connections|.
    // Note: the [] operator is intentionally used to default initialize the
    // count to zero if needed.
    detached_frame_count_per_worker_[kv.first] += kv.second;
  }
#endif  // DCHECK_IS_ON()
}

void WorkerWatcher::AddChildWorkerConnection(
    content::GlobalRenderFrameHostId render_frame_host_id,
    WorkerNodeImpl* child_worker_node,
    bool* is_first_child_worker,
    bool* is_first_child_worker_connection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto insertion_result =
      frame_node_child_worker_connections_.insert({render_frame_host_id, {}});
  *is_first_child_worker = insertion_result.second;

  auto& child_worker_connections = insertion_result.first->second;
  const size_t count = ++child_worker_connections[child_worker_node];
  DCHECK_LE(0u, count);
  *is_first_child_worker_connection = count == 1;
}

void WorkerWatcher::RemoveChildWorkerConnection(
    content::GlobalRenderFrameHostId render_frame_host_id,
    WorkerNodeImpl* child_worker_node,
    bool* was_last_child_worker,
    bool* was_last_child_worker_connection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = frame_node_child_worker_connections_.find(render_frame_host_id);
  CHECK(it != frame_node_child_worker_connections_.end(),
        base::NotFatalUntil::M130);
  auto& child_worker_connections = it->second;

  DCHECK_LE(1u, child_worker_connections[child_worker_node]);
  const size_t count = --child_worker_connections[child_worker_node];
  *was_last_child_worker_connection = count == 0;

  if (count == 0) {
    const size_t removed = child_worker_connections.erase(child_worker_node);
    DCHECK_EQ(removed, 1u);
  }

  *was_last_child_worker = child_worker_connections.empty();
  if (child_worker_connections.empty()) {
    frame_node_child_worker_connections_.erase(it);
  }
}

WorkerNodeImpl* WorkerWatcher::GetDedicatedWorkerNode(
    const blink::DedicatedWorkerToken& dedicated_worker_token) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = dedicated_worker_nodes_.find(dedicated_worker_token);
  if (it == dedicated_worker_nodes_.end())
    return nullptr;

  return it->second.get();
}

WorkerNodeImpl* WorkerWatcher::GetSharedWorkerNode(
    const blink::SharedWorkerToken& shared_worker_token) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = shared_worker_nodes_.find(shared_worker_token);
  if (it == shared_worker_nodes_.end())
    return nullptr;

  return it->second.get();
}

WorkerNodeImpl* WorkerWatcher::GetServiceWorkerNode(int64_t version_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = service_worker_nodes_.find(version_id);
  if (it == service_worker_nodes_.end())
    return nullptr;

  return it->second.get();
}

#if DCHECK_IS_ON()
bool WorkerWatcher::IsServiceWorkerNode(WorkerNodeImpl* worker_node) {
  for (const auto& kv : service_worker_nodes_) {
    if (kv.second.get() == worker_node)
      return true;
  }

  return false;
}
#endif

}  // namespace performance_manager

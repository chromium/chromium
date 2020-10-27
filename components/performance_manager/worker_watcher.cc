// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/worker_watcher.h"

#include <utility>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "components/performance_manager/frame_node_source.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/process_node_source.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {

namespace {

// Emits a boolean value that indicates if the client frame's node was found
// when trying to connect the worker to a client frame.
void RecordWorkerClientFound(bool found) {
  UMA_HISTOGRAM_BOOLEAN("PerformanceManager.WorkerClientFound", found);
}

// Helper function to add |client_frame_node| as a client of |worker_node| on
// the PM sequence.
void ConnectClientOnGraph(WorkerNodeImpl* worker_node,
                          FrameNodeImpl* client_frame_node) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&WorkerNodeImpl::AddClientFrame,
                     base::Unretained(worker_node), client_frame_node));
}

// Helper function to remove |client_frame_node| as a client of |worker_node|
// on the PM sequence.
void DisconnectClientOnGraph(WorkerNodeImpl* worker_node,
                             FrameNodeImpl* client_frame_node) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&WorkerNodeImpl::RemoveClientFrame,
                     base::Unretained(worker_node), client_frame_node));
}

// Helper function to add |client_worker_node| as a client of |worker_node| on
// the PM sequence.
void ConnectClientOnGraph(WorkerNodeImpl* worker_node,
                          WorkerNodeImpl* client_worker_node) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&WorkerNodeImpl::AddClientWorker,
                     base::Unretained(worker_node), client_worker_node));
}

// Helper function to remove |client_worker_node| as a client of |worker_node|
// on the PM sequence.
void DisconnectClientOnGraph(WorkerNodeImpl* worker_node,
                             WorkerNodeImpl* client_worker_node) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&WorkerNodeImpl::RemoveClientWorker,
                     base::Unretained(worker_node), client_worker_node));
}

// Helper function to remove |client_frame_node| as a client of all worker nodes
// in |worker_nodes| on the PM sequence.
void DisconnectClientsOnGraph(base::flat_set<WorkerNodeImpl*> worker_nodes,
                              FrameNodeImpl* client_frame_node) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(
                     [](base::flat_set<WorkerNodeImpl*> worker_nodes,
                        FrameNodeImpl* client_frame_node) {
                       for (auto* worker_node : worker_nodes)
                         worker_node->RemoveClientFrame(client_frame_node);
                     },
                     std::move(worker_nodes), client_frame_node));
}

// Helper function to remove |client_worker_node| as a client of all worker
// nodes in |worker_nodes| on the PM sequence.
void DisconnectClientsOnGraph(base::flat_set<WorkerNodeImpl*> worker_nodes,
                              WorkerNodeImpl* client_worker_node) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(
                     [](base::flat_set<WorkerNodeImpl*> worker_nodes,
                        WorkerNodeImpl* client_worker_node) {
                       for (auto* worker_node : worker_nodes)
                         worker_node->RemoveClientWorker(client_worker_node);
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
    content::ServiceWorkerContext* service_worker_context,
    ProcessNodeSource* process_node_source,
    FrameNodeSource* frame_node_source)
    : browser_context_id_(browser_context_id),
      process_node_source_(process_node_source),
      frame_node_source_(frame_node_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(dedicated_worker_service);
  DCHECK(shared_worker_service);
  DCHECK(service_worker_context);
  DCHECK(process_node_source_);
  DCHECK(frame_node_source_);

  dedicated_worker_service_observer_.Add(dedicated_worker_service);
  shared_worker_service_observer_.Add(shared_worker_service);
  service_worker_context_observer_.Add(service_worker_context);
}

WorkerWatcher::~WorkerWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_node_child_workers_.empty());
  DCHECK(dedicated_worker_nodes_.empty());
  DCHECK(!dedicated_worker_service_observer_.IsObservingSources());
  DCHECK(shared_worker_nodes_.empty());
  DCHECK(!shared_worker_service_observer_.IsObservingSources());
  DCHECK(service_worker_nodes_.empty());
  DCHECK(!service_worker_context_observer_.IsObservingSources());
}

void WorkerWatcher::TearDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // First clear client-child connections between frames and workers.
  for (auto& kv : frame_node_child_workers_) {
    const content::GlobalFrameRoutingId& render_frame_host_id = kv.first;
    base::flat_set<WorkerNodeImpl*>& child_workers = kv.second;
    DCHECK(!child_workers.empty());

    frame_node_source_->UnsubscribeFromFrameNode(render_frame_host_id);

    // Disconnect all child workers from |frame_node|.
    FrameNodeImpl* frame_node =
        frame_node_source_->GetFrameNode(render_frame_host_id);
    DCHECK(frame_node);
    DisconnectClientsOnGraph(std::move(child_workers), frame_node);
  }
  frame_node_child_workers_.clear();

  // Then clear client-child connections for dedicated workers.
  for (auto& kv : dedicated_worker_child_workers_) {
    const blink::DedicatedWorkerToken& dedicated_worker_token = kv.first;
    base::flat_set<WorkerNodeImpl*>& child_workers = kv.second;
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
    base::flat_set<WorkerNodeImpl*>& child_workers = kv.second;
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

  PerformanceManagerImpl::BatchDeleteNodes(std::move(nodes));

  dedicated_worker_service_observer_.RemoveAll();
  shared_worker_service_observer_.RemoveAll();
  service_worker_context_observer_.RemoveAll();
}

void WorkerWatcher::OnWorkerCreated(
    const blink::DedicatedWorkerToken& dedicated_worker_token,
    int worker_process_id,
    content::GlobalFrameRoutingId ancestor_render_frame_host_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(https://crbug.com/993029): Plumb through the URL.
  auto worker_node = PerformanceManagerImpl::CreateWorkerNode(
      browser_context_id_, WorkerNode::WorkerType::kDedicated,
      process_node_source_->GetProcessNode(worker_process_id),
      dedicated_worker_token);
  auto insertion_result = dedicated_worker_nodes_.emplace(
      dedicated_worker_token, std::move(worker_node));
  DCHECK(insertion_result.second);

  ConnectFrameClient(insertion_result.first->second.get(),
                     ancestor_render_frame_host_id);
}

void WorkerWatcher::OnBeforeWorkerDestroyed(
    const blink::DedicatedWorkerToken& dedicated_worker_token,
    content::GlobalFrameRoutingId ancestor_render_frame_host_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = dedicated_worker_nodes_.find(dedicated_worker_token);
  DCHECK(it != dedicated_worker_nodes_.end());

  auto worker_node = std::move(it->second);

  // First disconnect the ancestor's frame node from this worker node.
  DisconnectFrameClient(worker_node.get(), ancestor_render_frame_host_id);

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
    const base::UnguessableToken& /* dev_tools_token */) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto worker_node = PerformanceManagerImpl::CreateWorkerNode(
      browser_context_id_, WorkerNode::WorkerType::kShared,
      process_node_source_->GetProcessNode(worker_process_id),
      shared_worker_token);

  bool inserted =
      shared_worker_nodes_.emplace(shared_worker_token, std::move(worker_node))
          .second;
  DCHECK(inserted);
}

void WorkerWatcher::OnBeforeWorkerDestroyed(
    const blink::SharedWorkerToken& shared_worker_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = shared_worker_nodes_.find(shared_worker_token);
  DCHECK(it != shared_worker_nodes_.end());

  auto worker_node = std::move(it->second);
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
    content::GlobalFrameRoutingId render_frame_host_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ConnectFrameClient(GetSharedWorkerNode(shared_worker_token),
                     render_frame_host_id);
}

void WorkerWatcher::OnClientRemoved(
    const blink::SharedWorkerToken& shared_worker_token,
    content::GlobalFrameRoutingId render_frame_host_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DisconnectFrameClient(GetSharedWorkerNode(shared_worker_token),
                        render_frame_host_id);
}

void WorkerWatcher::OnVersionStartedRunning(
    int64_t version_id,
    const content::ServiceWorkerRunningInfo& running_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto insertion_result = service_worker_nodes_.emplace(
      version_id,
      PerformanceManagerImpl::CreateWorkerNode(
          browser_context_id_, WorkerNode::WorkerType::kService,
          process_node_source_->GetProcessNode(running_info.render_process_id),
          running_info.token));
  DCHECK(insertion_result.second);

  // Exclusively for service workers, some notifications for clients
  // (OnControlleeAdded) may have been received before the worker started.
  // Add those clients to the service worker on the PM graph.
  ConnectAllServiceWorkerClients(insertion_result.first->second.get(),
                                 version_id);
}

void WorkerWatcher::OnVersionStoppedRunning(int64_t version_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = service_worker_nodes_.find(version_id);
  DCHECK(it != service_worker_nodes_.end());

  auto service_worker_node = std::move(it->second);

  // First, disconnect all current clients of this service worker.
  DisconnectAllServiceWorkerClients(service_worker_node.get(), version_id);

#if DCHECK_IS_ON()
  DCHECK(!base::Contains(detached_frame_count_per_worker_,
                         service_worker_node.get()));
#endif  // DCHECK_IS_ON()
  PerformanceManagerImpl::DeleteNode(std::move(service_worker_node));

  service_worker_nodes_.erase(it);
}

void WorkerWatcher::OnControlleeAdded(
    int64_t version_id,
    const std::string& client_uuid,
    const content::ServiceWorkerClientInfo& client_info) {
  // TODO(pmonette): Handle service worker clients.
}

void WorkerWatcher::OnControlleeRemoved(int64_t version_id,
                                        const std::string& client_uuid) {
  // TODO(pmonette): Handle service worker clients.
}

void WorkerWatcher::OnControlleeNavigationCommitted(
    int64_t version_id,
    const std::string& client_uuid,
    content::GlobalFrameRoutingId render_frame_host_id) {
  // TODO(pmonette): Handle service worker clients.
}

void WorkerWatcher::ConnectFrameClient(
    WorkerNodeImpl* worker_node,
    content::GlobalFrameRoutingId client_render_frame_host_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(worker_node);

  FrameNodeImpl* frame_node =
      frame_node_source_->GetFrameNode(client_render_frame_host_id);
  // TODO(https://crbug.com/1078161): The client frame's node should always be
  // accessible. If it isn't, this means there is a missing
  // CreatePageNodeForWebContents() somewhere.
  if (!frame_node) {
    RecordWorkerClientFound(false);
#if DCHECK_IS_ON()
    // A call to DisconnectFrameClient() is still expected to be received for
    // this worker and frame pair.
    detached_frame_count_per_worker_[worker_node]++;
#endif  // DCHECK_IS_ON()
    return;
  }

  RecordWorkerClientFound(true);

  ConnectClientOnGraph(worker_node, frame_node);

  // Keep track of the workers that this frame is a client to.
  if (AddChildWorker(client_render_frame_host_id, worker_node)) {
    frame_node_source_->SubscribeToFrameNode(
        client_render_frame_host_id,
        base::BindOnce(&WorkerWatcher::OnBeforeFrameNodeRemoved,
                       base::Unretained(this), client_render_frame_host_id));
  }
}

void WorkerWatcher::DisconnectFrameClient(
    WorkerNodeImpl* worker_node,
    content::GlobalFrameRoutingId client_render_frame_host_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(worker_node);

  FrameNodeImpl* frame_node =
      frame_node_source_->GetFrameNode(client_render_frame_host_id);

  // It's possible that the frame was destroyed before receiving the
  // OnClientRemoved() for all of its child shared worker. Nothing to do in
  // that case because OnBeforeFrameNodeRemoved() took care of removing this
  // client from its child worker nodes.
  //
  // TODO(https://crbug.com/1078161): A second possibility is that it wasn't
  // possible to connect a worker to its client frame.
  if (!frame_node) {
#if DCHECK_IS_ON()
    // These debug only checks are used to ensure that this
    // DisconnectFrameClient() call was still expected even though the client
    // frame node no longer exist.
    auto it = detached_frame_count_per_worker_.find(worker_node);
    DCHECK(it != detached_frame_count_per_worker_.end());

    int& count = it->second;
    DCHECK_GT(count, 0);
    --count;

    if (count == 0)
      detached_frame_count_per_worker_.erase(it);
#endif  // DCHECK_IS_ON()
    return;
  }

  DisconnectClientOnGraph(worker_node, frame_node);

  // Remove |worker_node| from the set of workers that this frame is a client
  // of.
  if (RemoveChildWorker(client_render_frame_host_id, worker_node))
    frame_node_source_->UnsubscribeFromFrameNode(client_render_frame_host_id);
}

void WorkerWatcher::ConnectDedicatedWorkerClient(
    WorkerNodeImpl* worker_node,
    blink::DedicatedWorkerToken client_dedicated_worker_token) {
  DCHECK(worker_node);

  ConnectClientOnGraph(worker_node,
                       GetDedicatedWorkerNode(client_dedicated_worker_token));

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

  // Remove |worker_node| from the set of child workers of this dedicated
  // worker.
  auto it = dedicated_worker_child_workers_.find(client_dedicated_worker_token);
  DCHECK(it != dedicated_worker_child_workers_.end());
  auto& child_workers = it->second;

  size_t removed = child_workers.erase(worker_node);
  DCHECK_EQ(removed, 1u);

  if (child_workers.empty())
    dedicated_worker_child_workers_.erase(it);

  DisconnectClientOnGraph(
      worker_node, GetDedicatedWorkerNode(client_dedicated_worker_token));
}

void WorkerWatcher::ConnectSharedWorkerClient(
    WorkerNodeImpl* worker_node,
    blink::SharedWorkerToken client_shared_worker_token) {
  DCHECK(worker_node);

  ConnectClientOnGraph(worker_node,
                       GetSharedWorkerNode(client_shared_worker_token));

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

  // Remove |worker_node| from the set of child workers of this shared worker.
  auto it = shared_worker_child_workers_.find(client_shared_worker_token);
  DCHECK(it != shared_worker_child_workers_.end());
  auto& child_workers = it->second;

  size_t removed = child_workers.erase(worker_node);
  DCHECK_EQ(removed, 1u);

  if (child_workers.empty())
    shared_worker_child_workers_.erase(it);

  DisconnectClientOnGraph(worker_node,
                          GetSharedWorkerNode(client_shared_worker_token));
}

void WorkerWatcher::ConnectAllServiceWorkerClients(
    WorkerNodeImpl* service_worker_node,
    int64_t version_id) {
  // Nothing to do if there are no clients.
  auto it = service_worker_clients_.find(version_id);
  if (it == service_worker_clients_.end())
    return;

  for (const auto& kv : it->second) {
    const ServiceWorkerClient& client = kv.second;

    switch (client.type()) {
      case blink::mojom::ServiceWorkerClientType::kWindow:
        ConnectFrameClient(service_worker_node, client.GetRenderFrameHostId());
        break;
      case blink::mojom::ServiceWorkerClientType::kDedicatedWorker:
        ConnectDedicatedWorkerClient(service_worker_node,
                                     client.GetDedicatedWorkerToken());
        break;
      case blink::mojom::ServiceWorkerClientType::kSharedWorker:
        ConnectSharedWorkerClient(service_worker_node,
                                  client.GetSharedWorkerToken());
        break;
      case blink::mojom::ServiceWorkerClientType::kAll:
        NOTREACHED();
        break;
    }
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
    const ServiceWorkerClient& client = kv.second;

    switch (client.type()) {
      case blink::mojom::ServiceWorkerClientType::kWindow:
        DisconnectFrameClient(service_worker_node,
                              client.GetRenderFrameHostId());
        break;
      case blink::mojom::ServiceWorkerClientType::kDedicatedWorker:
        DisconnectDedicatedWorkerClient(service_worker_node,
                                        client.GetDedicatedWorkerToken());
        break;
      case blink::mojom::ServiceWorkerClientType::kSharedWorker:
        DisconnectSharedWorkerClient(service_worker_node,
                                     client.GetSharedWorkerToken());
        break;
      case blink::mojom::ServiceWorkerClientType::kAll:
        NOTREACHED();
        break;
    }
  }
}

void WorkerWatcher::OnBeforeFrameNodeRemoved(
    content::GlobalFrameRoutingId render_frame_host_id,
    FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = frame_node_child_workers_.find(render_frame_host_id);
  DCHECK(it != frame_node_child_workers_.end());

  // Clean up all child workers of this frame node.
  base::flat_set<WorkerNodeImpl*> child_workers = std::move(it->second);
  frame_node_child_workers_.erase(it);

  // Disconnect all child workers from |frame_node|.
  DCHECK(!child_workers.empty());
  DisconnectClientsOnGraph(child_workers, frame_node);

#if DCHECK_IS_ON()
  for (WorkerNodeImpl* worker_node : child_workers) {
    // A call to DisconnectFrameClient() is still expected to be received for
    // this frame to all workers in |child_workers|.
    // Note: the [] operator is intentionally used to default initialize the
    // count to zero if needed.
    detached_frame_count_per_worker_[worker_node]++;
  }
#endif  // DCHECK_IS_ON()
}

bool WorkerWatcher::AddChildWorker(
    content::GlobalFrameRoutingId render_frame_host_id,
    WorkerNodeImpl* child_worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto insertion_result =
      frame_node_child_workers_.insert({render_frame_host_id, {}});

  auto& child_workers = insertion_result.first->second;
  bool inserted = child_workers.insert(child_worker_node).second;
  DCHECK(inserted);

  return insertion_result.second;
}

bool WorkerWatcher::RemoveChildWorker(
    content::GlobalFrameRoutingId render_frame_host_id,
    WorkerNodeImpl* child_worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = frame_node_child_workers_.find(render_frame_host_id);
  DCHECK(it != frame_node_child_workers_.end());
  auto& child_workers = it->second;

  size_t removed = child_workers.erase(child_worker_node);
  DCHECK_EQ(removed, 1u);

  if (child_workers.empty()) {
    frame_node_child_workers_.erase(it);
    return true;
  }
  return false;
}

WorkerNodeImpl* WorkerWatcher::GetDedicatedWorkerNode(
    const blink::DedicatedWorkerToken& dedicated_worker_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = dedicated_worker_nodes_.find(dedicated_worker_token);
  if (it == dedicated_worker_nodes_.end()) {
    NOTREACHED();
    return nullptr;
  }
  return it->second.get();
}

WorkerNodeImpl* WorkerWatcher::GetSharedWorkerNode(
    const blink::SharedWorkerToken& shared_worker_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = shared_worker_nodes_.find(shared_worker_token);
  if (it == shared_worker_nodes_.end()) {
    NOTREACHED();
    return nullptr;
  }
  return it->second.get();
}

WorkerNodeImpl* WorkerWatcher::GetServiceWorkerNode(int64_t version_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = service_worker_nodes_.find(version_id);
  if (it == service_worker_nodes_.end()) {
    return nullptr;
  }
  return it->second.get();
}

}  // namespace performance_manager

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_WORKER_NODE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_WORKER_NODE_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/observer_list_types.h"
#include "base/types/token_type.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/graph/node.h"
#include "components/performance_manager/public/graph/node_set_view.h"
#include "components/performance_manager/public/resource_attribution/worker_context.h"
#include "third_party/blink/public/common/tokens/tokens.h"

class GURL;

namespace url {
class Origin;
}

namespace performance_manager {

class WorkerNodeObserver;
class FrameNode;
class ProcessNode;

using execution_context_priority::PriorityAndReason;

// Represents a running instance of a WorkerGlobalScope.
// See https://developer.mozilla.org/en-US/docs/Web/API/WorkerGlobalScope.
//
// A worker is the equivalent of a thread on the web platform. To do
// asynchronous work, a frame can create a dedicated worker or a shared worker
// with Javascript. Those workers can now be used by sending an asynchronous
// message using the postMessage() method, and replies can be received by
// registering a message handler on the worker object.
//
// One notable special case is that dedicated workers can be nested. That means
// that a dedicated worker can create another dedicated worker which is then
// only accessible from the parent worker.
//
// Service workers are different. Instead of being created by the javascript
// when needed, a service worker is registered once and affects all frames and
// dedicated/shared workers whose URL matches the scope of the service worker.
// A service worker is mainly used to intercept network requests and optionally
// serve the responses from a cache, making the web site work offline.
//
// A client, from the point of view of the worker, is the frame or worker that
// caused the worker to start running, either because it explicitly created it,
// or a service worker is registered to handle their network requests.
class WorkerNode : public TypedNode<WorkerNode> {
 public:
  using NodeSet = base::flat_set<const Node*>;
  template <class ReturnType>
  using NodeSetView = NodeSetView<NodeSet, ReturnType>;

  // The different possible worker types.
  enum class WorkerType {
    kDedicated,
    kShared,
    kService,
  };

  using Observer = WorkerNodeObserver;
  class ObserverDefaultImpl;

  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kWorker; }

  WorkerNode();

  WorkerNode(const WorkerNode&) = delete;
  WorkerNode& operator=(const WorkerNode&) = delete;

  ~WorkerNode() override;

  // Returns the worker type. Note that this is different from the NodeTypeEnum.
  virtual WorkerType GetWorkerType() const = 0;

  // Returns the unique ID of the browser context that this worker belongs to.
  virtual const std::string& GetBrowserContextID() const = 0;

  // Returns the process node to which this worker belongs. This is a constant
  // over the lifetime of the frame.
  virtual const ProcessNode* GetProcessNode() const = 0;

  // Returns the unique token identifying this worker.
  virtual const blink::WorkerToken& GetWorkerToken() const = 0;

  // Gets the unique token identifying this node for resource attribution. This
  // token will not be reused after the node is destroyed.
  virtual resource_attribution::WorkerContext GetResourceContext() const = 0;

  // Returns the URL of the worker script. This is the final response URL which
  // takes into account redirections. Note that for dedicated workers, this will
  // be empty unless the PlzDedicatedWorker feature is enabled.
  virtual const GURL& GetURL() const = 0;

  // Returns the worker's security origin. This will be set even if GetURL() is
  // empty. See docs/security/origin-vs-url.md for the difference between GURL
  // and Origin.
  virtual const url::Origin& GetOrigin() const = 0;

  // Returns the current priority of the worker, and the reason for the worker
  // having that particular priority.
  virtual const PriorityAndReason& GetPriorityAndReason() const = 0;

  // Returns the frames that are clients of this worker.
  virtual NodeSetView<const FrameNode*> GetClientFrames() const = 0;

  // Returns the workers that are clients of this worker.
  // There are 2 cases where this is possible:
  // - A dedicated worker can create nested workers. The parent worker becomes
  //   one of its client worker.
  // - A dedicated worker or a shared worker will become a client of the service
  //   worker that handles their network requests.
  virtual NodeSetView<const WorkerNode*> GetClientWorkers() const = 0;

  // Returns the child workers of this worker.
  // There are 2 cases where a worker can be the child of another worker:
  // - A dedicated worker can create nested workers. The nested worker becomes
  //   a child worker of the parent.
  // - A service worker will become a child worker of every worker for which
  //   it handles network requests.
  virtual NodeSetView<const WorkerNode*> GetChildWorkers() const = 0;

  // TODO(joenotcharles): Move the resource usage estimates to a separate
  // class.

  // Returns the most recently estimated resident set of the worker, in
  // kilobytes. This is an estimate because RSS is computed by process, and a
  // process can host multiple workers.
  virtual uint64_t GetResidentSetKbEstimate() const = 0;

  // Returns the most recently estimated private footprint of the worker, in
  // kilobytes. This is an estimate because PMF is computed by process, and a
  // process can host multiple workers.
  virtual uint64_t GetPrivateFootprintKbEstimate() const = 0;
};

// Pure virtual observer interface. Derive from this if you want to be forced to
// implement the entire interface.
class WorkerNodeObserver : public base::CheckedObserver {
 public:
  WorkerNodeObserver();

  WorkerNodeObserver(const WorkerNodeObserver&) = delete;
  WorkerNodeObserver& operator=(const WorkerNodeObserver&) = delete;

  ~WorkerNodeObserver() override;

  // Node lifetime notifications.

  // Called when a |worker_node| is added to the graph. Observers must not make
  // any property changes or cause re-entrant notifications during the scope of
  // this call. Instead, make property changes via a separate posted task.
  virtual void OnWorkerNodeAdded(const WorkerNode* worker_node) = 0;

  // Called before a |worker_node| is removed from the graph. Observers must not
  // make any property changes or cause re-entrant notifications during the
  // scope of this call.
  virtual void OnBeforeWorkerNodeRemoved(const WorkerNode* worker_node) = 0;

  // Notifications of property changes.

  // Invoked when the final url of the worker script has been determined, which
  // happens when the script has finished loading. Note that for dedicated
  // workers, this won't be called unless the PlzDedicatedWorker feature is
  // enabled.
  virtual void OnFinalResponseURLDetermined(const WorkerNode* worker_node) = 0;

  // Invoked before |client_frame_node| becomes a client of |worker_node|. This
  // is the last chance to traverse the graph and capture state that doesn't
  // include the worker/frame client relationship.
  virtual void OnBeforeClientFrameAdded(const WorkerNode* worker_node,
                                        const FrameNode* client_frame_node) = 0;

  // Invoked after |client_frame_node| becomes a client of |worker_node|.
  virtual void OnClientFrameAdded(const WorkerNode* worker_node,
                                  const FrameNode* client_frame_node) = 0;

  // Invoked when |client_frame_node| is no longer a client of |worker_node|.
  virtual void OnBeforeClientFrameRemoved(
      const WorkerNode* worker_node,
      const FrameNode* client_frame_node) = 0;

  // Invoked before |client_worker_node| becomes a client of |worker_node|. This
  // is the last chance to traverse the graph and capture state that doesn't
  // include the worker/worker client relationship.
  virtual void OnBeforeClientWorkerAdded(
      const WorkerNode* worker_node,
      const WorkerNode* client_worker_node) = 0;

  // Invoked after |client_worker_node| becomes a client of |worker_node|.
  virtual void OnClientWorkerAdded(const WorkerNode* worker_node,
                                   const WorkerNode* client_worker_node) = 0;

  // Invoked when |client_worker_node| is no longer a client of |worker_node|.
  virtual void OnBeforeClientWorkerRemoved(
      const WorkerNode* worker_node,
      const WorkerNode* client_worker_node) = 0;

  // Invoked when the worker priority and reason changes.
  virtual void OnPriorityAndReasonChanged(
      const WorkerNode* worker_node,
      const PriorityAndReason& previous_value) = 0;
};

// Default implementation of observer that provides dummy versions of each
// function. Derive from this if you only need to implement a few of the
// functions.
class WorkerNode::ObserverDefaultImpl : public WorkerNodeObserver {
 public:
  ObserverDefaultImpl();

  ObserverDefaultImpl(const ObserverDefaultImpl&) = delete;
  ObserverDefaultImpl& operator=(const ObserverDefaultImpl&) = delete;

  ~ObserverDefaultImpl() override;

  // WorkerNodeObserver implementation:

  // Called when a |worker_node| is added to the graph.
  void OnWorkerNodeAdded(const WorkerNode* worker_node) override {}
  void OnBeforeWorkerNodeRemoved(const WorkerNode* worker_node) override {}
  void OnFinalResponseURLDetermined(const WorkerNode* worker_node) override {}
  void OnBeforeClientFrameAdded(const WorkerNode* worker_node,
                                const FrameNode* client_frame_node) override {}
  void OnClientFrameAdded(const WorkerNode* worker_node,
                          const FrameNode* client_frame_node) override {}
  void OnBeforeClientFrameRemoved(const WorkerNode* worker_node,
                                  const FrameNode* client_frame_node) override {
  }
  void OnBeforeClientWorkerAdded(
      const WorkerNode* worker_node,
      const WorkerNode* client_worker_node) override {}
  void OnClientWorkerAdded(const WorkerNode* worker_node,
                           const WorkerNode* client_worker_node) override {}
  void OnBeforeClientWorkerRemoved(
      const WorkerNode* worker_node,
      const WorkerNode* client_worker_node) override {}
  void OnPriorityAndReasonChanged(
      const WorkerNode* worker_node,
      const PriorityAndReason& previous_value) override {}
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_WORKER_NODE_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_WORKER_NODE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_WORKER_NODE_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "components/performance_manager/public/graph/node.h"

class GURL;

namespace base {
class UnguessableToken;
}

namespace performance_manager {

class WorkerNodeObserver;
class FrameNode;
class ProcessNode;

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
class WorkerNode : public Node {
 public:
  // The different possible worker types.
  enum class WorkerType {
    kDedicated,
    kShared,
    kService,
  };

  using Observer = WorkerNodeObserver;
  class ObserverDefaultImpl;

  WorkerNode();
  ~WorkerNode() override;

  // Returns the worker type. Note that this is different from the NodeTypeEnum.
  virtual WorkerType GetWorkerType() const = 0;

  // Returns the unique ID of the browser context that this worker belongs to.
  virtual const std::string& GetBrowserContextID() const = 0;

  // Returns the process node to which this worker belongs. This is a constant
  // over the lifetime of the frame.
  virtual const ProcessNode* GetProcessNode() const = 0;

  // Returns the URL of the worker script.
  virtual const GURL& GetURL() const = 0;

  // Returns the dev tools token for this worker.
  virtual const base::UnguessableToken& GetDevToolsToken() const = 0;

  // Returns the frames that are clients of this worker.
  virtual const base::flat_set<const FrameNode*> GetClientFrames() const = 0;

  // Returns the workers that are clients of this worker.
  // There are 2 cases where this is possible:
  // - A dedicated worker can create nested workers. The parent worker becomes
  //   one of its client worker.
  // - A dedicated worker or a shared worker will become a client of the service
  //   worker that handles their network requests.
  virtual const base::flat_set<const WorkerNode*> GetClientWorkers() const = 0;

  // Returns the child workers of this worker.
  // There are 2 cases where a worker can be the child of another worker:
  // - A dedicated worker can create nested workers. The nested worker becomes
  //   a child worker of the parent.
  // - A service worker will become a child worker of every worker for which
  //   it handles network requests.
  virtual const base::flat_set<const WorkerNode*> GetChildWorkers() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkerNode);
};

// Pure virtual observer interface. Derive from this if you want to be forced to
// implement the entire interface.
class WorkerNodeObserver {
 public:
  WorkerNodeObserver();
  virtual ~WorkerNodeObserver();

  // Node lifetime notifications.

  // Called when a |worker_node| is added to the graph.
  virtual void OnWorkerNodeAdded(const WorkerNode* worker_node) = 0;

  // Called before a |worker_node| is removed from the graph.
  virtual void OnBeforeWorkerNodeRemoved(const WorkerNode* worker_node) = 0;

  // Notifications of property changes.

  // Invoked when |client_frame_node| becomes a client of |worker_node|.
  virtual void OnClientFrameAdded(const WorkerNode* worker_node,
                                  const FrameNode* client_frame_node) = 0;

  // Invoked when |client_frame_node| is no longer a client of |worker_node|.
  virtual void OnBeforeClientFrameRemoved(
      const WorkerNode* worker_node,
      const FrameNode* client_frame_node) = 0;

  // Invoked when |client_worker_node| becomes a client of |worker_node|.
  virtual void OnClientWorkerAdded(const WorkerNode* worker_node,
                                   const WorkerNode* client_worker_node) = 0;

  // Invoked when |client_worker_node| is no longer a client of |worker_node|.
  virtual void OnBeforeClientWorkerRemoved(
      const WorkerNode* worker_node,
      const WorkerNode* client_worker_node) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkerNodeObserver);
};

// Default implementation of observer that provides dummy versions of each
// function. Derive from this if you only need to implement a few of the
// functions.
class WorkerNode::ObserverDefaultImpl : public WorkerNodeObserver {
 public:
  ObserverDefaultImpl();
  ~ObserverDefaultImpl() override;

  // WorkerNodeObserver implementation:

  // Called when a |worker_node| is added to the graph.
  void OnWorkerNodeAdded(const WorkerNode* worker_node) override {}
  void OnBeforeWorkerNodeRemoved(const WorkerNode* worker_node) override {}
  void OnClientFrameAdded(const WorkerNode* worker_node,
                          const FrameNode* client_frame_node) override {}
  void OnBeforeClientFrameRemoved(const WorkerNode* worker_node,
                                  const FrameNode* client_frame_node) override {
  }
  void OnClientWorkerAdded(const WorkerNode* worker_node,
                           const WorkerNode* client_worker_node) override {}
  void OnBeforeClientWorkerRemoved(
      const WorkerNode* worker_node,
      const WorkerNode* client_worker_node) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ObserverDefaultImpl);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_WORKER_NODE_H_

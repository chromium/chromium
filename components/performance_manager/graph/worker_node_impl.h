// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_WORKER_NODE_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_WORKER_NODE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "url/gurl.h"

namespace performance_manager {

class FrameNodeImpl;
class ProcessNodeImpl;

class WorkerNodeImpl
    : public PublicNodeImpl<WorkerNodeImpl, WorkerNode>,
      public TypedNodeBase<WorkerNodeImpl, WorkerNode, WorkerNodeObserver> {
 public:
  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kWorker; }

  WorkerNodeImpl(GraphImpl* graph,
                 const std::string& browser_context_id,
                 WorkerType worker_type,
                 ProcessNodeImpl* process_node,
                 const GURL& url,
                 const base::UnguessableToken& dev_tools_token);
  ~WorkerNodeImpl() override;

  // Invoked when a frame starts/stops being a client of this worker.
  void AddClientFrame(FrameNodeImpl* frame_node);
  void RemoveClientFrame(FrameNodeImpl* frame_node);

  // Invoked when a worker starts/stops being a client of this worker.
  void AddClientWorker(WorkerNodeImpl* worker_node);
  void RemoveClientWorker(WorkerNodeImpl* worker_node);

  // Getters for const properties. These can be called from any thread.
  const std::string& browser_context_id() const;
  WorkerType worker_type() const;
  ProcessNodeImpl* process_node() const;
  const GURL& url() const;
  const base::UnguessableToken& dev_tools_token() const;

  // Getters for non-const properties. These are not thread safe.
  const base::flat_set<FrameNodeImpl*>& client_frames() const;
  const base::flat_set<WorkerNodeImpl*>& client_workers() const;
  const base::flat_set<WorkerNodeImpl*>& child_workers() const;

 private:
  void JoinGraph() override;
  void LeaveGraph() override;

  // WorkerNode: These are private so that users of the
  // impl use the private getters rather than the public interface.
  WorkerType GetWorkerType() const override;
  const std::string& GetBrowserContextID() const override;
  const ProcessNode* GetProcessNode() const override;
  const GURL& GetURL() const override;
  const base::UnguessableToken& GetDevToolsToken() const override;
  const base::flat_set<const FrameNode*> GetClientFrames() const override;
  const base::flat_set<const WorkerNode*> GetClientWorkers() const override;
  const base::flat_set<const WorkerNode*> GetChildWorkers() const override;

  // Invoked when |worker_node| becomes a child of this worker.
  void AddChildWorker(WorkerNodeImpl* worker_node);
  void RemoveChildWorker(WorkerNodeImpl* worker_node);

  // The unique ID of the browser context that this worker belongs to.
  const std::string browser_context_id_;

  // The type of this worker.
  const WorkerType worker_type_;

  // The process in which this worker lives.
  ProcessNodeImpl* const process_node_;

  // The URL of the worker script.
  const GURL url_;

  // A unique identifier shared with all representations of this node across
  // content and blink. The token is only defined by the browser process and
  // is never sent back from the renderer in control calls.
  const base::UnguessableToken dev_tools_token_;

  // Frames that are clients of this worker.
  base::flat_set<FrameNodeImpl*> client_frames_;

  // Other workers that are clients of this worker. See the declaration of
  // WorkerNode for a distinction between client workers and child workers.
  base::flat_set<WorkerNodeImpl*> client_workers_;

  // The child workers of this worker. See the declaration of WorkerNode for a
  // distinction between client workers and child workers.
  base::flat_set<WorkerNodeImpl*> child_workers_;

  DISALLOW_COPY_AND_ASSIGN(WorkerNodeImpl);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_WORKER_NODE_IMPL_H_

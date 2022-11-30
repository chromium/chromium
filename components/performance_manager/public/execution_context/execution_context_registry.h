// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_EXECUTION_CONTEXT_REGISTRY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_EXECUTION_CONTEXT_REGISTRY_H_

#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {

class Graph;

namespace execution_context {

class ExecutionContext;
class ExecutionContextObserver;

// The ExecutionContextRegistry is a per-graph class that allows for observers
// to be registered, and for ExecutionContexts to be looked up by their tokens.
// An instance of the registry must be passed to the Graph prior to any nodes
// being created.
class ExecutionContextRegistry {
 public:
  ExecutionContextRegistry();
  ExecutionContextRegistry(const ExecutionContextRegistry&) = delete;
  ExecutionContextRegistry& operator=(const ExecutionContextRegistry&) = delete;
  virtual ~ExecutionContextRegistry();

  // Returns the instance of the ExecutionContextRegistry for the given
  // Graph.
  static ExecutionContextRegistry* GetFromGraph(Graph* graph);

  // Returns the ExecutionContext associated with a node.
  static const ExecutionContext* GetExecutionContextForFrameNode(
      const FrameNode* frame_node);
  static const ExecutionContext* GetExecutionContextForWorkerNode(
      const WorkerNode* worker_node);

  // Adds an observer to the registry. The observer needs to be removed before
  // the registry is torn down.
  virtual void AddObserver(ExecutionContextObserver* observer) = 0;

  // Determines if an observer is in the registry.
  virtual bool HasObserver(ExecutionContextObserver* observer) const = 0;

  // Removes an observer from the registry.
  virtual void RemoveObserver(ExecutionContextObserver* observer) = 0;

  // Looks up an ExecutionContext by token. Returns nullptr if no such context
  // exists.
  virtual const ExecutionContext* GetExecutionContextByToken(
      const blink::ExecutionContextToken& token) = 0;

  // Does a typed lookup of a FrameNode ExecutionContext by FrameToken, returns
  // nullptr if no such FrameNode exists.
  virtual const FrameNode* GetFrameNodeByFrameToken(
      const blink::LocalFrameToken& token) = 0;

  // Does a typed lookup of a WorkerNode ExecutionContext by its DevToolsToken,
  // returns nullptr if no such WorkerNode exists.
  virtual const WorkerNode* GetWorkerNodeByWorkerToken(
      const blink::WorkerToken& token) = 0;

  // Returns the ExecutionContext associated with a node. These provide
  // implementations for the static functions above, which should be preferred.
  virtual const ExecutionContext* GetExecutionContextForFrameNodeImpl(
      const FrameNode* frame_node) = 0;
  virtual const ExecutionContext* GetExecutionContextForWorkerNodeImpl(
      const WorkerNode* worker_node) = 0;
};

}  // namespace execution_context
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_EXECUTION_CONTEXT_REGISTRY_H_
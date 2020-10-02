// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_H_

#include <memory>

#include "base/optional.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/v8_memory/v8_context_tracker_types.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {
namespace v8_memory {

// Forward declaration.
namespace internal {
class V8ContextTrackerDataStore;
}  // namespace internal

// A class that tracks individual V8Contexts in renderers as they go through
// their lifecycle. This tracks information such as detached (leaked) contexts
// and remote frame attribution, for surfacing in the performance.measureMemory
// API. This information is tracked per-process in ProcessNode-attached data.
// The tracker lets you query a V8ContextToken and retrieve information about
// that context, including its iframe attributes and associated
// ExecutionContext.
//
// Note that this component relies on the ExecutionContextRegistry having been
// added to the Graph.
class V8ContextTracker
    : public execution_context::ExecutionContextObserverDefaultImpl,
      public GraphObserver,
      public GraphOwned,
      public GraphRegisteredImpl<V8ContextTracker>,
      public NodeDataDescriberDefaultImpl,
      public ProcessNode::ObserverDefaultImpl {
 public:
  using DataStore = internal::V8ContextTrackerDataStore;

  // Data about an individual ExecutionContext. Note that this information can
  // outlive the ExecutionContext itself, and in that case it stores information
  // about the last known state of the ExecutionContext prior to it being
  // torn down in a renderer.
  struct ExecutionContextState {
    ExecutionContextState() = delete;
    ExecutionContextState(
        const blink::ExecutionContextToken& token,
        const base::Optional<IframeAttributionData>& iframe_attribution_data);
    ExecutionContextState(const ExecutionContextState&) = delete;
    ExecutionContextState& operator=(const ExecutionContextState&) = delete;
    virtual ~ExecutionContextState();

    // Returns the associated execution_context::ExecutionContext (which wraps
    // the underlying FrameNode or WorkerNode associated with this context) *if*
    // the node is available.
    const execution_context::ExecutionContext* GetExecutionContext() const;

    // The token identifying this context.
    const blink::ExecutionContextToken token;

    // The iframe attribution data most recently associated with this context.
    // This is sometimes only available asynchronously so is optional. Note that
    // this value can change over time, but will generally reflect the most up
    // to date data (with some lag).
    base::Optional<IframeAttributionData> iframe_attribution_data;

    // Whether or not the corresponding blink::ExecutionContext has been
    // destroyed. This occurs when the main V8Context associated with this
    // execution context has itself become detached. This starts as false and
    // can transition to true exactly once.
    bool destroyed = false;
  };

  struct V8ContextState {
    V8ContextState() = delete;
    V8ContextState(const V8ContextDescription& description,
                   ExecutionContextState* execution_context_state);
    V8ContextState(const V8ContextState&) = delete;
    V8ContextState& operator=(const V8ContextState&) = delete;
    virtual ~V8ContextState();

    // The full description of this context.
    const V8ContextDescription description;

    // A pointer to the upstream ExecutionContextState that this V8Context is
    // associated with. Note that this can be nullptr for V8Contexts that are
    // not associated with an ExecutionContext.
    ExecutionContextState* const execution_context_state;

    // Whether or not this context is detached. A context becomes detached
    // when the blink::ExecutionContext it was associated with is torn down.
    // When a V8Context remains detached for a long time (is not collected by
    // GC) it is effectively a leak (it is being kept alive by a stray
    // cross-context reference). This starts as false and can transition to
    // true exactly once.
    bool detached = false;
  };

  V8ContextTracker();
  ~V8ContextTracker() final;

  DataStore* data_store() const { return data_store_.get(); }

  // Returns the ExecutionContextState for the given token, nullptr if none
  // exists.
  const ExecutionContextState* GetExecutionContextState(
      const blink::ExecutionContextToken& token) const;

  // Returns V8ContextState for the given token, nullptr if none exists.
  const V8ContextState* GetV8ContextState(
      const blink::V8ContextToken& token) const;

 private:
  // Implementation of execution_context::ExecutionContextObserverDefaultImpl.
  void OnBeforeExecutionContextRemoved(
      const execution_context::ExecutionContext* ec) final;

  // Implementation of GraphObserver.
  void OnBeforeGraphDestroyed(Graph* graph) final;

  // Implementation of GraphOwned.
  void OnPassedToGraph(Graph* graph) final;
  void OnTakenFromGraph(Graph* graph) final;

  // Implementation of NodeDataDescriber. We have things to say about
  // execution contexts (frames and workers), as well as processes.
  base::Value DescribeFrameNodeData(const FrameNode* node) const final;
  base::Value DescribeProcessNodeData(const ProcessNode* node) const final;
  base::Value DescribeWorkerNodeData(const WorkerNode* node) const final;

  // Implementation of ProcessNode::ObserverDefaultImpl.
  void OnBeforeProcessNodeRemoved(const ProcessNode* node) final;

  // Stores Chrome-wide data store used by the tracking.
  std::unique_ptr<DataStore> data_store_;
};

}  // namespace v8_memory
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/mojom/v8_contexts.mojom.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {

class FrameNodeImpl;
class ProcessNodeImpl;

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
class V8ContextTracker final
    : public execution_context::ExecutionContextObserverDefaultImpl,
      public ProcessNode::ObserverDefaultImpl,
      public GraphOwnedAndRegistered<V8ContextTracker>,
      public NodeDataDescriberDefaultImpl {
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
        mojom::IframeAttributionDataPtr iframe_attribution_data);
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
    mojom::IframeAttributionDataPtr iframe_attribution_data;

    // Whether or not the corresponding blink::ExecutionContext has been
    // destroyed. This occurs when the main V8Context associated with this
    // execution context has itself become detached. This starts as false and
    // can transition to true exactly once.
    bool destroyed = false;
  };

  struct V8ContextState {
    V8ContextState() = delete;
    V8ContextState(const mojom::V8ContextDescription& description,
                   ExecutionContextState* execution_context_state);
    V8ContextState(const V8ContextState&) = delete;
    V8ContextState& operator=(const V8ContextState&) = delete;
    virtual ~V8ContextState();

    // The full description of this context.
    const mojom::V8ContextDescription description;

    // A pointer to the upstream ExecutionContextState that this V8Context is
    // associated with. Note that this can be nullptr for V8Contexts that are
    // not associated with an ExecutionContext.
    raw_ptr<ExecutionContextState> execution_context_state;

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

  //////////////////////////////////////////////////////////////////////////////
  // The following functions handle inbound IPC, and are only meant to be
  // called from ProcessNodeImpl and FrameNodeImpl (hence the use of PassKey).

  // Notifies the context tracker of a V8Context being created in a renderer
  // process. If the context is associated with an ExecutionContext (EC) then
  // |description.execution_context_token| will be provided. If the EC is a
  // frame, and the parent of that frame is also in the same process, then
  // |iframe_attribution_data| will be provided, otherwise these will be empty.
  // In the case where they are empty the iframe data will be provided by a
  // separate call to OnIframeAttached() from the process hosting the
  // parent frame. See the V8ContextWorldType enum for a description of the
  // relationship between world types, world names and execution contexts.
  void OnV8ContextCreated(
      base::PassKey<ProcessNodeImpl> key,
      ProcessNodeImpl* process_node,
      const mojom::V8ContextDescription& description,
      mojom::IframeAttributionDataPtr iframe_attribution_data);

  // Notifies the tracker that a V8Context is now detached from its associated
  // ExecutionContext (if one was provided during OnV8ContextCreated). If the
  // context stays detached for a long time this is indicative of a Javascript
  // leak, with the context being kept alive by a stray reference from another
  // context. All ExecutionContext-associated V8Contexts will have this method
  // called before they are destroyed, and it will not be called for other
  // V8Contexts (they are never considered detached).
  void OnV8ContextDetached(base::PassKey<ProcessNodeImpl> key,
                           ProcessNodeImpl* process_node,
                           const blink::V8ContextToken& v8_context_token);

  // Notifies the tracker that a V8Context has been garbage collected. This will
  // only be called after OnV8ContextDetached if the OnV8ContextCreated had a
  // non-empty |execution_context_token|.
  void OnV8ContextDestroyed(base::PassKey<ProcessNodeImpl> key,
                            ProcessNodeImpl* process_node,
                            const blink::V8ContextToken& v8_context_token);

  // Notifies the tracker that a RemoteFrame child with a LocalFrame parent was
  // created in a renderer, providing the iframe.id and iframe.src from the
  // parent point of view. This will decorate the ExecutionContextData of the
  // appropriate child frame. We require the matching OnRemoteIframeDetached to
  // be called for bookkeeping. This should only be called once for a given
  // |remote_frame_token|.
  void OnRemoteIframeAttached(
      base::PassKey<ProcessNodeImpl> key,
      FrameNodeImpl* parent_frame_node,
      const blink::RemoteFrameToken& remote_frame_token,
      mojom::IframeAttributionDataPtr iframe_attribution_data);

  // TODO(chrisha): Add OnRemoteIframeAttributesChanged support.

  // Notifies the tracker that a RemoteFrame child with a LocalFrame parent was
  // detached from an iframe element in a renderer. This is used to cleanup
  // iframe data that is being tracked due to a previous call to
  // OnIframeAttached, unless the data was adopted by a call to
  // OnV8ContextCreated. Should only be called once for a given
  // |remote_frame_token|, and only after a matching "OnRemoteIframeAttached"
  // call.
  void OnRemoteIframeDetached(
      base::PassKey<ProcessNodeImpl> key,
      FrameNodeImpl* parent_frame_node,
      const blink::RemoteFrameToken& remote_frame_token);

  //////////////////////////////////////////////////////////////////////////////
  // The following functions are for testing only.

  void OnRemoteIframeAttachedForTesting(
      FrameNodeImpl* frame_node,
      FrameNodeImpl* parent_frame_node,
      const blink::RemoteFrameToken& remote_frame_token,
      mojom::IframeAttributionDataPtr iframe_attribution_data);

  void OnRemoteIframeDetachedForTesting(
      FrameNodeImpl* parent_frame_node,
      const blink::RemoteFrameToken& remote_frame_token);

  // System wide metrics.
  size_t GetExecutionContextCountForTesting() const;
  size_t GetV8ContextCountForTesting() const;
  size_t GetDestroyedExecutionContextCountForTesting() const;
  size_t GetDetachedV8ContextCountForTesting() const;

 private:
  // Implementation of execution_context::ExecutionContextObserverDefaultImpl.
  void OnBeforeExecutionContextRemoved(
      const execution_context::ExecutionContext* ec) final;

  // Implementation of GraphOwned.
  void OnPassedToGraph(Graph* graph) final;
  void OnTakenFromGraph(Graph* graph) final;

  // Implementation of NodeDataDescriber. We have things to say about
  // execution contexts (frames and workers), as well as processes.
  base::Value::Dict DescribeFrameNodeData(const FrameNode* node) const final;
  base::Value::Dict DescribeProcessNodeData(
      const ProcessNode* node) const final;
  base::Value::Dict DescribeWorkerNodeData(const WorkerNode* node) const final;

  // Implementation of ProcessNode::ObserverDefaultImpl.
  void OnBeforeProcessNodeRemoved(const ProcessNode* node) final;

  // OnIframeAttached bounces over to the UI thread to
  // lookup the RenderFrameHost* associated with a given RemoteFrameToken,
  // landing here.
  void OnRemoteIframeAttachedImpl(
      mojo::ReportBadMessageCallback bad_message_callback,
      FrameNodeImpl* frame_node,
      FrameNodeImpl* parent_frame_node,
      const blink::RemoteFrameToken& remote_frame_token,
      mojom::IframeAttributionDataPtr iframe_attribution_data);

  // To maintain strict ordering with OnRemoteIframeAttached events, detached
  // events also detour through the UI thread to arrive here.
  void OnRemoteIframeDetachedImpl(
      FrameNodeImpl* parent_frame_node,
      const blink::RemoteFrameToken& remote_frame_token);

  // Stores Chrome-wide data store used by the tracking.
  std::unique_ptr<DataStore> data_store_;
};

}  // namespace v8_memory
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/v8_context_tracker.h"

#include "base/values.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/v8_memory/v8_context_tracker_internal.h"

namespace performance_manager {
namespace v8_memory {

////////////////////////////////////////////////////////////////////////////////
// V8ContextTracker::ExecutionContextState implementation:

V8ContextTracker::ExecutionContextState::ExecutionContextState(
    const blink::ExecutionContextToken& token,
    const base::Optional<IframeAttributionData>& iframe_attribution_data)
    : token(token), iframe_attribution_data(iframe_attribution_data) {}

V8ContextTracker::ExecutionContextState::~ExecutionContextState() = default;

////////////////////////////////////////////////////////////////////////////////
// V8ContextTracker::V8ContextState implementation:

V8ContextTracker::V8ContextState::V8ContextState(
    const V8ContextDescription& description,
    ExecutionContextState* execution_context_state)
    : description(description),
      execution_context_state(execution_context_state) {}

V8ContextTracker::V8ContextState::~V8ContextState() = default;

////////////////////////////////////////////////////////////////////////////////
// V8ContextTracker implementation:

V8ContextTracker::V8ContextTracker()
    : data_store_(std::make_unique<DataStore>()) {}

V8ContextTracker::~V8ContextTracker() = default;

void V8ContextTracker::OnBeforeExecutionContextRemoved(
    const execution_context::ExecutionContext* ec) {
  DCHECK_ON_GRAPH_SEQUENCE(ec->GetGraph());
  // TODO(chrisha): Implement me.
}

void V8ContextTracker::OnBeforeGraphDestroyed(Graph* graph) {
  DCHECK_ON_GRAPH_SEQUENCE(graph);
  // Remove ourselves from the execution context registry observer list here as
  // it may get torn down before our OnTakenFromGraph is called. This is also
  // called from "OnTakenFromGraph", so it is resistant to the
  // ExecutionContextRegistry no longer existing.
  auto* registry =
      execution_context::ExecutionContextRegistry::GetFromGraph(graph);
  if (registry && registry->HasObserver(this))
    registry->RemoveObserver(this);
}

void V8ContextTracker::OnPassedToGraph(Graph* graph) {
  DCHECK_ON_GRAPH_SEQUENCE(graph);

  graph->AddGraphObserver(this);
  graph->AddProcessNodeObserver(this);
  graph->RegisterObject(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           "V8ContextTracker");
  auto* registry =
      execution_context::ExecutionContextRegistry::GetFromGraph(graph);
  // We expect the registry to exist before we are passed to the graph.
  DCHECK(registry);
  registry->AddObserver(this);
}

void V8ContextTracker::OnTakenFromGraph(Graph* graph) {
  DCHECK_ON_GRAPH_SEQUENCE(graph);

  // Call OnBeforeGraphDestroyed as well. This unregisters us from the
  // ExecutionContextRegistry in case we're being removed from the graph
  // prior to its destruction.
  OnBeforeGraphDestroyed(graph);

  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  graph->UnregisterObject(this);
  graph->RemoveProcessNodeObserver(this);
  graph->RemoveGraphObserver(this);
}

base::Value V8ContextTracker::DescribeFrameNodeData(
    const FrameNode* node) const {
  DCHECK_ON_GRAPH_SEQUENCE(node->GetGraph());
  base::Value dict(base::Value::Type::DICTIONARY);
  // TODO(chrisha): Implement me.
  return dict;
}

base::Value V8ContextTracker::DescribeProcessNodeData(
    const ProcessNode* node) const {
  DCHECK_ON_GRAPH_SEQUENCE(node->GetGraph());
  base::Value dict(base::Value::Type::DICTIONARY);
  // TODO(chrisha): Implement me.
  return dict;
}

base::Value V8ContextTracker::DescribeWorkerNodeData(
    const WorkerNode* node) const {
  DCHECK_ON_GRAPH_SEQUENCE(node->GetGraph());
  base::Value dict(base::Value::Type::DICTIONARY);
  // TODO(chrisha): Implement me.
  return dict;
}

void V8ContextTracker::OnBeforeProcessNodeRemoved(const ProcessNode* node) {
  DCHECK_ON_GRAPH_SEQUENCE(node->GetGraph());
  // TODO(chrisha): Implement me by deleting all knowledge of execution contexts
  // and v8 contexts in this process!
}

}  // namespace v8_memory
}  // namespace performance_manager

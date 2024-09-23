// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/process_priority_aggregator.h"

#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"

namespace performance_manager {

namespace {

const char kDescriberName[] = "ProcessPriorityAggregator";

}  // namespace

ProcessPriorityAggregator::ProcessPriorityAggregator() = default;

ProcessPriorityAggregator::~ProcessPriorityAggregator() = default;

void ProcessPriorityAggregator::OnPassedToGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
  graph->AddProcessNodeObserver(this);

  auto* registry =
      execution_context::ExecutionContextRegistry::GetFromGraph(graph);
  // We expect the registry to exist before we are passed to the graph.
  DCHECK(registry);
  registry->AddObserver(this);
}

void ProcessPriorityAggregator::OnTakenFromGraph(Graph* graph) {
  auto* registry =
      execution_context::ExecutionContextRegistry::GetFromGraph(graph);
  CHECK(registry);
  registry->RemoveObserver(this);

  graph->RemoveProcessNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
}

base::Value::Dict ProcessPriorityAggregator::DescribeProcessNodeData(
    const ProcessNode* node) const {
  Data& data = Data::Get(ProcessNodeImpl::FromNode(node));
  return data.Describe();
}

void ProcessPriorityAggregator::OnProcessNodeAdded(
    const ProcessNode* process_node) {
  Data::Create(ProcessNodeImpl::FromNode(process_node));
}

void ProcessPriorityAggregator::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
#if DCHECK_IS_ON()
  auto* process_node_impl = ProcessNodeImpl::FromNode(process_node);
  Data& data = Data::Get(process_node_impl);
  DCHECK(data.IsEmpty());
#endif
}

void ProcessPriorityAggregator::OnExecutionContextAdded(
    const execution_context::ExecutionContext* ec) {
  auto* process_node = ProcessNodeImpl::FromNode(ec->GetProcessNode());
  Data& data = Data::Get(process_node);
  data.Increment(ec->GetPriorityAndReason().priority());
  // This is a nop if the priority didn't actually change.
  process_node->set_priority(data.GetPriority());
}

void ProcessPriorityAggregator::OnBeforeExecutionContextRemoved(
    const execution_context::ExecutionContext* ec) {
  auto* process_node = ProcessNodeImpl::FromNode(ec->GetProcessNode());
  Data& data = Data::Get(process_node);
  data.Decrement(ec->GetPriorityAndReason().priority());
  // This is a nop if the priority didn't actually change.
  process_node->set_priority(data.GetPriority());
}

void ProcessPriorityAggregator::OnPriorityAndReasonChanged(
    const execution_context::ExecutionContext* ec,
    const PriorityAndReason& previous_value) {
  // If the priority itself didn't change then ignore this notification.
  const PriorityAndReason& new_value = ec->GetPriorityAndReason();
  if (new_value.priority() == previous_value.priority())
    return;

  // Update the distinct frame priority counts, and set the process priority
  // accordingly.
  auto* process_node = ProcessNodeImpl::FromNode(ec->GetProcessNode());
  Data& data = Data::Get(process_node);
  data.Decrement(previous_value.priority());
  data.Increment(new_value.priority());
  // This is a nop if the priority didn't actually change.
  process_node->set_priority(data.GetPriority());
}

}  // namespace performance_manager

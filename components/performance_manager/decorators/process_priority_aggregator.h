// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PROCESS_PRIORITY_AGGREGATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PROCESS_PRIORITY_AGGREGATOR_H_

#include "components/performance_manager/decorators/process_priority_aggregator_data.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {

// The ProcessPriorityAggregator is responsible for calculating a process
// priority as an aggregate of the priorities of all executions contexts (frames
// and workers) it hosts. A process will inherit the priority of the highest
// priority context that it hosts.
class ProcessPriorityAggregator
    : public GraphOwnedDefaultImpl,
      public NodeDataDescriberDefaultImpl,
      public ProcessNode::ObserverDefaultImpl,
      public execution_context::ExecutionContextObserverDefaultImpl {
 public:
  using Data = ProcessPriorityAggregatorData;

  ProcessPriorityAggregator();

  ProcessPriorityAggregator(const ProcessPriorityAggregator&) = delete;
  ProcessPriorityAggregator& operator=(const ProcessPriorityAggregator&) =
      delete;

  ~ProcessPriorityAggregator() override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // NodeDataDescriber implementation:
  base::Value::Dict DescribeProcessNodeData(
      const ProcessNode* node) const override;

  // ProcessNodeObserver implementation:
  void OnProcessNodeAdded(const ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;

  // ExecutionContextObserver implementation:
  void OnExecutionContextAdded(
      const execution_context::ExecutionContext* ec) override;
  void OnBeforeExecutionContextRemoved(
      const execution_context::ExecutionContext* ec) override;
  void OnPriorityAndReasonChanged(
      const execution_context::ExecutionContext* ec,
      const execution_context_priority::PriorityAndReason& previous_value)
      override;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PROCESS_PRIORITY_AGGREGATOR_H_

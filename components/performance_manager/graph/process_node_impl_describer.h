// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_DESCRIBER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_DESCRIBER_H_

#include "base/values.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer.h"

namespace performance_manager {

class ProcessNode;

// Describes the state of of a ProcessNodeImpl for human consumption.
class ProcessNodeImplDescriber : public GraphOwned,
                                 public NodeDataDescriberDefaultImpl {
 public:
  ProcessNodeImplDescriber() = default;
  ProcessNodeImplDescriber(const ProcessNodeImplDescriber&) = delete;
  ProcessNodeImplDescriber& operator=(const ProcessNodeImplDescriber&) = delete;
  ~ProcessNodeImplDescriber() override = default;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // NodeDataDescriber implementation:
  base::Value::Dict DescribeProcessNodeData(
      const ProcessNode* node) const override;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_DESCRIBER_H_

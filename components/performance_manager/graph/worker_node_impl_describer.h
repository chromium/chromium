// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_WORKER_NODE_IMPL_DESCRIBER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_WORKER_NODE_IMPL_DESCRIBER_H_

#include "base/values.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer.h"

namespace performance_manager {

class WorkerNodeImplDescriber : public GraphOwnedDefaultImpl,
                                public NodeDataDescriberDefaultImpl {
 public:
  WorkerNodeImplDescriber() = default;
  ~WorkerNodeImplDescriber() override = default;

  WorkerNodeImplDescriber(const WorkerNodeImplDescriber&) = delete;
  WorkerNodeImplDescriber& operator=(const WorkerNodeImplDescriber&) = delete;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // NodeDataDescriber implementation:
  base::Value DescribeWorkerNodeData(const WorkerNode* node) const override;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_WORKER_NODE_IMPL_DESCRIBER_H_

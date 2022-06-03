// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_DESCRIBER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_DESCRIBER_H_

#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer.h"

namespace performance_manager {

// Describes the state of of a PageNodeImpl for human consumption.
class PageNodeImplDescriber : public GraphOwned,
                              public NodeDataDescriberDefaultImpl {
 public:
  PageNodeImplDescriber();
  ~PageNodeImplDescriber() override;

  PageNodeImplDescriber(const PageNodeImplDescriber&) = delete;
  PageNodeImplDescriber& operator=(const PageNodeImplDescriber&) = delete;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // NodeDataDescriber implementation:
  base::Value DescribePageNodeData(const PageNode* page_node) const override;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_DESCRIBER_H_

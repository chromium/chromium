// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_DESCRIBER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_DESCRIBER_H_

#include "base/values.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer.h"

namespace performance_manager {

class FrameNode;

class FrameNodeImplDescriber : public GraphOwned,
                               public NodeDataDescriberDefaultImpl {
 public:
  FrameNodeImplDescriber() = default;
  FrameNodeImplDescriber(const FrameNodeImplDescriber&) = delete;
  FrameNodeImplDescriber& operator=(const FrameNodeImplDescriber&) = delete;
  ~FrameNodeImplDescriber() override;

  // GraphOwned impl:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // NodeDataDescriberDefaultImpl impl:
  base::Value::Dict DescribeFrameNodeData(const FrameNode* node) const override;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_DESCRIBER_H_

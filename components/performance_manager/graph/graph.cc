// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/graph.h"

#include "base/check_op.h"
#include "base/dcheck_is_on.h"

namespace performance_manager {

Graph::Graph() = default;
Graph::~Graph() = default;

GraphOwned::GraphOwned() = default;

GraphOwned::~GraphOwned() {
  // Must be removed from the graph before destruction.
  CHECK_EQ(graph_, nullptr);
}

void GraphOwned::PassToGraphImpl(Graph* graph) {
  CHECK_EQ(graph_, nullptr);
  graph_ = graph;
  OnPassedToGraph(graph);
}

void GraphOwned::TakeFromGraphImpl(Graph* graph) {
  CHECK_EQ(graph_, graph);
  OnTakenFromGraph(graph);
  graph_ = nullptr;
}

GraphOwnedDefaultImpl::GraphOwnedDefaultImpl() = default;
GraphOwnedDefaultImpl::~GraphOwnedDefaultImpl() = default;

}  // namespace performance_manager

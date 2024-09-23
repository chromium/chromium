// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/graph.h"

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/sequence_checker.h"

namespace performance_manager {

Graph::Graph() = default;
Graph::~Graph() = default;

GraphOwned::GraphOwned() {
  // It's valid to create a GraphOwned object on the main thread and pass it to
  // the PM sequence with PerformanceManager::PassToGraph().
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

GraphOwned::~GraphOwned() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Must be removed from the graph before destruction.
  CHECK_EQ(graph_, nullptr);
}

void GraphOwned::PassToGraphImpl(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(graph_, nullptr);
  graph_ = graph;
  OnPassedToGraph(graph);
}

Graph* GraphOwned::GetOwningGraph() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return graph_.get();
}

void GraphOwned::TakeFromGraphImpl(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(graph_, graph);
  OnTakenFromGraph(graph);
  graph_ = nullptr;
}

GraphOwnedDefaultImpl::GraphOwnedDefaultImpl() = default;
GraphOwnedDefaultImpl::~GraphOwnedDefaultImpl() = default;

}  // namespace performance_manager

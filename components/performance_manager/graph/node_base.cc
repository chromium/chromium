// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/node_base.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/public/graph/node.h"

namespace performance_manager {

// static
const uintptr_t NodeBase::kNodeBaseType =
    reinterpret_cast<uintptr_t>(&kNodeBaseType);

NodeBase::NodeBase() = default;

NodeBase::~NodeBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The node must have been removed from the graph before destruction.
  DCHECK(!graph_);
  DCHECK_EQ(NodeState::kNotInGraph, GetNodeState());
}

// static
const NodeBase* NodeBase::FromNode(const Node* node) {
  CHECK_EQ(kNodeBaseType, node->GetImplType());
  return reinterpret_cast<const NodeBase*>(node->GetImpl());
}

// static
NodeBase* NodeBase::FromNode(Node* node) {
  CHECK_EQ(kNodeBaseType, node->GetImplType());
  return reinterpret_cast<NodeBase*>(const_cast<void*>(node->GetImpl()));
}

bool NodeBase::CanSetProperty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (GetNodeState()) {
    case NodeState::kNotInGraph:
    case NodeState::kJoiningGraph:
    case NodeState::kLeavingGraph:
    case NodeState::kLeftGraph:
      return false;
    case NodeState::kInitializingNotInGraph:
    case NodeState::kInitializingEdges:
    case NodeState::kUninitializingEdges:
      return true;
    case NodeState::kActiveInGraph:
      // Can set properties, but must notify. See CanSetAndNotifyProperty().
      return false;
  }
  NOTREACHED();
}

bool NodeBase::CanSetAndNotifyProperty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (GetNodeState()) {
    case NodeState::kNotInGraph:
    case NodeState::kInitializingNotInGraph:
    case NodeState::kInitializingEdges:
    case NodeState::kJoiningGraph:
    case NodeState::kLeavingGraph:
    case NodeState::kUninitializingEdges:
    case NodeState::kLeftGraph:
      return false;
    case NodeState::kActiveInGraph:
      return true;
  }
  NOTREACHED();
}

void NodeBase::SetGraphPointer(GraphImpl* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!graph_);
  DCHECK(graph->NodeInGraph(this));
  DCHECK_EQ(NodeState::kNotInGraph, GetNodeState());
  graph_ = graph;
}

void NodeBase::OnInitializingProperties() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(NodeState::kInitializingNotInGraph, GetNodeState());
  // This is overridden by node impls.
}

void NodeBase::OnInitializingEdges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(NodeState::kInitializingEdges, GetNodeState());
  // This is overridden by node impls.
}

void NodeBase::OnAfterJoiningGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(NodeState::kActiveInGraph, GetNodeState());
  // This is overridden by node impls.
}

void NodeBase::OnBeforeLeavingGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(NodeState::kActiveInGraph, GetNodeState());
  // This is overridden by node impls.
}

void NodeBase::OnUninitializingEdges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(NodeState::kUninitializingEdges, GetNodeState());
  // This is overridden by node impls.
}

void NodeBase::ClearGraphPointer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(graph_);
  DCHECK(graph_->NodeInGraph(this));
  DCHECK_EQ(NodeState::kLeftGraph, GetNodeState());
  graph_ = nullptr;
}

}  // namespace performance_manager

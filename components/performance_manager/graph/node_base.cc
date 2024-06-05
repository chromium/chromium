// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/node_base.h"

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
  return GetNodeState() == NodeState::kInitializing;
}

bool NodeBase::CanSetAndNotifyProperty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetNodeState() == NodeState::kActiveInGraph;
}

void NodeBase::JoinGraph(GraphImpl* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!graph_);
  DCHECK(graph->NodeInGraph(this));
  DCHECK_EQ(NodeState::kNotInGraph, GetNodeState());
  graph_ = graph;
}

void NodeBase::OnJoiningGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(NodeState::kInitializing, GetNodeState());
  // This is overridden by node impls.
}

void NodeBase::OnBeforeLeavingGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(NodeState::kActiveInGraph, GetNodeState());
  // This is overridden by node impls.
}

void NodeBase::LeaveGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(graph_);
  DCHECK(graph_->NodeInGraph(this));
  DCHECK_EQ(NodeState::kLeavingGraph, GetNodeState());
  graph_ = nullptr;
}

}  // namespace performance_manager

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/node_base.h"

#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/public/graph/node.h"

namespace performance_manager {

// static
const uintptr_t NodeBase::kNodeBaseType =
    reinterpret_cast<uintptr_t>(&kNodeBaseType);

NodeBase::NodeBase(NodeTypeEnum node_type, GraphImpl* graph)
    : graph_(graph), type_(node_type) {}

NodeBase::~NodeBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The node must have been removed from the graph before destruction.
  DCHECK(!graph_->NodeInGraph(this));
}

// static
int64_t NodeBase::GetSerializationId(NodeBase* node) {
  if (!node)
    return 0u;

  if (!node->serialization_id_)
    node->serialization_id_ = node->graph()->GetNextNodeSerializationId();

  DCHECK_NE(0u, node->serialization_id_);
  return node->serialization_id_;
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

void NodeBase::JoinGraph() {
  DCHECK(graph_->NodeInGraph(this));
}

void NodeBase::LeaveGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(graph_->NodeInGraph(this));
}

}  // namespace performance_manager

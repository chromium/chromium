// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/node.h"

#include "components/performance_manager/graph/node_base.h"

namespace performance_manager {

Node::Node() = default;
Node::~Node() = default;

// static
int64_t Node::GetSerializationId(const Node* node) {
  if (!node)
    return 0;
  // This const_cast is unfortunate, but the process of assigning a
  // serialization ID to a node doesn't change it semantically.
  const NodeBase* node_base = NodeBase::FromNode(node);
  return NodeBase::GetSerializationId(const_cast<NodeBase*>(node_base));
}

}  // namespace performance_manager

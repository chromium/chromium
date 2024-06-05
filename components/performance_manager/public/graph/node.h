// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_H_

#include <cstdint>

#include "base/check_op.h"
#include "components/performance_manager/public/graph/node_state.h"
#include "components/performance_manager/public/graph/node_type.h"

namespace performance_manager {

class Graph;

// Interface that all nodes must implement.
class Node {
 public:
  Node();

  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  virtual ~Node();

  // Returns the type of this node.
  virtual NodeTypeEnum GetNodeType() const = 0;

  // Returns the graph to which this node belongs.
  virtual Graph* GetGraph() const = 0;

  // Returns the state of this node.
  virtual NodeState GetNodeState() const = 0;

  // The following functions are implementation detail and should not need to be
  // used by external clients. They provide the ability to safely downcast to
  // the underlying implementation.
  virtual uintptr_t GetImplType() const = 0;
  virtual const void* GetImpl() const = 0;
};

template <class PublicNodeClass>
class TypedNode : public Node {
 public:
  TypedNode() = default;
  ~TypedNode() override = default;

  NodeTypeEnum GetNodeType() const override { return PublicNodeClass::Type(); }

  // Helper function for casting from the generic Node type to its underlying
  // public node type. This CHECKs that the cast is valid.
  static const PublicNodeClass* FromNode(const Node* node) {
    CHECK_EQ(node->GetNodeType(), PublicNodeClass::Type());
    return reinterpret_cast<const PublicNodeClass*>(node);
  }
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_H_

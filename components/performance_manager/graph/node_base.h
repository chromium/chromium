// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_

#include <stdint.h>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/properties.h"
#include "components/performance_manager/public/graph/node_state.h"
#include "components/performance_manager/public/graph/node_type.h"

namespace performance_manager {

class Node;

// NodeBase implements shared functionality among different types of graph
// nodes. A specific type of graph node will derive from this class and can
// override shared functionality when needed.
// All node classes allow construction on one sequence and subsequent use from
// another sequence.
// All methods not documented otherwise are single-threaded.
class NodeBase {
 public:
  // Used as a unique key to safely allow downcasting from a public node type
  // to NodeBase via "GetImplType" and "GetImpl". The implementations are
  // provided by PublicNodeImpl below.
  static const uintptr_t kNodeBaseType;

  NodeBase();

  NodeBase(const NodeBase&) = delete;
  NodeBase& operator=(const NodeBase&) = delete;

  virtual ~NodeBase();

  // May be called on any sequence.
  NodeTypeEnum GetNodeType() const { return ToNode()->GetNodeType(); }

  // The state of this node.
  NodeState GetNodeState() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!graph_) {
      return NodeState::kNotInGraph;
    }
    return graph_->GetNodeState(this);
  }

  // Returns the graph that contains this node. Only valid after JoinGraph() and
  // before LeaveGraph().
  GraphImpl* graph() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(graph_);
    return graph_;
  }

  // Helper functions for casting from a node type to its underlying NodeBase.
  // This CHECKs that the cast is valid. These functions work happily with
  // public and private node class inputs.
  static const NodeBase* FromNode(const Node* node);
  static NodeBase* FromNode(Node* node);

  // For converting from NodeBase to Node. This is implemented by
  // TypedNodeBase.
  virtual const Node* ToNode() const = 0;

  // Satisfies part of the contract expected by ObservedProperty.
  // GetObservers is implemented by TypedNodeImpl.
  bool CanSetProperty() const;
  bool CanSetAndNotifyProperty() const;

 protected:
  friend class GraphImpl;

  // Helper function for TypedNodeBase to access the list of typed observers
  // stored in the graph.
  template <typename Observer>
  const GraphImpl::ObserverList<Observer>& GetObservers(
      const GraphImpl* graph) const {
    DCHECK(CanSetAndNotifyProperty());
    return graph->GetObservers<Observer>();
  }

  // Node lifecycle:

  // Step 0: A node is constructed. Node state is kNotInGraph. Outgoing edges
  // are set but not publicly visible.

  // Step 1:
  // Initializes the `graph_` pointer. Node must be in the kNotInGraph state,
  // and will transition to kInitializingProperties immediately after this call.
  void SetGraphPointer(GraphImpl* graph);

  // Step 2:
  // Called after `graph_` is set, a good opportunity to initialize node state.
  // The node will be in the kInitializingProperties state during this call.
  // Nodes may modify their properties that don't affect the graph topology but
  // *not* cause notifications to be emitted. After this the state transitions
  // to kInitializedNotInGraph.
  virtual void OnInitializingProperties();

  // Step 3:
  // OnBeforeNodeAdded notifications are dispatched. The node must not be
  // modified during any of these notifications. The node is in the
  // kInitializingNotInGraph state, and will transition to kInitializingEdges.

  // Step 4:
  // Called after properties are initialized, for nodes to update incoming edges
  // to fully join the graph. The node will be in the kInitializingEdges state
  // during this call, and will transition to kJoiningGraph immediately
  // afterward. Nodes may modify their properties that link to other nodes but
  // *not* cause notifications to be emitted.
  virtual void OnInitializingEdges();

  // Step 5:
  // OnNodeAdded notifications are dispatched. The node must not be modified
  // during any of these notifications. The node is in the kJoingGraph state.
  // TODO(crbug.com/40640034 ): Loosen this restriction to parallel
  // OnBeforeLeavingGraph()?

  // Step 6:
  // The node lives in the graph normally at this point, in the kActiveInGraph
  // state.

  // Step 7:
  // Called just before leaving |graph_|. The node will be in the kActiveInGraph
  // state during this call. The node may make property changes, and these
  // changes may cause notifications to be dispatched. This must leave the node
  // and the graph in a consistent state since the node is still in the graph.
  // For example if this is used to sever the "opener" relationship between a
  // PageNode and a FrameNode, it must send the OnOpenerFrameNodeChanged
  // notification, and leave the PageNode and FrameNode in the valid state they
  // would have when the page has no opener.
  virtual void OnBeforeLeavingGraph();

  // Step 8:
  // Node removed notifications are dispatched. The node must not be modified
  // during any of these notifications. The node is in the kLeavingGraph state.

  // Step 9:
  // Called while leaving |graph_|, a good opportunity to uninitialize node
  // state. The node will be in the kUninitializing state during this call.
  // Nodes may modify their properties but *not* cause notifications that have
  // this node as a parameter to be emitted, because from the public viewpoint
  // the node is already gone.
  virtual void OnUninitializing();

  // Step 10:
  // Called as this node is leaving |graph_|. Any private node-attached data
  // should be destroyed at this point. The node is in the kUninitializing
  // state.
  virtual void RemoveNodeAttachedData() = 0;

  // Step 11:
  // Leaves the graph that this node is a part of. The node is in the
  // kUninitializing state during this call, and will be in the kNotInGraph
  // state immediately afterwards.
  void LeaveGraph();

  // Assigned when JoinGraph() is called, up until LeaveGraph() is called, where
  // it is reset to null.
  raw_ptr<GraphImpl> graph_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Helper for implementing the Node parent of a PublicNodeClass.
template <class NodeImplClass, class PublicNodeClass>
class PublicNodeImpl : public PublicNodeClass {
 public:
  // Node implementation:
  Graph* GetGraph() const override {
    return static_cast<const NodeImplClass*>(this)->graph();
  }
  NodeState GetNodeState() const override {
    return static_cast<const NodeBase*>(static_cast<const NodeImplClass*>(this))
        ->GetNodeState();
  }
  uintptr_t GetImplType() const override { return NodeBase::kNodeBaseType; }
  const void* GetImpl() const override {
    // This exposes NodeBase, so that we can complete the triangle of casting
    // between all views of a node: NodeBase, FooNodeImpl, and FooNode.
    return static_cast<const NodeBase*>(
        static_cast<const NodeImplClass*>(this));
  }
};

template <class NodeImplClass, class NodeClass, class NodeObserverClass>
class TypedNodeBase : public NodeBase {
 public:
  using ObservedProperty =
      ObservedPropertyImpl<NodeImplClass, NodeClass, NodeObserverClass>;

  TypedNodeBase() = default;

  TypedNodeBase(const TypedNodeBase&) = delete;
  TypedNodeBase& operator=(const TypedNodeBase&) = delete;

  // Helper functions for casting from NodeBase to a concrete node type. This
  // CHECKs that the cast is valid.
  static const NodeImplClass* FromNodeBase(const NodeBase* node) {
    CHECK_EQ(NodeImplClass::Type(), node->GetNodeType());
    return static_cast<const NodeImplClass*>(node);
  }
  static NodeImplClass* FromNodeBase(NodeBase* node) {
    CHECK_EQ(NodeImplClass::Type(), node->GetNodeType());
    return static_cast<NodeImplClass*>(node);
  }

  // Helper functions for casting from a public node type to the private impl.
  // This also casts away const correctness, as it is intended to be used by
  // impl code that uses the public observer interface for a node type, where
  // all notifications are delivered with a const node pointer. This CHECKs that
  // the cast is valid.
  static NodeImplClass* FromNode(const Node* node) {
    NodeBase* node_base = const_cast<NodeBase*>(NodeBase::FromNode(node));
    return FromNodeBase(node_base);
  }

  // Convenience accessor to the per-node-class list of observers that is stored
  // in the graph. Satisfies the contract expected by ObservedProperty.
  const GraphImpl::ObserverList<NodeObserverClass>& GetObservers() const {
    // Mediate through NodeBase, as it's the class that is friended by the
    // GraphImpl in order to provide access.
    return NodeBase::GetObservers<NodeObserverClass>(graph());
  }

  const Node* ToNode() const override {
    return static_cast<const NodeImplClass*>(this);
  }
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_

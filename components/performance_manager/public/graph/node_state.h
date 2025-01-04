// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_STATE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_STATE_H_

namespace performance_manager {

// The state of a node. Nodes transition from the states linearly from top
// to bottom. For the full lifecycle of the node, see class comments on
// NodeBase.
//
// Each state has a set of flags:
//  * Edges visible: Public methods that return pointers to other nodes will
//    return pointers to nodes in the graph. If false, these methods will return
//    nullptr.
//  * Property changes allowed: Changes to properties that don't point to other
//    nodes are allowed, but they must not cause notifications to be sent unless
//    "Notifications allowed" is also true.
//  * Edge changes allowed: Changes to properties that point to other nodes are
//    allowed, but they must not cause notifications to be sent unless
//    "Notifications allowed" is also true.
//  * Notifications allowed: Public observer methods can be called with this
//    node as a parameter. Except for the OnBeforeNodeAdded and OnNodeRemoved
//    methods, all public observer methods expect the graph to be in a
//    consistent state, with all nodes in their parameters reachable and all
//    graph connections symmetrical.
enum class NodeState {
  // The node is in an uninitialized state, and does not belong to a graph. This
  // is the state of a newly constructed node, and one that has been removed
  // from the graph and uninitialized but not yet destroyed.
  kNotInGraph,

  // The node is initializing but not yet in the graph. Nodes will be in this
  // state during public OnBeforeNodeAdded observer methods.
  //
  // Property changes allowed.
  kInitializingNotInGraph,

  // The node is initializing edges. Nodes will not be in this state during any
  // public observer methods, because the public API must always see graph edges
  // in a consistent state.
  //
  // Property changes allowed.
  // Edge changes allowed.
  kInitializingEdges,

  // The node is being added to the graph. Incoming and outgoing edges have been
  // initialized. Nodes will be in this state during public OnNodeAdded observer
  // methods.
  //
  // Edges visible.
  kJoiningGraph,

  // The node is active in the graph. Nodes will be in this state during most
  // public observer methods.
  //
  // Edges visible.
  // Property changes allowed.
  // Edge changes allowed.
  // Notifications allowed.
  kActiveInGraph,

  // The node is being removed from the graph. Incoming and outgoing edges are
  // still set. Nodes will be in this state during public OnBeforeNodeRemoved
  // observer methods.
  //
  // Edges visible.
  kLeavingGraph,

  // The node is uninitializing edges. Nodes will not be in this state during
  // any public observer methods, because the public API must always see graph
  // edges in a consistent state.
  //
  // Property changes allowed.
  // Edge changes allowed.
  kUninitializingEdges,

  // The node is no longer in the graph. Its properties are still valid except
  // for those that point to other graph nodes, which are all null. Nodes will
  // be in this state during public OnNodeRemoved observer methods.
  //
  // Unlike kInitializingNotInGraph, property changes are not allowed, because
  // the node is about to be deleted so they would have no effect.
  kLeftGraph,
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_STATE_H_

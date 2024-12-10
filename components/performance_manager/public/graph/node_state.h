// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_STATE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_STATE_H_

namespace performance_manager {

// The state of a node. Nodes transition from the states linearly from top
// to bottom. For the full lifecycle of the node, see class comments on
// NodeBase.
enum class NodeState {
  // The node is in an uninitialized state, and does not belong to a graph. This
  // is the state of a newly constructed node, and one that has just been
  // removed from a graph but not yet destroyed.
  kNotInGraph,

  // The node is initializing. Making property changes is fine, but no
  // notifications should be dispatched and no edges to other nodes should be
  // added. This state is only seen by node implementations and will never be
  // visible via the public API.
  kInitializingProperties,

  // The node is not in the graph. Its properties are initialized except for
  // those that point to other graph nodes. No property changes or notifications
  // are permitted.
  kInitializedNotInGraph,

  // The node is initializing edges. Making property changes that point to other
  // graph nodes is fine, but no notifications should be dispatched. This state
  // is only seen by node implementations and will never be visible via the
  // public API.
  kInitializingEdges,

  // The node is being added to the graph. Incoming and outgoing edges have been
  // initialized. No property changes or notifications are permitted.
  kJoiningGraph,

  // The node is active in the graph.
  kActiveInGraph,

  // The node is being removed from the graph. No property changes or
  // notifications are permitted.
  kLeavingGraph,

  // The node is uninitializing. Making property changes is fine, but no
  // notifications should be dispatched. This state is only seen by node
  // implementations and will never be visible via the public API.
  kUninitializing,
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_STATE_H_

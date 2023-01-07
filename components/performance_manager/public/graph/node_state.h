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
  // notifications should be dispatched. This state is only seen by node
  // implementations and will never be visible via the public API.
  kInitializing,

  // The node is being added to the graph. No property changes or notifications
  // are permitted.
  kJoiningGraph,

  // The node is active in the graph.
  kActiveInGraph,

  // The node is being removed from the graph. No property changes or
  // notifications are permitted.
  kLeavingGraph
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_STATE_H_

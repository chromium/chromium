// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_

#include "base/observer_list_types.h"
#include "components/performance_manager/public/graph/node.h"

namespace performance_manager {

class SystemNodeObserver;

// The SystemNode represents system-wide state. Each graph owns exactly one
// system node. This node has the same lifetime has the graph that owns it.
class SystemNode : public TypedNode<SystemNode> {
 public:
  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kSystem; }

  SystemNode();

  SystemNode(const SystemNode&) = delete;
  SystemNode& operator=(const SystemNode&) = delete;

  ~SystemNode() override;
};

// Observer interface for the system node.
class SystemNodeObserver : public base::CheckedObserver {
 public:
  SystemNodeObserver();

  SystemNodeObserver(const SystemNodeObserver&) = delete;
  SystemNodeObserver& operator=(const SystemNodeObserver&) = delete;

  ~SystemNodeObserver() override;

  // Called when a new set of process memory metrics is available.
  //
  // Note: This is only valid if at least one component has expressed interest
  // for process memory metrics by calling
  // ProcessMetricsDecorator::RegisterInterestForProcessMetrics.
  virtual void OnProcessMemoryMetricsAvailable(const SystemNode* system_node) {}
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_

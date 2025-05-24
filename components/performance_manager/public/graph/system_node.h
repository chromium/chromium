// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_

#include "base/memory/memory_pressure_listener.h"
#include "base/observer_list_types.h"
#include "components/performance_manager/public/graph/node.h"

namespace performance_manager {

class SystemNodeObserver;

// The SystemNode represents system-wide state. Each graph owns exactly one
// system node. This node has the same lifetime has the graph that owns it.
class SystemNode : public TypedNode<SystemNode> {
 public:
  using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;

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

  // Called before OnMemoryPressure(). This can be used to track state before
  // memory start being released in response to memory pressure.
  //
  // Note: This is guaranteed to be invoked before OnMemoryPressure(), but
  // will not necessarily be called before base::MemoryPressureListeners
  // are notified.
  virtual void OnBeforeMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel new_level) {}

  // Called when the system is under memory pressure. Observers may start
  // releasing memory in response to memory pressure.
  //
  // NOTE: This isn't called for a transition to the MEMORY_PRESSURE_LEVEL_NONE
  // level. For this reason there's no corresponding property in this node and
  // the response to these notifications should be stateless.
  virtual void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel new_level) {}
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_

#include "base/macros.h"
#include "components/performance_manager/public/graph/node.h"

namespace performance_manager {

class SystemNodeObserver;

// The SystemNode represents system-wide state. There is at most one system node
// in a graph.
class SystemNode : public Node {
 public:
  using Observer = SystemNodeObserver;
  class ObserverDefaultImpl;

  SystemNode();
  ~SystemNode() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemNode);
};

// Pure virtual observer interface. Derive from this if you want to be forced to
// implement the entire interface.
class SystemNodeObserver {
 public:
  SystemNodeObserver();
  virtual ~SystemNodeObserver();

  // Node lifetime notifications.

  // Called when the |system_node| is added to the graph.
  virtual void OnSystemNodeAdded(const SystemNode* system_node) = 0;

  // Called before the |system_node| is removed from the graph.
  virtual void OnBeforeSystemNodeRemoved(const SystemNode* system_node) = 0;

  // Called when a new set of process memory metrics is available.
  virtual void OnProcessMemoryMetricsAvailable(
      const SystemNode* system_node) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemNodeObserver);
};

// Default implementation of observer that provides dummy versions of each
// function. Derive from this if you only need to implement a few of the
// functions.
class SystemNode::ObserverDefaultImpl : public SystemNodeObserver {
 public:
  ObserverDefaultImpl();
  ~ObserverDefaultImpl() override;

  // SystemNodeObserver implementation:
  void OnSystemNodeAdded(const SystemNode* system_node) override {}
  void OnBeforeSystemNodeRemoved(const SystemNode* system_node) override {}
  void OnProcessMemoryMetricsAvailable(const SystemNode* system_node) override {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ObserverDefaultImpl);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_

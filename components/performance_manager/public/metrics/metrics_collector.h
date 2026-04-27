// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_METRICS_COLLECTOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_METRICS_COLLECTOR_H_

#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {

// The MetricsCollector is a graph observer that reports UMA.
class MetricsCollector : public GraphOwned,
                         public ProcessNodeObserver {
 public:
  MetricsCollector();

  MetricsCollector(const MetricsCollector&) = delete;
  MetricsCollector& operator=(const MetricsCollector&) = delete;

  ~MetricsCollector() override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // ProcessNodeObserver implementation:
  void OnProcessLifetimeChange(const ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;


 private:

  void OnProcessDestroyed(const ProcessNode* process_node);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_METRICS_COLLECTOR_H_

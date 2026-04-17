// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_METRICS_COLLECTOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_METRICS_COLLECTOR_H_

#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace performance_manager {

// The MetricsCollector is a graph observer that reports UMA/UKM.
class MetricsCollector : public GraphOwned,
                         public PageNodeObserver,
                         public ProcessNodeObserver {
 public:
  MetricsCollector();

  MetricsCollector(const MetricsCollector&) = delete;
  MetricsCollector& operator=(const MetricsCollector&) = delete;

  ~MetricsCollector() override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver implementation:
  void OnUkmSourceIdChanged(const PageNode* page_node) override;

  // ProcessNodeObserver implementation:
  void OnProcessLifetimeChange(const ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;

 protected:
  friend class UkmCollectionStateHolder;

  struct UkmCollectionState {
    ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
  };

 private:
  static UkmCollectionState* GetUkmCollectionState(const PageNode* page_node);

  // (Un)registers the various node observer flavors of this object with the
  // graph. These are invoked by OnPassedToGraph and OnTakenFromGraph, but
  // hoisted to their own functions for testing.
  void RegisterObservers(Graph* graph);
  void UnregisterObservers(Graph* graph);

  void UpdateUkmSourceIdForPage(const PageNode* page_node,
                                ukm::SourceId ukm_source_id);

  void OnProcessDestroyed(const ProcessNode* process_node);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_METRICS_COLLECTOR_H_

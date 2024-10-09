// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_LOADING_SCENARIO_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_LOADING_SCENARIO_OBSERVER_H_

#include "base/containers/span.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/scenarios/loading_scenario_data.h"

namespace performance_manager {

class LoadingScenarioObserver : public FrameNode::ObserverDefaultImpl,
                                public PageNode::ObserverDefaultImpl,
                                public ProcessNode::ObserverDefaultImpl,
                                public GraphOwned {
 public:
  LoadingScenarioObserver();
  ~LoadingScenarioObserver() override;

  LoadingScenarioObserver(const LoadingScenarioObserver&) = delete;
  LoadingScenarioObserver& operator=(const LoadingScenarioObserver&) = delete;

  // FrameNodeObserver:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;

  // PageNodeObserver:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnIsFocusedChanged(const PageNode* page_node) override;
  void OnIsVisibleChanged(const PageNode* page_node) override;
  void OnLoadingStateChanged(const PageNode* page_node,
                             PageNode::LoadingState previous_state) override;

  // ProcessNodeObserver:
  void OnProcessNodeAdded(const ProcessNode* process_node) override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

 private:
  // Increments or decrements the loading counts for a page that `is_visible`
  // and `is_focused`. Modifies the counts for the global loading scenario and
  // all process scenarios for `process_nodes`.
  void IncrementLoadingCounts(base::span<const ProcessNode*> process_nodes,
                              bool is_visible,
                              bool is_focused);
  void DecrementLoadingCounts(base::span<const ProcessNode*> process_nodes,
                              bool is_visible,
                              bool is_focused);

  // Updates the global loading scenario, and all process scenarios for
  // `process_nodes`, based on the current loading counts.
  void UpdateLoadingScenarios(base::span<const ProcessNode*> process_nodes);

  SEQUENCE_CHECKER(sequence_checker_);

  LoadingScenarioCounts global_counts_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_LOADING_SCENARIO_OBSERVER_H_

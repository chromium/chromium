// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_LOADING_SCENARIO_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_LOADING_SCENARIO_OBSERVER_H_

#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/scenarios/performance_scenarios.h"

namespace performance_manager {

class LoadingScenarioObserver : public PageNode::ObserverDefaultImpl,
                                public GraphOwned {
 public:
  // Counts of pages in each loading state.
  class LoadingCounts {
   public:
    LoadingCounts() = default;
    ~LoadingCounts() = default;

    constexpr friend bool operator==(const LoadingCounts&,
                                     const LoadingCounts&) = default;

    // Focused pages that are loading.
    size_t focused_loading_pages() const { return focused_loading_pages_; }

    // Visible pages (including focused) that are loading.
    size_t visible_loading_pages() const { return visible_loading_pages_; }

    // All pages (including focused and visible) that are loading.
    size_t loading_pages() const { return loading_pages_; }

    void IncrementLoadingPageCounts(bool visible, bool focused);
    void DecrementLoadingPageCounts(bool visible, bool focused);

    LoadingScenario GetScenario() const;

   private:
    size_t focused_loading_pages_ = 0;
    size_t visible_loading_pages_ = 0;
    size_t loading_pages_ = 0;
  };

  LoadingScenarioObserver() = default;
  ~LoadingScenarioObserver() override = default;

  LoadingScenarioObserver(const LoadingScenarioObserver&) = delete;
  LoadingScenarioObserver& operator=(const LoadingScenarioObserver&) = delete;

  // PageNodeObserver:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnIsFocusedChanged(const PageNode* page_node) override;
  void OnIsVisibleChanged(const PageNode* page_node) override;
  void OnLoadingStateChanged(const PageNode* page_node,
                             PageNode::LoadingState previous_state) override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

 private:
  void UpdateGlobalScenario();

  SEQUENCE_CHECKER(sequence_checker_);
  LoadingCounts global_counts_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_LOADING_SCENARIO_OBSERVER_H_

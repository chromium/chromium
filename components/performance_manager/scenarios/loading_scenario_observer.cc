// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/scenarios/loading_scenario_observer.h"

#include <atomic>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/scenarios/performance_scenarios.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"

namespace performance_manager {

namespace {

bool StateIsLoading(PageNode::LoadingState loading_state) {
  switch (loading_state) {
    case PageNode::LoadingState::kLoadingNotStarted:
    case PageNode::LoadingState::kLoadingTimedOut:
    case PageNode::LoadingState::kLoadedIdle:
      return false;
    case PageNode::LoadingState::kLoading:
    case PageNode::LoadingState::kLoadedBusy:
      return true;
  }
  NOTREACHED();
}

}  // namespace

LoadingScenario LoadingScenarioObserver::LoadingCounts::GetScenario() const {
  if (focused_loading_pages_ > 0) {
    return LoadingScenario::kFocusedPageLoading;
  }
  if (visible_loading_pages_ > 0) {
    return LoadingScenario::kVisiblePageLoading;
  }
  if (loading_pages_ > 0) {
    return LoadingScenario::kBackgroundPageLoading;
  }
  return LoadingScenario::kNoPageLoading;
}

void LoadingScenarioObserver::LoadingCounts::IncrementLoadingPageCounts(
    bool visible,
    bool focused) {
  loading_pages_++;
  if (visible) {
    visible_loading_pages_++;
  }
  if (focused) {
    focused_loading_pages_++;
  }
}

void LoadingScenarioObserver::LoadingCounts::DecrementLoadingPageCounts(
    bool visible,
    bool focused) {
  CHECK_GT(loading_pages_.RawValue(), 0u);
  loading_pages_--;
  if (visible) {
    CHECK_GT(visible_loading_pages_.RawValue(), 0u);
    visible_loading_pages_--;
  }
  if (focused) {
    CHECK_GT(focused_loading_pages_.RawValue(), 0u);
    focused_loading_pages_--;
  }
}

void LoadingScenarioObserver::OnPageNodeAdded(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (StateIsLoading(page_node->GetLoadingState())) {
    global_counts_.IncrementLoadingPageCounts(page_node->IsVisible(),
                                              page_node->IsFocused());
  }
  UpdateGlobalScenario();
}

void LoadingScenarioObserver::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (StateIsLoading(page_node->GetLoadingState())) {
    global_counts_.DecrementLoadingPageCounts(page_node->IsVisible(),
                                              page_node->IsFocused());
    UpdateGlobalScenario();
  }
}

void LoadingScenarioObserver::OnIsFocusedChanged(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (StateIsLoading(page_node->GetLoadingState())) {
    global_counts_.DecrementLoadingPageCounts(page_node->IsVisible(),
                                              !page_node->IsFocused());
    global_counts_.IncrementLoadingPageCounts(page_node->IsVisible(),
                                              page_node->IsFocused());
    UpdateGlobalScenario();
  }
}

void LoadingScenarioObserver::OnIsVisibleChanged(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (StateIsLoading(page_node->GetLoadingState())) {
    global_counts_.DecrementLoadingPageCounts(!page_node->IsVisible(),
                                              page_node->IsFocused());
    global_counts_.IncrementLoadingPageCounts(page_node->IsVisible(),
                                              page_node->IsFocused());
    UpdateGlobalScenario();
  }
}

void LoadingScenarioObserver::OnLoadingStateChanged(
    const PageNode* page_node,
    PageNode::LoadingState previous_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool is_loading = StateIsLoading(page_node->GetLoadingState());
  const bool was_loading = StateIsLoading(previous_state);
  if (is_loading != was_loading) {
    if (is_loading) {
      global_counts_.IncrementLoadingPageCounts(page_node->IsVisible(),
                                                page_node->IsFocused());
    } else {
      global_counts_.DecrementLoadingPageCounts(page_node->IsVisible(),
                                                page_node->IsFocused());
    }
    UpdateGlobalScenario();
  }
}

void LoadingScenarioObserver::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->AddPageNodeObserver(this);
  CHECK(global_counts_ == LoadingCounts{});
  CHECK_EQ(blink::performance_scenarios::GetLoadingScenario(
               blink::performance_scenarios::Scope::kGlobal)
               ->load(std::memory_order_relaxed),
           LoadingScenario::kNoPageLoading);
  for (const PageNode* page_node : graph->GetAllPageNodes()) {
    if (StateIsLoading(page_node->GetLoadingState())) {
      global_counts_.IncrementLoadingPageCounts(page_node->IsVisible(),
                                                page_node->IsFocused());
    }
  }
  UpdateGlobalScenario();
}

void LoadingScenarioObserver::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemovePageNodeObserver(this);
  global_counts_ = LoadingCounts{};
  UpdateGlobalScenario();
}

void LoadingScenarioObserver::UpdateGlobalScenario() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetGlobalLoadingScenario(global_counts_.GetScenario());
}

}  // namespace performance_manager

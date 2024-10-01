// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/loading_scenario_observer.h"

#include <atomic>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
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

// Increments `num` in-place, and CHECK's on overflow.
void CheckIncrement(size_t& num) {
  num = base::CheckAdd(num, 1).ValueOrDie();
}

// Decrements `num` in-place, and CHECK's on underflow.
void CheckDecrement(size_t& num) {
  num = base::CheckSub(num, 1).ValueOrDie();
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
  CheckIncrement(loading_pages_);
  if (visible) {
    CheckIncrement(visible_loading_pages_);
  }
  if (focused) {
    CheckIncrement(focused_loading_pages_);
  }
}

void LoadingScenarioObserver::LoadingCounts::DecrementLoadingPageCounts(
    bool visible,
    bool focused) {
  CheckDecrement(loading_pages_);
  if (visible) {
    CheckDecrement(visible_loading_pages_);
  }
  if (focused) {
    CheckDecrement(focused_loading_pages_);
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

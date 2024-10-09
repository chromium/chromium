// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/loading_scenario_observer.h"

#include <atomic>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/scenarios/performance_scenarios.h"
#include "components/performance_manager/scenarios/loading_scenario_data.h"
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

LoadingScenario CalculateLoadingScenario(const LoadingScenarioCounts& counts) {
  if (counts.focused_loading_pages() > 0) {
    return LoadingScenario::kFocusedPageLoading;
  }
  if (counts.visible_loading_pages() > 0) {
    return LoadingScenario::kVisiblePageLoading;
  }
  if (counts.loading_pages() > 0) {
    return LoadingScenario::kBackgroundPageLoading;
  }
  return LoadingScenario::kNoPageLoading;
}

}  // namespace

LoadingScenarioObserver::LoadingScenarioObserver() = default;

LoadingScenarioObserver::~LoadingScenarioObserver() = default;

void LoadingScenarioObserver::OnFrameNodeAdded(const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const ProcessNode* process_node = frame_node->GetProcessNode();
  const PageNode* page_node = frame_node->GetPageNode();
  const size_t new_frame_count =
      LoadingScenarioPageFrameCounts::Get(PageNodeImpl::FromNode(page_node))
          .IncrementFrameCountForProcess(process_node);
  if (new_frame_count == 1 && StateIsLoading(page_node->GetLoadingState())) {
    // Process joined a loading page. Need to update process state.
    auto& loading_counts =
        LoadingScenarioCounts::Get(ProcessNodeImpl::FromNode(process_node));
    loading_counts.IncrementLoadingPageCounts(page_node->IsVisible(),
                                              page_node->IsFocused());
    SetLoadingScenarioForProcessNode(CalculateLoadingScenario(loading_counts),
                                     process_node);
  }
}

void LoadingScenarioObserver::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const ProcessNode* process_node = frame_node->GetProcessNode();
  const PageNode* page_node = frame_node->GetPageNode();
  const size_t new_frame_count =
      LoadingScenarioPageFrameCounts::Get(PageNodeImpl::FromNode(page_node))
          .DecrementFrameCountForProcess(process_node);
  if (new_frame_count == 0 && StateIsLoading(page_node->GetLoadingState())) {
    // Process no longer part of a loading page. Need to update process state.
    auto& loading_counts =
        LoadingScenarioCounts::Get(ProcessNodeImpl::FromNode(process_node));
    loading_counts.DecrementLoadingPageCounts(page_node->IsVisible(),
                                              page_node->IsFocused());
    SetLoadingScenarioForProcessNode(CalculateLoadingScenario(loading_counts),
                                     process_node);
  }
}

void LoadingScenarioObserver::OnPageNodeAdded(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(page_node->GetMainFrameNodes().empty());
  LoadingScenarioPageFrameCounts::Create(PageNodeImpl::FromNode(page_node));
  if (StateIsLoading(page_node->GetLoadingState())) {
    auto process_nodes =
        LoadingScenarioPageFrameCounts::Get(PageNodeImpl::FromNode(page_node))
            .GetProcessesWithFramesInPage();
    IncrementLoadingCounts(process_nodes, page_node->IsVisible(),
                           page_node->IsFocused());
    UpdateLoadingScenarios(process_nodes);
  }
}

void LoadingScenarioObserver::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (StateIsLoading(page_node->GetLoadingState())) {
    auto process_nodes =
        LoadingScenarioPageFrameCounts::Get(PageNodeImpl::FromNode(page_node))
            .GetProcessesWithFramesInPage();
    DecrementLoadingCounts(process_nodes, page_node->IsVisible(),
                           page_node->IsFocused());
    UpdateLoadingScenarios(process_nodes);
  }
}

void LoadingScenarioObserver::OnIsFocusedChanged(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (StateIsLoading(page_node->GetLoadingState())) {
    auto process_nodes =
        LoadingScenarioPageFrameCounts::Get(PageNodeImpl::FromNode(page_node))
            .GetProcessesWithFramesInPage();
    DecrementLoadingCounts(process_nodes, page_node->IsVisible(),
                           !page_node->IsFocused());
    IncrementLoadingCounts(process_nodes, page_node->IsVisible(),
                           page_node->IsFocused());
    UpdateLoadingScenarios(process_nodes);
  }
}

void LoadingScenarioObserver::OnIsVisibleChanged(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (StateIsLoading(page_node->GetLoadingState())) {
    auto process_nodes =
        LoadingScenarioPageFrameCounts::Get(PageNodeImpl::FromNode(page_node))
            .GetProcessesWithFramesInPage();
    DecrementLoadingCounts(process_nodes, !page_node->IsVisible(),
                           page_node->IsFocused());
    IncrementLoadingCounts(process_nodes, page_node->IsVisible(),
                           page_node->IsFocused());
    UpdateLoadingScenarios(process_nodes);
  }
}

void LoadingScenarioObserver::OnLoadingStateChanged(
    const PageNode* page_node,
    PageNode::LoadingState previous_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool is_loading = StateIsLoading(page_node->GetLoadingState());
  const bool was_loading = StateIsLoading(previous_state);
  if (is_loading != was_loading) {
    auto process_nodes =
        LoadingScenarioPageFrameCounts::Get(PageNodeImpl::FromNode(page_node))
            .GetProcessesWithFramesInPage();
    if (is_loading) {
      IncrementLoadingCounts(process_nodes, page_node->IsVisible(),
                             page_node->IsFocused());
    } else {
      DecrementLoadingCounts(process_nodes, page_node->IsVisible(),
                             page_node->IsFocused());
    }
    UpdateLoadingScenarios(process_nodes);
  }
}

void LoadingScenarioObserver::OnProcessNodeAdded(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LoadingScenarioCounts::Create(ProcessNodeImpl::FromNode(process_node));
}

void LoadingScenarioObserver::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Must be created before any nodes are added. This is a simplification to
  // avoid extra code to calculating the current scenario here.
  CHECK(graph->GetAllPageNodes().empty());
  CHECK(graph->GetAllProcessNodes().empty());
  CHECK(graph->GetAllFrameNodes().empty());
  CHECK_EQ(blink::performance_scenarios::GetLoadingScenario(
               blink::performance_scenarios::Scope::kGlobal)
               ->load(std::memory_order_relaxed),
           LoadingScenario::kNoPageLoading);
  graph->AddFrameNodeObserver(this);
  graph->AddPageNodeObserver(this);
  graph->AddProcessNodeObserver(this);
}

void LoadingScenarioObserver::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  graph->RemoveProcessNodeObserver(this);
  SetGlobalLoadingScenario(LoadingScenario::kNoPageLoading);
  for (const ProcessNode* process_node : graph->GetAllProcessNodes()) {
    SetLoadingScenarioForProcessNode(LoadingScenario::kNoPageLoading,
                                     process_node);
  }
}

void LoadingScenarioObserver::IncrementLoadingCounts(
    base::span<const ProcessNode*> process_nodes,
    bool is_visible,
    bool is_focused) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  global_counts_.IncrementLoadingPageCounts(is_visible, is_focused);
  for (const ProcessNode* process_node : process_nodes) {
    LoadingScenarioCounts::Get(ProcessNodeImpl::FromNode(process_node))
        .IncrementLoadingPageCounts(is_visible, is_focused);
  }
}

void LoadingScenarioObserver::DecrementLoadingCounts(
    base::span<const ProcessNode*> process_nodes,
    bool is_visible,
    bool is_focused) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  global_counts_.DecrementLoadingPageCounts(is_visible, is_focused);
  for (const ProcessNode* process_node : process_nodes) {
    LoadingScenarioCounts::Get(ProcessNodeImpl::FromNode(process_node))
        .DecrementLoadingPageCounts(is_visible, is_focused);
  }
}

void LoadingScenarioObserver::UpdateLoadingScenarios(
    base::span<const ProcessNode*> process_nodes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetGlobalLoadingScenario(CalculateLoadingScenario(global_counts_));
  for (const ProcessNode* process_node : process_nodes) {
    SetLoadingScenarioForProcessNode(
        CalculateLoadingScenario(LoadingScenarioCounts::Get(
            ProcessNodeImpl::FromNode(process_node))),
        process_node);
  }
}

}  // namespace performance_manager

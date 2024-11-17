// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/loading_page_voter.h"

#include <utility>

#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager::execution_context_priority {

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  return execution_context::ExecutionContextRegistry::GetFromGraph(
             frame_node->GetGraph())
      ->GetExecutionContextForFrameNode(frame_node);
}

// Returns true if `loading_state` represent an actively loading state.
bool IsLoading(PageNode::LoadingState loading_state) {
  return loading_state == PageNode::LoadingState::kLoading ||
         loading_state == PageNode::LoadingState::kLoadedBusy;
}

}  // namespace

// static
const char LoadingPageVoter::kPageIsLoadingReason[] = "Page is loading.";

LoadingPageVoter::LoadingPageVoter(VotingChannel voting_channel)
    : voting_channel_(std::move(voting_channel)) {}

LoadingPageVoter::~LoadingPageVoter() = default;

void LoadingPageVoter::InitializeOnGraph(Graph* graph) {
  graph->AddPageNodeObserver(this);
  graph->AddInitializingFrameNodeObserver(this);
}

void LoadingPageVoter::TearDownOnGraph(Graph* graph) {
  graph->RemovePageNodeObserver(this);
  graph->RemoveInitializingFrameNodeObserver(this);
}

void LoadingPageVoter::OnPageNodeAdded(const PageNode* page_node) {
  if (!IsLoading(page_node->GetLoadingState())) {
    return;
  }

  OnPageNodeStartedLoading(page_node);
}

void LoadingPageVoter::OnBeforePageNodeRemoved(const PageNode* page_node) {
  if (!IsLoading(page_node->GetLoadingState())) {
    return;
  }

  OnPageNodeStoppedLoading(page_node);
}

void LoadingPageVoter::OnLoadingStateChanged(
    const PageNode* page_node,
    PageNode::LoadingState previous_state) {
  const bool was_loading = IsLoading(previous_state);
  const bool is_loading = IsLoading(page_node->GetLoadingState());

  if (was_loading && !is_loading) {
    OnPageNodeStoppedLoading(page_node);
  }
  if (!was_loading && is_loading) {
    OnPageNodeStartedLoading(page_node);
  }
}

void LoadingPageVoter::OnFrameNodeInitializing(const FrameNode* frame_node) {
  if (!IsLoading(frame_node->GetPageNode()->GetLoadingState())) {
    return;
  }

  voting_channel_.SubmitVote(
      GetExecutionContext(frame_node),
      Vote(base::TaskPriority::USER_VISIBLE, kPageIsLoadingReason));
}

void LoadingPageVoter::OnFrameNodeTearingDown(const FrameNode* frame_node) {
  if (!IsLoading(frame_node->GetPageNode()->GetLoadingState())) {
    return;
  }

  voting_channel_.InvalidateVote(GetExecutionContext(frame_node));
}

void LoadingPageVoter::OnPageNodeStartedLoading(const PageNode* page_node) {
  for (const FrameNode* main_frame_node : page_node->GetMainFrameNodes()) {
    SubmitVoteForSubtree(main_frame_node);
  }
}

void LoadingPageVoter::OnPageNodeStoppedLoading(const PageNode* page_node) {
  for (const FrameNode* main_frame_node : page_node->GetMainFrameNodes()) {
    InvalidateVoteForSubtree(main_frame_node);
  }
}

void LoadingPageVoter::SubmitVoteForSubtree(const FrameNode* frame_node) {
  voting_channel_.SubmitVote(
      GetExecutionContext(frame_node),
      Vote(base::TaskPriority::USER_VISIBLE, kPageIsLoadingReason));

  // Recurse through subtree.
  for (const FrameNode* child_frame_node : frame_node->GetChildFrameNodes()) {
    SubmitVoteForSubtree(child_frame_node);
  }
}

void LoadingPageVoter::InvalidateVoteForSubtree(const FrameNode* frame_node) {
  voting_channel_.InvalidateVote(GetExecutionContext(frame_node));

  // Recurse through subtree.
  for (const FrameNode* child_frame_node : frame_node->GetChildFrameNodes()) {
    InvalidateVoteForSubtree(child_frame_node);
  }
}

}  // namespace performance_manager::execution_context_priority

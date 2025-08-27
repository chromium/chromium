// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/closing_page_voter.h"

#include <utility>

#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager::execution_context_priority {

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const PageNode* page_node) {
  const FrameNode* main_frame_node = page_node->GetMainFrameNode();
  if (!main_frame_node) {
    return nullptr;
  }
  return execution_context::ExecutionContextRegistry::GetFromGraph(
             page_node->GetGraph())
      ->GetExecutionContextForFrameNode(main_frame_node);
}

}  // namespace

// static
const char ClosingPageVoter::kPageIsClosingReason[] = "Page is closing.";

ClosingPageVoter::ClosingPageVoter() = default;

ClosingPageVoter::~ClosingPageVoter() = default;

void ClosingPageVoter::InitializeOnGraph(Graph* graph,
                                         VotingChannel voting_channel) {
  voting_channel_ = std::move(voting_channel);
  graph->AddPageNodeObserver(this);
}

void ClosingPageVoter::TearDownOnGraph(Graph* graph) {
  // Invalidate all remaining votes before resetting the channel.
  for (const auto& vote : active_votes_) {
    voting_channel_.InvalidateVote(vote.second);
  }
  active_votes_.clear();

  graph->RemovePageNodeObserver(this);
  voting_channel_.Reset();
}

void ClosingPageVoter::OnBeforePageNodeAdded(const PageNode* page_node) {
  CHECK(!page_node->IsClosing(), base::NotFatalUntil::M145);
}

void ClosingPageVoter::OnBeforePageNodeRemoved(const PageNode* page_node) {
  auto it = active_votes_.find(page_node);
  if (it != active_votes_.end()) {
    voting_channel_.InvalidateVote(it->second);
    active_votes_.erase(it);
  }
}

void ClosingPageVoter::OnIsClosingChanged(const PageNode* page_node) {
  if (page_node->IsClosing()) {
    if (const auto* ec = GetExecutionContext(page_node)) {
      voting_channel_.SubmitVote(
          ec, Vote(base::TaskPriority::USER_BLOCKING, kPageIsClosingReason));
      active_votes_[page_node] = ec;
    }
  } else {
    // The page is no longer closing: Remove the previously submitted vote.
    auto it = active_votes_.find(page_node);
    CHECK(it != active_votes_.end(), base::NotFatalUntil::M145);
    if (it != active_votes_.end()) {
      voting_channel_.InvalidateVote(it->second);
      active_votes_.erase(it);
    }
  }
}

}  // namespace performance_manager::execution_context_priority

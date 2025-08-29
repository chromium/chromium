// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/closing_page_voter.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
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

}  // namespace

// static
const char ClosingPageVoter::kPageIsClosingReason[] = "Page is closing.";

ClosingPageVoter::ClosingPageVoter() = default;
ClosingPageVoter::~ClosingPageVoter() = default;

void ClosingPageVoter::SetPageIsClosing(const PageNode* page_node,
                                        bool is_closing) {
  if (is_closing) {
    auto [it, inserted] = closing_pages_.insert(page_node);
    if (!inserted) {
      // TODO(crbug.com/432275395): Investigate cases where
      // SetPageIsClosing(true) is invoked multiple times.
      return;
    }
  } else {
    size_t num_removed = closing_pages_.erase(page_node);
    CHECK_EQ(num_removed, 1U, base::NotFatalUntil::M145);
  }

  for (const FrameNode* main_frame_node : page_node->GetMainFrameNodes()) {
    AdjustVotesForSubtree(main_frame_node, is_closing);
  }
}

void ClosingPageVoter::InitializeOnGraph(Graph* graph,
                                         VotingChannel voting_channel) {
  voting_channel_ = std::move(voting_channel);
  graph->AddPageNodeObserver(this);
  graph->AddFrameNodeObserver(this);
}

void ClosingPageVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  voting_channel_.Reset();
}

void ClosingPageVoter::OnBeforePageNodeRemoved(const PageNode* page_node) {
  // Assume that the page has no more frames.
  CHECK(page_node->GetMainFrameNodes().empty(), base::NotFatalUntil::M145);

  // Stop tracking the closing state for the page on removal.
  closing_pages_.erase(page_node);
}

void ClosingPageVoter::OnBeforeFrameNodeAdded(
    const FrameNode* frame_node,
    const FrameNode* pending_parent_frame_node,
    const PageNode* pending_page_node,
    const ProcessNode* pending_process_node,
    const FrameNode* pending_parent_or_outer_document_or_embedder) {
  if (base::Contains(closing_pages_, pending_page_node)) {
    // A frame is added to a closing page. Adjust the vote.
    AdjustVotesForSubtree(frame_node, /*is_closing=*/true);
  }
}

void ClosingPageVoter::OnBeforeFrameNodeRemoved(const FrameNode* frame_node) {
  // Invalidate vote on frame removal.
  if (base::Contains(closing_pages_, frame_node->GetPageNode())) {
    AdjustVotesForSubtree(frame_node, /*is_closing=*/false);
  }
}

void ClosingPageVoter::AdjustVotesForSubtree(const FrameNode* frame_node,
                                             bool is_closing) {
  if (is_closing) {
    voting_channel_.SubmitVote(
        GetExecutionContext(frame_node),
        Vote(base::TaskPriority::USER_BLOCKING, kPageIsClosingReason));
  } else {
    voting_channel_.InvalidateVote(GetExecutionContext(frame_node));
  }

  // Recurse through subtree.
  for (const FrameNode* child_frame_node : frame_node->GetChildFrameNodes()) {
    AdjustVotesForSubtree(child_frame_node, is_closing);
  }
}

}  // namespace performance_manager::execution_context_priority

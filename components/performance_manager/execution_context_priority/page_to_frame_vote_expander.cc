// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/page_to_frame_vote_expander.h"

#include "base/check.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/public/execution_context_priority/max_vote_aggregator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/node_state.h"

namespace performance_manager::execution_context_priority {

PageToFrameVoteExpander::PageToFrameVoteExpander(
    MaxVoteAggregator* max_vote_aggregator)
    : max_vote_aggregator_(max_vote_aggregator),
      voting_channel_(max_vote_aggregator_->GetVotingChannel()) {
  max_vote_aggregator_->AddObserver(this);
}

PageToFrameVoteExpander::~PageToFrameVoteExpander() {
  max_vote_aggregator_->RemoveObserver(this);
}

void PageToFrameVoteExpander::InitializeOnGraph(Graph* graph) {
  graph->AddFrameNodeObserver(this);
}

void PageToFrameVoteExpander::TearDownOnGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
}

void PageToFrameVoteExpander::OnPageTopVoteChanged(
    const PageNode* page_node,
    const std::optional<Vote>& vote) {
  GraphOperations::VisitFrameTreePreOrder(
      page_node, [&](const FrameNode* frame_node) {
        // Page votes can't change while a frame of the page is being removed.
        // This is not supported: depending on the observer order, the page
        // vote of the frame may already have been removed, and would be
        // leaked.
        CHECK(FrameNodeImpl::FromNode(frame_node)->GetNodeState() !=
              NodeState::kLeavingGraph)
            << "A page vote changed while a frame of the page is being "
               "removed";
        voting_channel_.SetVote(frame_node, vote);
        return true;
      });
}

void PageToFrameVoteExpander::OnBeforeFrameNodeAdded(
    const FrameNode* frame_node,
    const FrameNode* pending_parent_frame_node,
    const PageNode* pending_page_node,
    const ProcessNode* pending_process_node,
    const FrameNode* pending_parent_or_outer_document_or_embedder) {
  // The frame inherits the vote of its page.
  if (std::optional<Vote> page_vote =
          max_vote_aggregator_->GetVote(pending_page_node)) {
    voting_channel_.SetVote(frame_node, page_vote);
  }
}

void PageToFrameVoteExpander::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  // This is a no-op if the frame has no page vote.
  voting_channel_.SetVote(frame_node, std::nullopt);
}

}  // namespace performance_manager::execution_context_priority

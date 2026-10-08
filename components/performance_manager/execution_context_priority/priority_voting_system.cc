// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/execution_context_priority/priority_voting_system.h"

#include "base/check.h"
#include "components/performance_manager/decorators/process_priority_aggregator.h"
#include "components/performance_manager/execution_context_priority/page_to_frame_vote_expander.h"
#include "components/performance_manager/execution_context_priority/priority_setter.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager::execution_context_priority {

PriorityVotingSystem::PriorityVotingSystem()
    : priority_setter_(std::make_unique<PrioritySetter>(&max_vote_aggregator_)),
      page_to_frame_vote_expander_(
          std::make_unique<PageToFrameVoteExpander>(&max_vote_aggregator_)),
      process_priority_aggregator_(std::make_unique<ProcessPriorityAggregator>(
          max_vote_aggregator_.GetVotingChannel())) {}

PriorityVotingSystem::~PriorityVotingSystem() = default;

void PriorityVotingSystem::OnPassedToGraph(Graph* graph) {
  // The ProcessPriorityAggregator tracks frames and workers from when they are
  // added, so it would miss nodes that already exist.
  CHECK(graph->HasOnlySystemNode());

  graph->AddFrameNodeObserver(this);
  graph->AddPageNodeObserver(this);
  graph->AddProcessNodeObserver(this);
  graph->AddWorkerNodeObserver(this);
  page_to_frame_vote_expander_->InitializeOnGraph(graph);
  process_priority_aggregator_->InitializeOnGraph(graph);
}

void PriorityVotingSystem::OnTakenFromGraph(Graph* graph) {
  for (auto& priority_voter : priority_voters_) {
    priority_voter->TearDownOnGraph(graph);
  }
  process_priority_aggregator_->TearDownOnGraph(graph);
  page_to_frame_vote_expander_->TearDownOnGraph(graph);
  graph->RemoveWorkerNodeObserver(this);
  graph->RemoveProcessNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  graph->RemoveFrameNodeObserver(this);
}

void PriorityVotingSystem::AddPriorityVoter(
    std::unique_ptr<PriorityVoter> priority_voter) {
  CHECK(priority_voter);
  priority_voter->InitializeOnGraph(GetOwningGraph(),
                                    max_vote_aggregator_.GetVotingChannel());
  priority_voters_.push_back(std::move(priority_voter));
}

// The leak checks are done in the On*NodeRemoved() notifications, which are
// dispatched after every observer received OnBefore*NodeRemoved(), where voters
// remove their votes. This makes them independent of the observer order.

void PriorityVotingSystem::OnFrameNodeRemoved(
    const FrameNode* frame_node,
    const FrameNode* previous_parent_frame_node,
    const PageNode* previous_page_node,
    const ProcessNode* previous_process_node,
    const FrameNode* previous_parent_or_outer_document_or_embedder) {
  CHECK(!max_vote_aggregator_.HasVotes(frame_node))
      << "A voter did not remove its vote on a removed frame";
}

void PriorityVotingSystem::OnPageNodeRemoved(const PageNode* page_node) {
  CHECK(!max_vote_aggregator_.HasVotes(page_node))
      << "A voter did not remove its vote on a removed page";
}

void PriorityVotingSystem::OnProcessNodeRemoved(
    const ProcessNode* process_node) {
  CHECK(!max_vote_aggregator_.HasVotes(process_node))
      << "A voter did not remove its vote on a removed process";
}

void PriorityVotingSystem::OnWorkerNodeRemoved(
    const WorkerNode* worker_node,
    const ProcessNode* previous_process_node) {
  CHECK(!max_vote_aggregator_.HasVotes(worker_node))
      << "A voter did not remove its vote on a removed worker";
}

}  // namespace performance_manager::execution_context_priority

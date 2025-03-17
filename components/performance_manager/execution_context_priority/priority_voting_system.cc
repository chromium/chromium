// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/execution_context_priority/priority_voting_system.h"

#include "components/performance_manager/execution_context_priority/root_vote_observer.h"

namespace performance_manager::execution_context_priority {

PriorityVotingSystem::PriorityVotingSystem()
    : root_vote_observer_(std::make_unique<RootVoteObserver>()) {
  max_vote_aggregator_.SetUpstreamVotingChannel(
      root_vote_observer_->GetVotingChannel());
}

PriorityVotingSystem::~PriorityVotingSystem() = default;

void PriorityVotingSystem::OnTakenFromGraph(Graph* graph) {
  for (auto& priority_voter : priority_voters_) {
    priority_voter->TearDownOnGraph(graph);
  }
}

void PriorityVotingSystem::AddPriorityVoter(
    std::unique_ptr<PriorityVoter> priority_voter) {
  CHECK(priority_voter);
  priority_voter->InitializeOnGraph(GetOwningGraph(),
                                    max_vote_aggregator_.GetVotingChannel());
  priority_voters_.push_back(std::move(priority_voter));
}

}  // namespace performance_manager::execution_context_priority

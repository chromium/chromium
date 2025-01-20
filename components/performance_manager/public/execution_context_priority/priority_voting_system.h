// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_PRIORITY_VOTING_SYSTEM_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_PRIORITY_VOTING_SYSTEM_H_

#include <memory>
#include <utility>
#include <vector>

#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/execution_context_priority/max_vote_aggregator.h"
#include "components/performance_manager/public/graph/graph_registered.h"

namespace performance_manager {

class Graph;

namespace execution_context_priority {

class RootVoteObserver;

// Base interface for creating a voter class that can submit a vote to influence
// the priority of an execution context.
//
// Use `PriorityVotingSystem::AddPriorityVoter()` to register your voter.
class PriorityVoter {
 public:
  virtual ~PriorityVoter() = default;

  // Initializes the voter on the graph. Used to register as observers, and to
  // initialize the VotingChannel.
  virtual void InitializeOnGraph(Graph* graph,
                                 VotingChannel voting_channel) = 0;

  // Tears down the voter on the graph. Used to unregister as observers.
  virtual void TearDownOnGraph(Graph* graph) = 0;
};

// This class owns the voters that are responsible for deciding the priority of
// execution contexts.
class PriorityVotingSystem
    : public GraphOwnedAndRegistered<PriorityVotingSystem> {
 public:
  PriorityVotingSystem();
  ~PriorityVotingSystem() override;

  // Adds a new PriorityVoter to the graph.
  template <class T, class... Args>
  void AddPriorityVoter(Args&&... args) {
    AddPriorityVoter(std::make_unique<T>(std::forward(args)...));
  }

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override {}
  void OnTakenFromGraph(Graph* graph) override;

 private:
  void AddPriorityVoter(std::unique_ptr<PriorityVoter> priority_voter);

  // Takes in the aggregated votes and applies them to the execution contexts in
  // the graph.
  std::unique_ptr<RootVoteObserver> root_vote_observer_;

  // Aggregates all the votes from the voters.
  MaxVoteAggregator max_vote_aggregator_;

  std::vector<std::unique_ptr<PriorityVoter>> priority_voters_;
};

}  // namespace execution_context_priority

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_PRIORITY_VOTING_SYSTEM_H_

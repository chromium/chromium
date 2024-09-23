// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_EXECUTION_CONTEXT_PRIORITY_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_EXECUTION_CONTEXT_PRIORITY_DECORATOR_H_

#include <memory>
#include <utility>
#include <vector>

#include "components/performance_manager/execution_context_priority/max_vote_aggregator.h"
#include "components/performance_manager/execution_context_priority/override_vote_aggregator.h"
#include "components/performance_manager/execution_context_priority/root_vote_observer.h"
#include "components/performance_manager/execution_context_priority/voter_base.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager::execution_context_priority {

// The ExecutionContextPriorityDecorator's responsibility is to own the voting
// system that assigns the priority of every frame and worker in the graph.
//
// See the README.md for more details on the voting system.
class ExecutionContextPriorityDecorator final : public GraphOwned {
 public:
  ExecutionContextPriorityDecorator();
  ~ExecutionContextPriorityDecorator() override;

  ExecutionContextPriorityDecorator(const ExecutionContextPriorityDecorator&) =
      delete;
  ExecutionContextPriorityDecorator& operator=(
      const ExecutionContextPriorityDecorator&) = delete;

 private:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // Constructs a voter and adds it to `voters_`.
  template <class T, class... Args>
  void AddVoter(Args&&... args) {
    voters_.push_back(std::make_unique<T>(std::forward<Args>(args)...));
  }

  // Takes in the aggregated votes and applies them to the execution contexts in
  // the graph.
  RootVoteObserver root_vote_observer_;

  // Used to cast a negative vote that overrides the vote from
  // |max_vote_aggregator_|.
  OverrideVoteAggregator override_vote_aggregator_;

  // Aggregates all the votes from the voters.
  MaxVoteAggregator max_vote_aggregator_;

  std::vector<std::unique_ptr<VoterBase>> voters_;
};

}  // namespace performance_manager::execution_context_priority

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_EXECUTION_CONTEXT_PRIORITY_DECORATOR_H_

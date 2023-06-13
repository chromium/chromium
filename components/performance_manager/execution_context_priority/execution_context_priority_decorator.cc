// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/execution_context_priority_decorator.h"

#include "components/performance_manager/public/execution_context/execution_context_registry.h"

namespace performance_manager {
namespace execution_context_priority {

ExecutionContextPriorityDecorator::ExecutionContextPriorityDecorator() {
  // The following schema describes the structure of the voting tree. Arrows are
  // voting channels.
  //
  // Note: |ad_frame_voter_| is currently the only downvoter but there could
  // possibly be more should the need arise. Their votes would be aggregated
  // using some sort of MinVoteAggregator.
  //
  //            |root_vote_observer_|
  //                     ^
  //                     |
  //         |override_vote_aggregator_|
  //            ^                  ^
  //            | (override)       | (default)
  //            |                  |
  //        Downvoter         |max_vote_aggregator_|
  //                              ^    ^        ^
  //                             /     |         \
  //                            /      |          \
  //                         Voter1, Voter2, ..., VoterN
  //
  // Set up the voting tree from top to bottom.
  override_vote_aggregator_.SetUpstreamVotingChannel(
      root_vote_observer_.GetVotingChannel());
  max_vote_aggregator_.SetUpstreamVotingChannel(
      override_vote_aggregator_.GetDefaultVotingChannel());

  // Set up downvoter.
  ad_frame_voter_.SetVotingChannel(
      override_vote_aggregator_.GetOverrideVotingChannel());

  // Set up voters.
  frame_visibility_voter_.SetVotingChannel(
      max_vote_aggregator_.GetVotingChannel());
  frame_audible_voter_.SetVotingChannel(
      max_vote_aggregator_.GetVotingChannel());
  inherit_client_priority_voter_.SetVotingChannel(
      max_vote_aggregator_.GetVotingChannel());
}

ExecutionContextPriorityDecorator::~ExecutionContextPriorityDecorator() =
    default;

void ExecutionContextPriorityDecorator::OnPassedToGraph(Graph* graph) {
  // Subscribe voters to the graph.
  graph->AddInitializingFrameNodeObserver(&ad_frame_voter_);
  graph->AddInitializingFrameNodeObserver(&frame_visibility_voter_);
  graph->AddInitializingFrameNodeObserver(&frame_audible_voter_);
  graph->AddFrameNodeObserver(&inherit_client_priority_voter_);
  graph->AddWorkerNodeObserver(&inherit_client_priority_voter_);
}

void ExecutionContextPriorityDecorator::OnTakenFromGraph(Graph* graph) {
  // Unsubscribe voters from the graph.
  graph->RemoveWorkerNodeObserver(&inherit_client_priority_voter_);
  graph->RemoveFrameNodeObserver(&inherit_client_priority_voter_);
  graph->RemoveInitializingFrameNodeObserver(&frame_audible_voter_);
  graph->RemoveInitializingFrameNodeObserver(&frame_visibility_voter_);
  graph->RemoveInitializingFrameNodeObserver(&ad_frame_voter_);
}

}  // namespace execution_context_priority
}  // namespace performance_manager

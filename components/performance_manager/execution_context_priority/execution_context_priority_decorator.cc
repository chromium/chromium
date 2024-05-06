// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/execution_context_priority_decorator.h"

#include "base/feature_list.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/features.h"

namespace performance_manager::execution_context_priority {

ExecutionContextPriorityDecorator::ExecutionContextPriorityDecorator() {
#if BUILDFLAG(IS_MAC)
  if (features::kBoostChildFrames.Get()) {
    child_frame_booster_ =
        std::make_unique<ChildFrameBooster>(&boosting_vote_aggregator_);
  }
#endif

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
  //        Downvoter        |boosting_vote_aggregator_| <-------------
  //                                      ^                           |
  //                                      |                           |
  //                           |max_vote_aggregator_|             Boosters
  //                              ^    ^        ^
  //                             /     |         \
  //                            /      |          \
  //                         Voter1, Voter2, ..., VoterN
  //
  // Set up the voting tree from top to bottom.
  override_vote_aggregator_.SetUpstreamVotingChannel(
      root_vote_observer_.GetVotingChannel());
  boosting_vote_aggregator_.SetUpstreamVotingChannel(
      override_vote_aggregator_.GetDefaultVotingChannel());
  max_vote_aggregator_.SetUpstreamVotingChannel(
      boosting_vote_aggregator_.GetVotingChannel());

  // Set up downvoter.
  ad_frame_voter_.SetVotingChannel(
      override_vote_aggregator_.GetOverrideVotingChannel());

  // Set up voters.
  frame_visibility_voter_.SetVotingChannel(
      max_vote_aggregator_.GetVotingChannel());
  frame_audible_voter_.SetVotingChannel(
      max_vote_aggregator_.GetVotingChannel());
  frame_capturing_media_stream_voter_.SetVotingChannel(
      max_vote_aggregator_.GetVotingChannel());
  inherit_client_priority_voter_.SetVotingChannel(
      max_vote_aggregator_.GetVotingChannel());
  loading_page_voter_.SetVotingChannel(max_vote_aggregator_.GetVotingChannel());
}

ExecutionContextPriorityDecorator::~ExecutionContextPriorityDecorator() =
    default;

void ExecutionContextPriorityDecorator::OnPassedToGraph(Graph* graph) {
  // Subscribe voters and boosters to the graph.
  if (features::kDownvoteAdFrames.Get()) {
    graph->AddInitializingFrameNodeObserver(&ad_frame_voter_);
  }
  graph->AddInitializingFrameNodeObserver(&frame_visibility_voter_);
  graph->AddInitializingFrameNodeObserver(&frame_audible_voter_);
  graph->AddInitializingFrameNodeObserver(&frame_capturing_media_stream_voter_);
  graph->AddFrameNodeObserver(&inherit_client_priority_voter_);
  graph->AddWorkerNodeObserver(&inherit_client_priority_voter_);
  if (base::FeatureList::IsEnabled(features::kPMLoadingPageVoter)) {
    graph->AddPageNodeObserver(&loading_page_voter_);
    graph->AddInitializingFrameNodeObserver(&loading_page_voter_);
  }
#if BUILDFLAG(IS_MAC)
  if (features::kBoostChildFrames.Get()) {
    graph->AddInitializingFrameNodeObserver(child_frame_booster_.get());
  }
#endif
}

void ExecutionContextPriorityDecorator::OnTakenFromGraph(Graph* graph) {
  // Unsubscribe voters and boosters from the graph.
#if BUILDFLAG(IS_MAC)
  if (features::kBoostChildFrames.Get()) {
    graph->RemoveInitializingFrameNodeObserver(child_frame_booster_.get());
  }
#endif
  if (base::FeatureList::IsEnabled(features::kPMLoadingPageVoter)) {
    graph->RemoveInitializingFrameNodeObserver(&loading_page_voter_);
    graph->RemovePageNodeObserver(&loading_page_voter_);
  }
  graph->RemoveWorkerNodeObserver(&inherit_client_priority_voter_);
  graph->RemoveFrameNodeObserver(&inherit_client_priority_voter_);
  graph->RemoveInitializingFrameNodeObserver(
      &frame_capturing_media_stream_voter_);
  graph->RemoveInitializingFrameNodeObserver(&frame_audible_voter_);
  graph->RemoveInitializingFrameNodeObserver(&frame_visibility_voter_);
  if (features::kDownvoteAdFrames.Get()) {
    graph->RemoveInitializingFrameNodeObserver(&ad_frame_voter_);
  }
}

}  // namespace performance_manager::execution_context_priority

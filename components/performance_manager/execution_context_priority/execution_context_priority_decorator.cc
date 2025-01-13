// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/execution_context_priority_decorator.h"

#include "base/feature_list.h"
#include "components/performance_manager/execution_context_priority/frame_audible_voter.h"
#include "components/performance_manager/execution_context_priority/frame_capturing_media_stream_voter.h"
#include "components/performance_manager/execution_context_priority/frame_visibility_voter.h"
#include "components/performance_manager/execution_context_priority/inherit_client_priority_voter.h"
#include "components/performance_manager/execution_context_priority/loading_page_voter.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/features.h"

#if BUILDFLAG(IS_MAC)
#include "components/performance_manager/execution_context_priority/inherit_parent_priority_voter.h"
#endif  // BUILDFLAG(IS_MAC)

namespace performance_manager::execution_context_priority {

ExecutionContextPriorityDecorator::ExecutionContextPriorityDecorator() {
  // The following schema describes the structure of the voting tree. Arrows are
  // voting channels.
  //
  //      |root_vote_observer_|
  //               ^
  //               |
  //     |max_vote_aggregator_|
  //        ^    ^        ^
  //       /     |         \
  //      /      |          \
  //  Voter1, Voter2, ..., VoterN
  //

  // --- Set up the MaxVoteAggregator. ---

  max_vote_aggregator_.SetUpstreamVotingChannel(
      root_vote_observer_.GetVotingChannel());

  // --- Set up voters. ---

  // Casts a USER_BLOCKING vote when a frame is visible.
  AddVoter<FrameVisibilityVoter>(max_vote_aggregator_.GetVotingChannel());

  // Casts a USER_BLOCKING vote when a frame is audible.
  AddVoter<FrameAudibleVoter>(max_vote_aggregator_.GetVotingChannel());

  // Casts a USER_BLOCKING vote when a frame is capturing a media stream.
  AddVoter<FrameCapturingMediaStreamVoter>(
      max_vote_aggregator_.GetVotingChannel());

  // Casts a vote for each child worker with the client's priority.
  AddVoter<InheritClientPriorityVoter>(max_vote_aggregator_.GetVotingChannel());

  // Casts a USER_BLOCKING vote for all frames in a loading page.
  if (base::FeatureList::IsEnabled(features::kPMLoadingPageVoter)) {
    AddVoter<LoadingPageVoter>(max_vote_aggregator_.GetVotingChannel());
  }

#if BUILDFLAG(IS_MAC)
  // Casts a vote for each child frame with the parent's priority.
  if (features::kInheritParentPriority.Get()) {
    AddVoter<InheritParentPriorityVoter>(
        max_vote_aggregator_.GetVotingChannel());
  }
#endif
}

ExecutionContextPriorityDecorator::~ExecutionContextPriorityDecorator() =
    default;

void ExecutionContextPriorityDecorator::OnPassedToGraph(Graph* graph) {
  for (auto& voter : voters_) {
    voter->InitializeOnGraph(graph);
  }
}

void ExecutionContextPriorityDecorator::OnTakenFromGraph(Graph* graph) {
  for (auto& voter : voters_) {
    voter->TearDownOnGraph(graph);
  }
}

}  // namespace performance_manager::execution_context_priority

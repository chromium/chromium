// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_FREEZING_VOTE_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_FREEZING_VOTE_DECORATOR_H_

#include "components/performance_manager/freezing/freezing_vote_aggregator.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager {

class PageNode;

// Decorator that adorns PageNodes with a FreezingVote.
//
// This decorator owns a GraphRegistered FreezingVoteAggregator instance that
// can be used to submit votes for a given PageNode. Components interested in
// submitting a freezing vote should do the following:
//   voting::VotingChannelWrapper<FreezingVote> voter;
//   voter.SetVotingChannel(
//      graph()->GetRegisteredObjectAs<freezing::FreezingVoteAggregator>()
//          ->GetVotingChannel());
//   voter.SubmitVote(page_node,
//       FreezingVote(FreezingVoteValue::kCannotFreeze, "reason"));
//
// The aggregator is responsible for upstreaming the final vote to this
// decorator.
class FreezingVoteDecorator : public GraphOwnedDefaultImpl,
                              public freezing::FreezingVoteObserver {
 public:
  FreezingVoteDecorator();
  ~FreezingVoteDecorator() override;

  FreezingVoteDecorator(const FreezingVoteDecorator&) = delete;
  FreezingVoteDecorator& operator=(const FreezingVoteDecorator&) = delete;

 private:
  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // FreezingVoteObserver implementation:
  void OnVoteSubmitted(freezing::FreezingVoterId voter_id,
                       const PageNode* page_node,
                       const freezing::FreezingVote& vote) override;
  void OnVoteChanged(freezing::FreezingVoterId voter_id,
                     const PageNode* page_node,
                     const freezing::FreezingVote& new_vote) override;
  void OnVoteInvalidated(freezing::FreezingVoterId voter_id,
                         const PageNode* page_node) override;

  freezing::FreezingVotingChannelFactory freezing_voting_channel_factory_{this};
  freezing::FreezingVoteAggregator freezing_vote_aggregator_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_FREEZING_VOTE_DECORATOR_H_

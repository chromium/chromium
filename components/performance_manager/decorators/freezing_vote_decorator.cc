// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/freezing_vote_decorator.h"

#include "components/performance_manager/graph/page_node_impl.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager {

FreezingVoteDecorator::FreezingVoteDecorator() {
  freezing_vote_aggregator_.SetUpstreamVotingChannel(
      freezing_voting_channel_factory_.BuildVotingChannel());
}

FreezingVoteDecorator::~FreezingVoteDecorator() = default;

void FreezingVoteDecorator::OnPassedToGraph(Graph* graph) {
  graph->RegisterObject(&freezing_vote_aggregator_);
  freezing_vote_aggregator_.RegisterNodeDataDescriber(graph);
}

void FreezingVoteDecorator::OnTakenFromGraph(Graph* graph) {
  freezing_vote_aggregator_.UnregisterNodeDataDescriber(graph);
  graph->UnregisterObject(&freezing_vote_aggregator_);
}

void FreezingVoteDecorator::OnVoteSubmitted(
    freezing::FreezingVoterId voter_id,
    const PageNode* page_node,
    const freezing::FreezingVote& vote) {
  DCHECK_EQ(NodeState::kActiveInGraph, page_node->GetNodeState());
  PageNodeImpl::FromNode(page_node)->set_freezing_vote(vote);
}

void FreezingVoteDecorator::OnVoteChanged(
    freezing::FreezingVoterId voter_id,
    const PageNode* page_node,
    const freezing::FreezingVote& new_vote) {
  DCHECK_EQ(NodeState::kActiveInGraph, page_node->GetNodeState());
  PageNodeImpl::FromNode(page_node)->set_freezing_vote(new_vote);
}

void FreezingVoteDecorator::OnVoteInvalidated(
    freezing::FreezingVoterId voter_id,
    const PageNode* page_node) {
  // Don't change votes for pages that are being removed from the graph. This
  // causes recursive notifications and useless policy dispatches.
  if (page_node->GetNodeState() == NodeState::kLeavingGraph)
    return;
  PageNodeImpl::FromNode(page_node)->set_freezing_vote(absl::nullopt);
}

}  // namespace performance_manager

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/freezing_vote_aggregator.h"

#include <algorithm>

#include "base/stl_util.h"

namespace performance_manager {
namespace freezing {

FreezingVoteAggregator::FreezingVoteAggregator()
    : vote_consumer_default_impl_(this) {}
FreezingVoteAggregator::~FreezingVoteAggregator() = default;

FreezingVotingChannel FreezingVoteAggregator::GetVotingChannel() {
  return vote_consumer_default_impl_.BuildVotingChannel();
}

void FreezingVoteAggregator::SetUpstreamVotingChannel(
    FreezingVotingChannel&& channel) {
  DCHECK(channel.IsValid());
  DCHECK(vote_data_map_.empty());
  DCHECK(!channel_.IsValid());
  channel_ = std::move(channel);
}

void FreezingVoteAggregator::OnVoteSubmitted(FreezingVoterId voter_id,
                                             const PageNode* page_node,
                                             const FreezingVote& vote) {
  DCHECK(vote.IsValid());
  DCHECK(channel_.IsValid());

  auto& vote_data = vote_data_map_[page_node];

  if (vote_data.AddVote(voter_id, vote) ==
      FreezingVoteData::UpstreamVoteImpact::kUpstreamVoteChanged) {
    vote_data.UpstreamVote(page_node, &channel_);
  }
}

void FreezingVoteAggregator::OnVoteChanged(FreezingVoterId voter_id,
                                           const PageNode* page_node,
                                           const FreezingVote& new_vote) {
  auto& vote_data = GetVoteData(page_node)->second;

  if (vote_data.UpdateVote(voter_id, new_vote) ==
      FreezingVoteData::UpstreamVoteImpact::kUpstreamVoteChanged) {
    vote_data.UpstreamVote(page_node, &channel_);
  }
}

void FreezingVoteAggregator::OnVoteInvalidated(FreezingVoterId voter_id,
                                               const PageNode* page_node) {
  auto it = GetVoteData(page_node);
  auto& vote_data = it->second;

  auto remove_vote_result = vote_data.RemoveVote(voter_id);
  // Remove the vote, and upstream if necessary.
  if (remove_vote_result ==
      FreezingVoteData::UpstreamVoteImpact::kUpstreamVoteChanged) {
    vote_data.UpstreamVote(page_node, &channel_);
  }

  // If all the votes for this PageNode have disappeared then remove the entry
  // entirely. This will release the receipt that it contains and will cancel
  // our upstream vote.
  if (remove_vote_result ==
      FreezingVoteData::UpstreamVoteImpact::kUpstreamVoteRemoved) {
    vote_data_map_.erase(it);
  }
}

FreezingVoteAggregator::FreezingVoteData::FreezingVoteData() = default;
FreezingVoteAggregator::FreezingVoteData::FreezingVoteData(FreezingVoteData&&) =
    default;
FreezingVoteAggregator::FreezingVoteData&
FreezingVoteAggregator::FreezingVoteData::operator=(
    FreezingVoteAggregator::FreezingVoteData&& rhs) = default;
FreezingVoteAggregator::FreezingVoteData::~FreezingVoteData() = default;

FreezingVoteAggregator::FreezingVoteData::UpstreamVoteImpact
FreezingVoteAggregator::FreezingVoteData::AddVote(FreezingVoterId voter_id,
                                                  const FreezingVote& vote) {
  auto current_decision = FreezingVoteValue::kCanFreeze;
  if (votes_.size())
    current_decision = GetCurrentVote().value();

  AddVoteToDeque(voter_id, vote);

  // Always report the first vote.
  if (votes_.size() == 1)
    return UpstreamVoteImpact::kUpstreamVoteChanged;

  return (current_decision != GetCurrentVote().value())
             ? UpstreamVoteImpact::kUpstreamVoteChanged
             : UpstreamVoteImpact::kUpstreamVoteUnchanged;
}

FreezingVoteAggregator::FreezingVoteData::UpstreamVoteImpact
FreezingVoteAggregator::FreezingVoteData::UpdateVote(
    FreezingVoterId voter_id,
    const FreezingVote& new_vote) {
  auto current_decision = GetCurrentVote().value();

  auto it = FindVote(voter_id);
  DCHECK(it != votes_.end());
  votes_.erase(it);

  AddVoteToDeque(voter_id, new_vote);

  return (current_decision != GetCurrentVote().value())
             ? UpstreamVoteImpact::kUpstreamVoteChanged
             : UpstreamVoteImpact::kUpstreamVoteUnchanged;
}

FreezingVoteAggregator::FreezingVoteData::UpstreamVoteImpact
FreezingVoteAggregator::FreezingVoteData::RemoveVote(FreezingVoterId voter_id) {
  auto current_decision = GetCurrentVote().value();

  votes_.erase(FindVote(voter_id));

  // Indicate that the upstream vote should be removed.
  if (votes_.empty())
    return UpstreamVoteImpact::kUpstreamVoteRemoved;

  return (current_decision != GetCurrentVote().value())
             ? UpstreamVoteImpact::kUpstreamVoteChanged
             : UpstreamVoteImpact::kUpstreamVoteUnchanged;
}

void FreezingVoteAggregator::FreezingVoteData::UpstreamVote(
    const PageNode* page_node,
    FreezingVotingChannel* channel) {
  DCHECK_NE(0u, votes_.size());
  auto& vote = GetCurrentVote();

  // Change our existing vote, or create a new one as necessary.
  if (receipt_.HasVote()) {
    receipt_.ChangeVote(vote.value(), vote.reason());
  } else {
    receipt_ = channel->SubmitVote(page_node, vote);
  }
}

const FreezingVote& FreezingVoteAggregator::FreezingVoteData::GetCurrentVote() {
  DCHECK(!IsEmpty());
  // The set of votes is ordered and the first one in the set is the one that
  // should be sent to the consumer.
  return votes_.begin()->second;
}

FreezingVoteAggregator::FreezingVoteData::VotesDeque::iterator
FreezingVoteAggregator::FreezingVoteData::FindVote(FreezingVoterId voter_id) {
  // TODO(sebmarchand): Consider doing a reverse search for kCanFreeze votes and
  // a normal one for kCannotFreeze votes.

  auto it =
      std::find_if(votes_.begin(), votes_.end(),
                   [voter_id](const auto& e) { return e.first == voter_id; });
  DCHECK(it != votes_.end());
  return it;
}

void FreezingVoteAggregator::FreezingVoteData::AddVoteToDeque(
    FreezingVoterId voter_id,
    const FreezingVote& vote) {
  DCHECK(std::find_if(votes_.begin(), votes_.end(), [voter_id](const auto& e) {
           return e.first == voter_id;
         }) == votes_.end());
  if (vote.value() == FreezingVoteValue::kCannotFreeze) {
    votes_.emplace_front(voter_id, vote);
  } else {
    votes_.emplace_back(voter_id, vote);
  }
}

FreezingVoteAggregator::VoteDataMap::iterator
FreezingVoteAggregator::GetVoteData(const PageNode* page_node) {
  auto it = vote_data_map_.find(page_node);
  DCHECK(it != vote_data_map_.end());
  return it;
}

}  // namespace freezing
}  // namespace performance_manager

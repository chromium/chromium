// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/freezing_vote_aggregator.h"

#include <algorithm>

#include "base/stl_util.h"

namespace performance_manager {
namespace freezing {

FreezingVoteAggregator::FreezingVoteAggregator() : factory_(this) {}
FreezingVoteAggregator::~FreezingVoteAggregator() = default;

FreezingVotingChannel FreezingVoteAggregator::GetVotingChannel() {
  return factory_.BuildVotingChannel();
}

void FreezingVoteAggregator::SetUpstreamVotingChannel(
    FreezingVotingChannel&& channel) {
  DCHECK(channel.IsValid());
  DCHECK(vote_data_map_.empty());
  DCHECK(!channel_.IsValid());
  channel_ = std::move(channel);
}

FreezingVoteReceipt FreezingVoteAggregator::SubmitVote(
    util::PassKey<FreezingVotingChannel>,
    voting::VoterId<FreezingVote> voter_id,
    const PageNode* page_node,
    const FreezingVote& vote) {
  DCHECK(vote.IsValid());
  DCHECK(channel_.IsValid());

  auto& vote_data = vote_data_map_[page_node];

  AcceptedFreezingVote accepted_vote(this, voter_id, page_node, vote);
  auto receipt = accepted_vote.IssueReceipt();
  if (vote_data.AddVote(std::move(accepted_vote)) ==
      FreezingVoteData::UpstreamVoteImpact::kUpstreamVoteChanged) {
    vote_data.UpstreamVote(&channel_);
  }

  // Return a vote receipt to our voter for the received vote.
  return receipt;
}

void FreezingVoteAggregator::ChangeVote(util::PassKey<AcceptedFreezingVote>,
                                        AcceptedFreezingVote* old_vote,
                                        const FreezingVote& new_vote) {
  DCHECK(old_vote->IsValid());

  auto& vote_data = GetVoteData(old_vote->context())->second;

  if (vote_data.UpdateVote(old_vote, new_vote) ==
      FreezingVoteData::UpstreamVoteImpact::kUpstreamVoteChanged) {
    vote_data.UpstreamVote(&channel_);
  }
}

void FreezingVoteAggregator::VoteInvalidated(
    util::PassKey<AcceptedFreezingVote>,
    AcceptedFreezingVote* vote) {
  DCHECK(!vote->IsValid());
  auto it = GetVoteData(vote->context());
  auto& vote_data = it->second;

  auto remove_vote_result = vote_data.RemoveVote(vote);
  // Remove the vote, and upstream if necessary.
  if (remove_vote_result ==
      FreezingVoteData::UpstreamVoteImpact::kUpstreamVoteChanged) {
    vote_data.UpstreamVote(&channel_);
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
FreezingVoteAggregator::FreezingVoteData::AddVote(AcceptedFreezingVote&& vote) {
  auto current_decision = FreezingVoteValue::kCanFreeze;
  if (accepted_votes_.size())
    current_decision = GetCurrentVote().vote().value();

  AddVoteToDeque(std::move(vote));

  // Always report the first vote.
  if (accepted_votes_.size() == 1)
    return UpstreamVoteImpact::kUpstreamVoteChanged;

  return (current_decision != GetCurrentVote().vote().value())
             ? UpstreamVoteImpact::kUpstreamVoteChanged
             : UpstreamVoteImpact::kUpstreamVoteUnchanged;
}

FreezingVoteAggregator::FreezingVoteData::UpstreamVoteImpact
FreezingVoteAggregator::FreezingVoteData::UpdateVote(
    AcceptedFreezingVote* old_vote,
    const FreezingVote& new_vote) {
  auto current_decision = GetCurrentVote().vote().value();

  auto it = FindVote(old_vote);
  DCHECK(it != accepted_votes_.end());
  auto vote = std::move(*it);
  accepted_votes_.erase(it);
  vote.UpdateVote(new_vote);
  AddVoteToDeque(std::move(vote));

  return (current_decision != GetCurrentVote().vote().value())
             ? UpstreamVoteImpact::kUpstreamVoteChanged
             : UpstreamVoteImpact::kUpstreamVoteUnchanged;
}

FreezingVoteAggregator::FreezingVoteData::UpstreamVoteImpact
FreezingVoteAggregator::FreezingVoteData::RemoveVote(
    AcceptedFreezingVote* vote) {
  auto current_decision = GetCurrentVote().vote().value();

  accepted_votes_.erase(FindVote(vote));

  // Indicate that the upstream vote should be removed.
  if (accepted_votes_.empty())
    return UpstreamVoteImpact::kUpstreamVoteRemoved;

  return (current_decision != GetCurrentVote().vote().value())
             ? UpstreamVoteImpact::kUpstreamVoteChanged
             : UpstreamVoteImpact::kUpstreamVoteUnchanged;
}

void FreezingVoteAggregator::FreezingVoteData::UpstreamVote(
    FreezingVotingChannel* channel) {
  DCHECK_NE(0u, accepted_votes_.size());
  auto& vote = GetCurrentVote();

  // Change our existing vote, or create a new one as necessary.
  if (receipt_.HasVote()) {
    receipt_.ChangeVote(vote.vote().value(), vote.vote().reason());
  } else {
    receipt_ = channel->SubmitVote(vote.context(), vote.vote());
  }
}

const AcceptedFreezingVote&
FreezingVoteAggregator::FreezingVoteData::GetCurrentVote() {
  DCHECK(!IsEmpty());
  // The set of votes is ordered and the first one in the set is the one that
  // should be sent to the consumer.
  return *accepted_votes_.begin();
}

FreezingVoteAggregator::FreezingVoteData::AcceptedVotesDeque::iterator
FreezingVoteAggregator::FreezingVoteData::FindVote(AcceptedFreezingVote* vote) {
  // TODO(sebmarchand): Consider doing a reverse search for kCanFreeze votes and
  // a normal one for kCannotFreeze votes.

  auto it = std::find_if(accepted_votes_.begin(), accepted_votes_.end(),
                         [vote](const auto& rhs) { return &rhs == vote; });
  DCHECK(it != accepted_votes_.end());
  return it;
}

void FreezingVoteAggregator::FreezingVoteData::AddVoteToDeque(
    AcceptedFreezingVote&& vote) {
  if (vote.vote().value() == FreezingVoteValue::kCannotFreeze) {
    accepted_votes_.push_front(std::move(vote));
  } else {
    accepted_votes_.push_back(std::move(vote));
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
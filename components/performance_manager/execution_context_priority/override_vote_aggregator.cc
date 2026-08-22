// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/execution_context_priority/override_vote_aggregator.h"

namespace performance_manager {
namespace execution_context_priority {

OverrideVoteAggregator::OverrideVoteAggregator() = default;

OverrideVoteAggregator::~OverrideVoteAggregator() = default;

VotingChannel OverrideVoteAggregator::GetOverrideVotingChannel() {
  DCHECK(vote_data_map_.empty());
  DCHECK(!override_voter_id_);
  DCHECK_GT(2u, voting_channel_factory_.voting_channels_issued());
  auto channel = voting_channel_factory_.BuildVotingChannel();
  override_voter_id_ = channel.voter_id();
  return channel;
}

VotingChannel OverrideVoteAggregator::GetDefaultVotingChannel() {
  DCHECK(vote_data_map_.empty());
  DCHECK(!default_voter_id_);
  DCHECK_GT(2u, voting_channel_factory_.voting_channels_issued());
  auto channel = voting_channel_factory_.BuildVotingChannel();
  default_voter_id_ = channel.voter_id();
  return channel;
}

void OverrideVoteAggregator::SetUpstreamVotingChannel(VotingChannel channel) {
  channel_ = std::move(channel);
}

bool OverrideVoteAggregator::IsSetup() const {
  return override_voter_id_ && default_voter_id_ && channel_.IsValid();
}

void OverrideVoteAggregator::OnVoteSet(
    VoterId voter_id,
    const ExecutionContext* execution_context,
    const std::optional<Vote>& vote) {
  DCHECK(IsSetup());
  VoteData::VoterType voter_type = GetVoterType(voter_id);

  if (!vote.has_value()) {
    // Vote removal.
    auto it = vote_data_map_.find(execution_context);
    if (it == vote_data_map_.end()) {
      return;
    }

    VoteData& vote_data = it->second;

    const std::optional<Vote> old_chosen_vote = vote_data.GetChosenVote();
    vote_data.SetVote(voter_type, std::nullopt);
    const std::optional<Vote> new_chosen_vote = vote_data.GetChosenVote();

    if (old_chosen_vote != new_chosen_vote) {
      channel_.SetVote(execution_context, new_chosen_vote);
    }
    if (!new_chosen_vote.has_value()) {
      vote_data_map_.erase(it);
    }
    return;
  }

  // Vote addition or modification.
  auto [it, _] = vote_data_map_.try_emplace(execution_context);
  VoteData& vote_data = it->second;

  const std::optional<Vote> old_chosen_vote = vote_data.GetChosenVote();
  vote_data.SetVote(voter_type, vote);
  const std::optional<Vote> new_chosen_vote = vote_data.GetChosenVote();

  if (old_chosen_vote != new_chosen_vote) {
    channel_.SetVote(execution_context, new_chosen_vote);
  }
}

OverrideVoteAggregator::VoteData::VoteData() = default;
OverrideVoteAggregator::VoteData::VoteData(VoteData&& rhs) = default;
OverrideVoteAggregator::VoteData::~VoteData() = default;

void OverrideVoteAggregator::VoteData::SetVote(
    VoterType voter_type,
    const std::optional<Vote>& vote) {
  switch (voter_type) {
    case VoterType::kDefault:
      default_vote_ = vote;
      break;
    case VoterType::kOverride:
      override_vote_ = vote;
      break;
  }
}

std::optional<Vote> OverrideVoteAggregator::VoteData::GetChosenVote() const {
  // The |override_vote| is always chosen first.
  if (override_vote_.has_value()) {
    return override_vote_;
  }
  return default_vote_;
}

OverrideVoteAggregator::VoteData::VoterType
OverrideVoteAggregator::GetVoterType(VoterId voter_id) const {
  DCHECK(voter_id == default_voter_id_ || voter_id == override_voter_id_);
  return voter_id == default_voter_id_ ? VoteData::VoterType::kDefault
                                       : VoteData::VoterType::kOverride;
}

}  // namespace execution_context_priority
}  // namespace performance_manager

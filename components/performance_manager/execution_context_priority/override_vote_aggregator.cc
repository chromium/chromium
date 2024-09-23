// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/override_vote_aggregator.h"

#include "base/not_fatal_until.h"

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

void OverrideVoteAggregator::OnVoteSubmitted(
    VoterId voter_id,
    const ExecutionContext* execution_context,
    const Vote& vote) {
  DCHECK(IsSetup());
  // Create the VoteData for this execution context, if necessary.
  VoteData& vote_data = vote_data_map_[execution_context];

  // Remember the previous chosen vote before adding the new vote. There could
  // be none if the this the first vote submitted for |execution_context|.
  std::optional<Vote> old_chosen_vote;
  if (vote_data.HasChosenVote())
    old_chosen_vote = vote_data.GetChosenVote();

  vote_data.AddVote(GetVoterType(voter_id), vote);

  // If there was no previous chosen vote, the vote must be submitted.
  if (!old_chosen_vote) {
    channel_.SubmitVote(execution_context, vote);
    return;
  }

  // Since there is a previous chosen vote, it must be modified if the chosen
  // vote changed.
  const Vote new_chosen_vote = vote_data.GetChosenVote();
  if (old_chosen_vote.value() != new_chosen_vote)
    channel_.ChangeVote(execution_context, new_chosen_vote);
}

void OverrideVoteAggregator::OnVoteChanged(
    VoterId voter_id,
    const ExecutionContext* execution_context,
    const Vote& new_vote) {
  // The VoteData for this execution context is guaranteed to exist.
  VoteData& vote_data = GetVoteData(execution_context)->second;

  // Remember the previous chosen vote before updating the vote for this
  // |voter_id|.
  const Vote old_chosen_vote = vote_data.GetChosenVote();

  vote_data.ChangeVote(GetVoterType(voter_id), new_vote);

  // If the chosen vote changed, the upstream vote must also be changed.
  const Vote new_chosen_vote = vote_data.GetChosenVote();
  if (old_chosen_vote != new_chosen_vote)
    channel_.ChangeVote(execution_context, new_chosen_vote);
}

void OverrideVoteAggregator::OnVoteInvalidated(
    VoterId voter_id,
    const ExecutionContext* execution_context) {
  // The VoteData for this execution context is guaranteed to exist.
  auto it = GetVoteData(execution_context);
  VoteData& vote_data = it->second;

  // Remember the previous chosen vote before removing the vote for this
  // |voter_id|.
  const Vote old_chosen_vote = vote_data.GetChosenVote();

  vote_data.RemoveVote(GetVoterType(voter_id));

  // In case the last vote for |execution_context| was invalidated, the upstream
  // vote must also be invalidated.
  if (!vote_data.HasChosenVote()) {
    channel_.InvalidateVote(execution_context);

    // Clean up the VoteData for |execution_context| since it is empty.
    vote_data_map_.erase(it);
    return;
  }

  // If the top vote changed, the upstream vote must also be changed.
  const Vote new_chosen_vote = vote_data.GetChosenVote();
  if (old_chosen_vote != new_chosen_vote)
    channel_.ChangeVote(execution_context, new_chosen_vote);
}

OverrideVoteAggregator::VoteData::VoteData() = default;
OverrideVoteAggregator::VoteData::VoteData(VoteData&& rhs) = default;
OverrideVoteAggregator::VoteData::~VoteData() = default;

void OverrideVoteAggregator::VoteData::AddVote(VoterType voter_type,
                                               const Vote& vote) {
  switch (voter_type) {
    case VoterType::kDefault:
      DCHECK(!default_vote_.has_value());
      default_vote_ = vote;
      break;
    case VoterType::kOverride:
      DCHECK(!override_vote_.has_value());
      override_vote_ = vote;
      break;
  }
}

void OverrideVoteAggregator::VoteData::ChangeVote(VoterType voter_type,
                                                  const Vote& new_vote) {
  switch (voter_type) {
    case VoterType::kDefault:
      DCHECK(default_vote_.has_value());
      default_vote_ = new_vote;
      break;
    case VoterType::kOverride:
      DCHECK(override_vote_.has_value());
      override_vote_ = new_vote;
      break;
  }
}

void OverrideVoteAggregator::VoteData::RemoveVote(VoterType voter_type) {
  switch (voter_type) {
    case VoterType::kDefault:
      DCHECK(default_vote_.has_value());
      default_vote_ = std::nullopt;
      break;
    case VoterType::kOverride:
      DCHECK(override_vote_.has_value());
      override_vote_ = std::nullopt;
      break;
  }
}

bool OverrideVoteAggregator::VoteData::HasChosenVote() const {
  return default_vote_.has_value() || override_vote_.has_value();
}

const Vote& OverrideVoteAggregator::VoteData::GetChosenVote() const {
  // The |override_vote| is always chosen first.
  if (override_vote_.has_value())
    return override_vote_.value();
  return default_vote_.value();
}

OverrideVoteAggregator::VoteDataMap::iterator
OverrideVoteAggregator::GetVoteData(const ExecutionContext* execution_context) {
  auto it = vote_data_map_.find(execution_context);
  CHECK(it != vote_data_map_.end(), base::NotFatalUntil::M130);
  return it;
}

OverrideVoteAggregator::VoteData::VoterType
OverrideVoteAggregator::GetVoterType(VoterId voter_id) const {
  DCHECK(voter_id == default_voter_id_ || voter_id == override_voter_id_);
  return voter_id == default_voter_id_ ? VoteData::VoterType::kDefault
                                       : VoteData::VoterType::kOverride;
}

}  // namespace execution_context_priority
}  // namespace performance_manager

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/execution_context_priority/override_vote_aggregator.h"

namespace performance_manager {
namespace execution_context_priority {

OverrideVoteAggregator::OverrideVoteAggregator()
    : vote_consumer_default_impl_(this) {}

OverrideVoteAggregator::~OverrideVoteAggregator() = default;

VotingChannel OverrideVoteAggregator::GetOverrideVotingChannel() {
  DCHECK(vote_data_map_.empty());
  DCHECK_EQ(voting::kInvalidVoterId<Vote>, override_voter_id_);
  DCHECK_GT(2u, vote_consumer_default_impl_.voting_channels_issued());
  auto channel = vote_consumer_default_impl_.BuildVotingChannel();
  override_voter_id_ = channel.voter_id();
  return channel;
}

VotingChannel OverrideVoteAggregator::GetDefaultVotingChannel() {
  DCHECK(vote_data_map_.empty());
  DCHECK_EQ(voting::kInvalidVoterId<Vote>, default_voter_id_);
  DCHECK_GT(2u, vote_consumer_default_impl_.voting_channels_issued());
  auto channel = vote_consumer_default_impl_.BuildVotingChannel();
  default_voter_id_ = channel.voter_id();
  return channel;
}

void OverrideVoteAggregator::SetUpstreamVotingChannel(VotingChannel&& channel) {
  DCHECK(channel.IsValid());
  DCHECK(vote_data_map_.empty());
  DCHECK(!channel_.IsValid());
  channel_ = std::move(channel);
}

bool OverrideVoteAggregator::IsSetup() const {
  return override_voter_id_ != voting::kInvalidVoterId<Vote> &&
         default_voter_id_ != voting::kInvalidVoterId<Vote> &&
         channel_.IsValid();
}

void OverrideVoteAggregator::OnVoteSubmitted(
    VoterId voter_id,
    const ExecutionContext* execution_context,
    const Vote& vote) {
  DCHECK(IsSetup());

  VoteData& vote_data = vote_data_map_[execution_context];
  if (voter_id == override_voter_id_) {
    DCHECK(!vote_data.override_vote.has_value());

    vote_data.override_vote = vote;

    UpstreamVote(execution_context, vote, &vote_data);
  } else {
    DCHECK_EQ(default_voter_id_, voter_id);
    DCHECK(!vote_data.default_vote.has_value());

    vote_data.default_vote = vote;

    if (!vote_data.override_vote.has_value())
      UpstreamVote(execution_context, vote, &vote_data);
  }
}

void OverrideVoteAggregator::OnVoteChanged(
    VoterId voter_id,
    const ExecutionContext* execution_context,
    const Vote& new_vote) {
  VoteData& vote_data = GetVoteData(execution_context)->second;
  if (voter_id == override_voter_id_) {
    DCHECK(vote_data.override_vote.has_value());
    vote_data.override_vote = new_vote;

    UpstreamVote(execution_context, new_vote, &vote_data);
  } else {
    DCHECK_EQ(default_voter_id_, voter_id);
    DCHECK(vote_data.default_vote.has_value());
    vote_data.default_vote = new_vote;

    if (!vote_data.override_vote.has_value())
      UpstreamVote(execution_context, new_vote, &vote_data);
  }
}

void OverrideVoteAggregator::OnVoteInvalidated(
    VoterId voter_id,
    const ExecutionContext* execution_context) {
  auto it = GetVoteData(execution_context);
  VoteData& vote_data = it->second;

  // Figure out which is the "other" vote in this case.
  bool is_override_vote = voter_id == override_voter_id_;
  base::Optional<Vote>* other_vote = nullptr;
  if (is_override_vote) {
    DCHECK(vote_data.override_vote.has_value());
    vote_data.override_vote = base::nullopt;
    other_vote = &vote_data.default_vote;
  } else {
    DCHECK_EQ(default_voter_id_, voter_id);
    DCHECK(vote_data.default_vote.has_value());
    vote_data.default_vote = base::nullopt;
    other_vote = &vote_data.override_vote;
  }

  // If there is no other vote, erase the whole entry. This will cancel the
  // upstream vote as well.
  if (!other_vote->has_value()) {
    vote_data_map_.erase(it);
    return;
  }

  // Otherwise, the other vote is valid. If the other vote is the
  // |override_vote| (ie, the vote being invalidated is not) then the upstream
  // vote doesn't need to change. The last case is that the |override_vote| is
  // being invalidated, and the default is valid. In this case we need to
  // upstream a new vote.
  if (is_override_vote)
    UpstreamVote(execution_context, other_vote->value(), &vote_data);
}

OverrideVoteAggregator::VoteData::VoteData() = default;
OverrideVoteAggregator::VoteData::VoteData(VoteData&& rhs) = default;
OverrideVoteAggregator::VoteData::~VoteData() = default;

OverrideVoteAggregator::VoteDataMap::iterator
OverrideVoteAggregator::GetVoteData(const ExecutionContext* execution_context) {
  auto it = vote_data_map_.find(execution_context);
  DCHECK(it != vote_data_map_.end());
  return it;
}

void OverrideVoteAggregator::UpstreamVote(
    const ExecutionContext* execution_context,
    const Vote& vote,
    VoteData* vote_data) {
  // Change our existing vote, or create a new one as necessary.
  if (vote_data->receipt.HasVote())
    vote_data->receipt.ChangeVote(vote.value(), vote.reason());
  else
    vote_data->receipt = channel_.SubmitVote(execution_context, vote);
}

}  // namespace execution_context_priority
}  // namespace performance_manager

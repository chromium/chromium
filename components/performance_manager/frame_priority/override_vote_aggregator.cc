// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/frame_priority/override_vote_aggregator.h"

namespace performance_manager {
namespace frame_priority {

OverrideVoteAggregator::OverrideVoteAggregator() : factory_(this) {}

OverrideVoteAggregator::~OverrideVoteAggregator() = default;

VotingChannel OverrideVoteAggregator::GetOverrideVotingChannel() {
  DCHECK(vote_data_map_.empty());
  DCHECK_EQ(kInvalidVoterId, override_voter_id_);
  DCHECK_GT(2u, factory_.voting_channels_issued());
  auto channel = factory_.BuildVotingChannel();
  override_voter_id_ = channel.voter_id();
  return channel;
}

VotingChannel OverrideVoteAggregator::GetDefaultVotingChannel() {
  DCHECK(vote_data_map_.empty());
  DCHECK_EQ(kInvalidVoterId, default_voter_id_);
  DCHECK_GT(2u, factory_.voting_channels_issued());
  auto channel = factory_.BuildVotingChannel();
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
  return override_voter_id_ != kInvalidVoterId &&
         default_voter_id_ != kInvalidVoterId && channel_.IsValid();
}

VoteReceipt OverrideVoteAggregator::SubmitVote(VoterId voter_id,
                                               const Vote& vote) {
  DCHECK(vote.IsValid());
  DCHECK(IsSetup());

  VoteData& vote_data = vote_data_map_[vote.frame_node()];
  if (voter_id == override_voter_id_) {
    DCHECK(!vote_data.override_vote.IsValid());
    vote_data.override_vote = AcceptedVote(this, voter_id, vote);
    UpstreamVote(vote, &vote_data);
    return vote_data.override_vote.IssueReceipt();
  } else {
    DCHECK_EQ(default_voter_id_, voter_id);
    DCHECK(!vote_data.default_vote.IsValid());
    vote_data.default_vote = AcceptedVote(this, voter_id, vote);
    if (!vote_data.override_vote.IsValid())
      UpstreamVote(vote, &vote_data);
    return vote_data.default_vote.IssueReceipt();
  }
}

VoteReceipt OverrideVoteAggregator::ChangeVote(VoteReceipt receipt,
                                               AcceptedVote* old_vote,
                                               const Vote& new_vote) {
  DCHECK(receipt.HasVote(old_vote));
  DCHECK(old_vote->IsValid());
  VoteData& vote_data = GetVoteData(old_vote)->second;

  // Update the vote in place.
  old_vote->UpdateVote(new_vote);

  // The vote being changed is the upstream vote if:
  // (1) It is the override vote, or
  // (2) There is no override vote.
  if (old_vote == &vote_data.override_vote ||
      !vote_data.override_vote.IsValid()) {
    UpstreamVote(new_vote, &vote_data);
  }

  // Pass the same receipt right back to the user.
  return receipt;
}

void OverrideVoteAggregator::VoteInvalidated(AcceptedVote* vote) {
  DCHECK(!vote->IsValid());
  auto it = GetVoteData(vote);
  VoteData& vote_data = it->second;

  // Figure out which is the "other" vote in this case.
  bool is_override_vote = false;
  AcceptedVote* other = nullptr;
  if (vote == &vote_data.override_vote) {
    is_override_vote = true;
    other = &vote_data.default_vote;
  } else {
    DCHECK_EQ(&vote_data.default_vote, vote);
    other = &vote_data.override_vote;
  }

  // If the other vote is invalid the whole entry is being erased, which will
  // cancel the upstream vote as well.
  if (!other->IsValid()) {
    vote_data_map_.erase(it);
    return;
  }

  // Otherwise, the other vote is valid. If the other vote is the
  // |override_vote| (ie, the vote being invalidated is not) then the upstream
  // vote doesn't need to change. The last case is that the |override_vote| is
  // being invalidated, and the default is valid. In this case we need to
  // upstream a new vote.
  if (is_override_vote)
    UpstreamVote(other->vote(), &vote_data);
}

OverrideVoteAggregator::VoteDataMap::iterator
OverrideVoteAggregator::GetVoteData(AcceptedVote* vote) {
  // The vote being retrieved should have us as its consumer, and have been
  // emitted by one of our known voters.
  DCHECK(vote);
  DCHECK_EQ(this, vote->consumer());
  DCHECK(vote->voter_id() == override_voter_id_ ||
         vote->voter_id() == default_voter_id_);
  DCHECK(IsSetup());

  auto it = vote_data_map_.find(vote->vote().frame_node());
  DCHECK(it != vote_data_map_.end());
  return it;
}

void OverrideVoteAggregator::UpstreamVote(const Vote& vote,
                                          VoteData* vote_data) {
  // Change our existing vote, or create a new one as necessary.
  if (vote_data->receipt.HasVote())
    vote_data->receipt.ChangeVote(vote.priority(), vote.reason());
  else
    vote_data->receipt = channel_.SubmitVote(vote);
}

}  // namespace frame_priority
}  // namespace performance_manager

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/frame_priority/max_vote_aggregator.h"

#include <algorithm>
#include <tuple>

namespace performance_manager {
namespace frame_priority {

MaxVoteAggregator::MaxVoteAggregator() : factory_(this) {}

MaxVoteAggregator::~MaxVoteAggregator() = default;

VotingChannel MaxVoteAggregator::GetVotingChannel() {
  return factory_.BuildVotingChannel();
}

void MaxVoteAggregator::SetUpstreamVotingChannel(VotingChannel&& channel) {
  DCHECK(channel.IsValid());
  DCHECK(vote_data_map_.empty());
  DCHECK(!channel_.IsValid());
  channel_ = std::move(channel);
}

VoteReceipt MaxVoteAggregator::SubmitVote(VoterId voter_id, const Vote& vote) {
  DCHECK(vote.IsValid());
  DCHECK(channel_.IsValid());

  // NOTE: We don't currently explicitly worry about having multiple votes for
  // the same frame from a single voter, although such logic could be added.

  // Add the new vote.
  VoteData& vote_data = vote_data_map_[vote.frame_node()];
  auto accepted_vote = AcceptedVote(this, voter_id, vote);
  auto receipt = accepted_vote.IssueReceipt();
  if (vote_data.AddVote(std::move(accepted_vote), next_vote_id_++))
    vote_data.UpstreamVote(&channel_);

  // Finally, return a vote receipt to our voter for the received vote.
  return receipt;
}

VoteReceipt MaxVoteAggregator::ChangeVote(VoteReceipt receipt,
                                          AcceptedVote* old_vote,
                                          const Vote& new_vote) {
  DCHECK(receipt.HasVote(old_vote));
  DCHECK(old_vote->IsValid());
  VoteData& vote_data = GetVoteData(old_vote)->second;
  size_t index = vote_data.GetVoteIndex(old_vote);

  // Update the vote directly, then repair the heap.
  old_vote->UpdateVote(new_vote);
  if (vote_data.UpdateVote(index, next_vote_id_++))
    vote_data.UpstreamVote(&channel_);

  // Return the same receipt back to the user.
  return receipt;
}

void MaxVoteAggregator::VoteInvalidated(AcceptedVote* vote) {
  DCHECK(!vote->IsValid());
  auto it = GetVoteData(vote);
  VoteData& vote_data = it->second;
  size_t index = vote_data.GetVoteIndex(vote);

  // Remove the vote, and upstream if necessary.
  if (vote_data.RemoveVote(index))
    vote_data.UpstreamVote(&channel_);

  // If all the votes for this frame have disappeared then remove the entry
  // entirely. This will automatically cancel our upstream vote.
  if (vote_data.IsEmpty())
    vote_data_map_.erase(it);
}

MaxVoteAggregator::VoteDataMap::iterator MaxVoteAggregator::GetVoteData(
    AcceptedVote* vote) {
  // The vote being retrieved should have us as its consumer, and we should
  // already have been setup to receive votes before this is called.
  DCHECK(vote);
  DCHECK_EQ(this, vote->consumer());
  DCHECK(channel_.IsValid());

  // Find the votes associated with this frame.
  auto it = vote_data_map_.find(vote->vote().frame_node());
  DCHECK(it != vote_data_map_.end());
  return it;
}

MaxVoteAggregator::VoteData::VoteData() = default;

MaxVoteAggregator::VoteData::VoteData(VoteData&& rhs) = default;

MaxVoteAggregator::VoteData& MaxVoteAggregator::VoteData::operator=(
    VoteData&& rhs) = default;

MaxVoteAggregator::VoteData::~VoteData() = default;

bool MaxVoteAggregator::VoteData::AddVote(AcceptedVote&& vote,
                                          uint32_t vote_id) {
  DCHECK(vote.IsValid());
  votes_.emplace(std::move(vote), vote_id);
  return true;
}

bool MaxVoteAggregator::VoteData::UpdateVote(size_t index, uint32_t vote_id) {
  DCHECK_LE(0u, index);
  DCHECK_LT(index, votes_.size());
  DCHECK(votes_[index].vote.IsValid());

  // Remember the upstream vote as it may change.
  const Vote old_root = votes_.top().vote.vote();

  // The AcceptedVote has actually already been changed in place. Remove the
  // vote, finish the update, and reinsert it into the heap.
  votes_.Update(index);

  // The vote always needs to be upstreamed if the changed vote was at the root.
  // Otherwise, we only need to upstream if the root vote was observed to change
  // as part of the heap repair.
  const Vote& new_root = votes_.top().vote.vote();
  return index == 0 || old_root != new_root;
}

bool MaxVoteAggregator::VoteData::RemoveVote(size_t index) {
  DCHECK_LE(0u, index);
  DCHECK_LT(index, votes_.size());
  DCHECK(!votes_[index].vote.IsValid());

  votes_.erase(index);

  // A new upstream vote needs to be made if the root was disturbed.
  return votes_.size() && index == 0;
}

size_t MaxVoteAggregator::VoteData::GetVoteIndex(AcceptedVote* vote) {
  static_assert(offsetof(StampedVote, vote) == 0,
                "AcceptedVote is expected to be at offset 0 of StampedVote");
  StampedVote* stamped_vote = reinterpret_cast<StampedVote*>(vote);
  DCHECK_NE(0u, votes_.size());
  DCHECK_LE(votes_.data(), stamped_vote);
  DCHECK_LT(stamped_vote, votes_.data() + votes_.size());
  return stamped_vote - votes_.data();
}

void MaxVoteAggregator::VoteData::UpstreamVote(VotingChannel* channel) {
  DCHECK_NE(0u, votes_.size());
  DCHECK(votes_.top().vote.IsValid());
  const Vote& vote = votes_.top().vote.vote();

  // Change our existing vote, or create a new one as necessary.
  if (receipt_.HasVote()) {
    receipt_.ChangeVote(vote.priority(), vote.reason());
  } else {
    receipt_ = channel->SubmitVote(vote);
  }
}

}  // namespace frame_priority
}  // namespace performance_manager

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/execution_context_priority/max_vote_aggregator.h"

#include <algorithm>
#include <tuple>

namespace performance_manager {
namespace execution_context_priority {

MaxVoteAggregator::MaxVoteAggregator()
    : vote_consumer_default_impl_(this), next_vote_id_(0) {}

MaxVoteAggregator::~MaxVoteAggregator() = default;

VotingChannel MaxVoteAggregator::GetVotingChannel() {
  return vote_consumer_default_impl_.BuildVotingChannel();
}

void MaxVoteAggregator::SetUpstreamVotingChannel(VotingChannel&& channel) {
  DCHECK(channel.IsValid());
  DCHECK(vote_data_map_.empty());
  DCHECK(!channel_.IsValid());
  channel_ = std::move(channel);
}

void MaxVoteAggregator::OnVoteSubmitted(
    VoterId voter_id,
    const ExecutionContext* execution_context,
    const Vote& vote) {
  DCHECK(channel_.IsValid());

  // Add the new vote.
  VoteData& vote_data = vote_data_map_[execution_context];
  if (vote_data.AddVote(voter_id, vote, next_vote_id_++))
    vote_data.UpstreamVote(execution_context, &channel_);
}

void MaxVoteAggregator::OnVoteChanged(VoterId voter_id,
                                      const ExecutionContext* execution_context,
                                      const Vote& new_vote) {
  VoteData& vote_data = GetVoteData(execution_context)->second;

  if (vote_data.UpdateVote(voter_id, new_vote))
    vote_data.UpstreamVote(execution_context, &channel_);
}

void MaxVoteAggregator::OnVoteInvalidated(
    VoterId voter_id,
    const ExecutionContext* execution_context) {
  auto it = GetVoteData(execution_context);
  VoteData& vote_data = it->second;

  // Remove the vote, and upstream if necessary.
  if (vote_data.RemoveVote(voter_id))
    vote_data.UpstreamVote(execution_context, &channel_);

  // If all the votes for this execution context have disappeared then remove
  // the entry entirely. This will automatically cancel our upstream vote.
  if (vote_data.IsEmpty())
    vote_data_map_.erase(it);
}

MaxVoteAggregator::StampedVote::StampedVote() = default;
MaxVoteAggregator::StampedVote::StampedVote(const Vote& vote, uint32_t vote_id)
    : vote_(vote), vote_id_(vote_id) {}
MaxVoteAggregator::StampedVote::StampedVote(StampedVote&&) = default;
MaxVoteAggregator::StampedVote::~StampedVote() = default;

MaxVoteAggregator::VoteDataMap::iterator MaxVoteAggregator::GetVoteData(
    const ExecutionContext* execution_context) {
  auto it = vote_data_map_.find(execution_context);
  DCHECK(it != vote_data_map_.end());
  return it;
}

MaxVoteAggregator::VoteData::VoteData() = default;

MaxVoteAggregator::VoteData::VoteData(VoteData&& rhs) = default;

MaxVoteAggregator::VoteData& MaxVoteAggregator::VoteData::operator=(
    VoteData&& rhs) = default;

MaxVoteAggregator::VoteData::~VoteData() = default;

bool MaxVoteAggregator::VoteData::AddVote(VoterId voter_id,
                                          const Vote& vote,
                                          uint32_t vote_id) {
  // Remember the upstream vote as it may change. There could be none.
  base::Optional<Vote> old_root;
  if (!votes_.empty())
    old_root = votes_.top().vote();

  auto it = votes_.emplace(vote, vote_id);

  bool inserted = heap_handles_.emplace(voter_id, it->handle()).second;
  DCHECK(inserted);

  // There was no previous root. This vote must be upstreamed.
  if (!old_root)
    return true;

  // The vote always needs to be upstreamed if the root vote changed.
  const Vote& new_root = votes_.top().vote();
  return old_root.value() != new_root;
}

bool MaxVoteAggregator::VoteData::UpdateVote(VoterId voter_id,
                                             const Vote& new_vote) {
  // Remember the upstream vote as it may change.
  const Vote old_root = votes_.top().vote();

  auto it = heap_handles_.find(voter_id);
  DCHECK(it != heap_handles_.end());
  base::HeapHandle* heap_handle = it->second;

  votes_.Modify(*heap_handle, [&new_vote](StampedVote& element) {
    element.SetVote(new_vote);
  });

  // The vote always needs to be upstreamed if the root vote changed.
  const Vote& new_root = votes_.top().vote();
  return old_root != new_root;
}

bool MaxVoteAggregator::VoteData::RemoveVote(VoterId voter_id) {
  // Remember the upstream vote as it may change.
  const Vote old_root = votes_.top().vote();

  auto it = heap_handles_.find(voter_id);
  DCHECK(it != heap_handles_.end());
  base::HeapHandle* heap_handle = it->second;
  heap_handles_.erase(it);

  votes_.erase(*heap_handle);

  // If |votes_| is now empty, the upstream vote needs to be invalidated instead
  // of upstreaming a new vote.
  if (votes_.empty())
    return false;

  const Vote& new_root = votes_.top().vote();
  return old_root != new_root;
}

void MaxVoteAggregator::VoteData::UpstreamVote(
    const ExecutionContext* execution_context,
    VotingChannel* channel) {
  DCHECK(!votes_.empty());
  const Vote& vote = votes_.top().vote();

  // Change our existing vote, or create a new one as necessary.
  if (receipt_.HasVote()) {
    receipt_.ChangeVote(vote.value(), vote.reason());
  } else {
    receipt_ = channel->SubmitVote(execution_context, vote);
  }
}

}  // namespace execution_context_priority
}  // namespace performance_manager

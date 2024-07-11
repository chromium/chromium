// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/max_vote_aggregator.h"

#include <algorithm>
#include <tuple>

#include "base/not_fatal_until.h"

namespace performance_manager {
namespace execution_context_priority {

MaxVoteAggregator::MaxVoteAggregator() = default;

MaxVoteAggregator::~MaxVoteAggregator() = default;

VotingChannel MaxVoteAggregator::GetVotingChannel() {
  return voting_channel_factory_.BuildVotingChannel();
}

void MaxVoteAggregator::SetUpstreamVotingChannel(VotingChannel channel) {
  channel_ = std::move(channel);
}

void MaxVoteAggregator::OnVoteSubmitted(
    VoterId voter_id,
    const ExecutionContext* execution_context,
    const Vote& vote) {
  DCHECK(channel_.IsValid());

  // Create the VoteData for this execution context, if necessary.
  VoteData& vote_data = vote_data_map_[execution_context];

  // Remember the previous top vote before adding the new vote. There could be
  // none if this is the first vote submitted for |execution_context|.
  std::optional<Vote> old_top_vote;
  if (!vote_data.IsEmpty())
    old_top_vote = vote_data.GetTopVote();

  vote_data.AddVote(voter_id, vote, next_vote_id_++);

  // If there was no previous top vote, the vote must be submitted.
  if (!old_top_vote) {
    channel_.SubmitVote(execution_context, vote);
    return;
  }

  // Since there is a previous top vote, it must be modified if the top vote
  // changed.
  const Vote new_top_vote = vote_data.GetTopVote();
  if (old_top_vote.value() != new_top_vote)
    channel_.ChangeVote(execution_context, new_top_vote);
}

void MaxVoteAggregator::OnVoteChanged(VoterId voter_id,
                                      const ExecutionContext* execution_context,
                                      const Vote& new_vote) {
  // The VoteData for this execution context is guaranteed to exist.
  VoteData& vote_data = GetVoteData(execution_context)->second;

  // Remember the previous top vote before updating the vote for this
  // |voter_id|.
  const Vote old_top_vote = vote_data.GetTopVote();

  vote_data.UpdateVote(voter_id, new_vote);

  // If the top vote changed, the upstream vote must also be changed.
  const Vote new_top_vote = vote_data.GetTopVote();
  if (old_top_vote != new_top_vote)
    channel_.ChangeVote(execution_context, new_top_vote);
}

void MaxVoteAggregator::OnVoteInvalidated(
    VoterId voter_id,
    const ExecutionContext* execution_context) {
  // The VoteData for this execution context is guaranteed to exist.
  auto it = GetVoteData(execution_context);
  VoteData& vote_data = it->second;

  // Remember the previous top vote before removing the vote for this
  // |voter_id|.
  const Vote old_top_vote = vote_data.GetTopVote();

  vote_data.RemoveVote(voter_id);

  // In case the last vote for |execution_context| was invalidated, the upstream
  // vote must also be invalidated.
  if (vote_data.IsEmpty()) {
    channel_.InvalidateVote(execution_context);

    // Clean up the VoteData for |execution_context| since it is empty.
    vote_data_map_.erase(it);
    return;
  }

  // If the top vote changed, the upstream vote must also be changed.
  const Vote new_top_vote = vote_data.GetTopVote();
  if (old_top_vote != new_top_vote)
    channel_.ChangeVote(execution_context, new_top_vote);
}

MaxVoteAggregator::StampedVote::StampedVote() = default;
MaxVoteAggregator::StampedVote::StampedVote(const Vote& vote, uint32_t vote_id)
    : vote_(vote), vote_id_(vote_id) {}
MaxVoteAggregator::StampedVote::StampedVote(StampedVote&&) = default;
MaxVoteAggregator::StampedVote::~StampedVote() = default;

MaxVoteAggregator::VoteDataMap::iterator MaxVoteAggregator::GetVoteData(
    const ExecutionContext* execution_context) {
  auto it = vote_data_map_.find(execution_context);
  CHECK(it != vote_data_map_.end(), base::NotFatalUntil::M130);
  return it;
}

MaxVoteAggregator::VoteData::VoteData() = default;

MaxVoteAggregator::VoteData::VoteData(VoteData&& rhs) = default;

MaxVoteAggregator::VoteData& MaxVoteAggregator::VoteData::operator=(
    VoteData&& rhs) = default;

MaxVoteAggregator::VoteData::~VoteData() = default;

void MaxVoteAggregator::VoteData::AddVote(VoterId voter_id,
                                          const Vote& vote,
                                          uint32_t vote_id) {
  auto it = votes_.emplace(vote, vote_id);

  bool inserted = heap_handles_.emplace(voter_id, it->handle()).second;
  DCHECK(inserted);
}

void MaxVoteAggregator::VoteData::UpdateVote(VoterId voter_id,
                                             const Vote& new_vote) {
  auto it = heap_handles_.find(voter_id);
  CHECK(it != heap_handles_.end(), base::NotFatalUntil::M130);
  base::HeapHandle* heap_handle = it->second;

  votes_.Modify(*heap_handle, [&new_vote](StampedVote& element) {
    element.SetVote(new_vote);
  });
}

void MaxVoteAggregator::VoteData::RemoveVote(VoterId voter_id) {
  auto it = heap_handles_.find(voter_id);
  CHECK(it != heap_handles_.end(), base::NotFatalUntil::M130);
  base::HeapHandle* heap_handle = it->second;
  heap_handles_.erase(it);

  votes_.erase(*heap_handle);
}

const Vote& MaxVoteAggregator::VoteData::GetTopVote() const {
  return votes_.top().vote();
}

}  // namespace execution_context_priority
}  // namespace performance_manager

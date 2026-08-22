// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/execution_context_priority/max_vote_aggregator.h"

#include <algorithm>
#include <tuple>


namespace performance_manager {
namespace execution_context_priority {

MaxVoteAggregator::MaxVoteAggregator() = default;

MaxVoteAggregator::~MaxVoteAggregator() = default;

VotingChannel MaxVoteAggregator::GetVotingChannel() {
  return voting_channel_factory_.BuildVotingChannel();
}

void MaxVoteAggregator::SetUpstreamVotingChannel(VotingChannel channel) {
  CHECK(!channel_.IsValid());
  CHECK(channel.IsValid());
  channel_ = std::move(channel);
}

void MaxVoteAggregator::ResetUpstreamVotingChannel() {
  CHECK(channel_.IsValid());
  channel_.Reset();
}

void MaxVoteAggregator::OnVoteSet(VoterId voter_id,
                                  const ExecutionContext* execution_context,
                                  const std::optional<Vote>& vote) {
  if (!vote.has_value()) {
    // Vote removal.
    auto it = vote_data_map_.find(execution_context);
    if (it == vote_data_map_.end() || !it->second.HasVote(voter_id)) {
      return;
    }

    VoteData& vote_data = it->second;

    const std::optional<Vote> old_top_vote = vote_data.GetTopVote();
    vote_data.RemoveVote(voter_id);
    const std::optional<Vote> new_top_vote = vote_data.GetTopVote();

    if (old_top_vote != new_top_vote) {
      channel_.SetVote(execution_context, new_top_vote);
    }
    if (!new_top_vote.has_value()) {
      vote_data_map_.erase(it);
    }
    return;
  }

  // Vote addition or modification.
  auto [it, _] = vote_data_map_.try_emplace(execution_context);
  VoteData& vote_data = it->second;

  const std::optional<Vote> old_top_vote = vote_data.GetTopVote();
  if (!vote_data.SetVote(voter_id, *vote, next_vote_id_++)) {
    return;
  }
  const std::optional<Vote> new_top_vote = vote_data.GetTopVote();

  if (old_top_vote != new_top_vote) {
    channel_.SetVote(execution_context, new_top_vote);
  }
}

MaxVoteAggregator::StampedVote::StampedVote() = default;
MaxVoteAggregator::StampedVote::StampedVote(const Vote& vote, uint32_t vote_id)
    : vote_(vote), vote_id_(vote_id) {}
MaxVoteAggregator::StampedVote::StampedVote(StampedVote&&) = default;
MaxVoteAggregator::StampedVote::~StampedVote() = default;

MaxVoteAggregator::VoteData::VoteData() = default;

MaxVoteAggregator::VoteData::VoteData(VoteData&& rhs) = default;

MaxVoteAggregator::VoteData& MaxVoteAggregator::VoteData::operator=(
    VoteData&& rhs) = default;

MaxVoteAggregator::VoteData::~VoteData() = default;

void MaxVoteAggregator::VoteData::RemoveVote(VoterId voter_id) {
  auto it = heap_handles_.find(voter_id);
  CHECK(it != heap_handles_.end());
  base::HeapHandle* heap_handle = it->second;
  heap_handles_.erase(it);

  votes_.erase(*heap_handle);
}

bool MaxVoteAggregator::VoteData::SetVote(VoterId voter_id,
                                          const Vote& vote,
                                          uint32_t vote_id) {
  auto [it, inserted] = heap_handles_.emplace(voter_id, nullptr);
  if (inserted) {
    auto votes_it = votes_.emplace(vote, vote_id);
    it->second = votes_it->handle();
    return true;
  }

  base::HeapHandle* heap_handle = it->second;
  if (votes_.at(*heap_handle).vote() == vote) {
    return false;
  }

  votes_.Modify(*heap_handle,
                [&vote](StampedVote& element) { element.SetVote(vote); });
  return true;
}

bool MaxVoteAggregator::VoteData::HasVote(VoterId voter_id) const {
  return heap_handles_.contains(voter_id);
}

std::optional<Vote> MaxVoteAggregator::VoteData::GetTopVote() const {
  if (votes_.empty()) {
    return std::nullopt;
  }
  return votes_.top().vote();
}

}  // namespace execution_context_priority
}  // namespace performance_manager

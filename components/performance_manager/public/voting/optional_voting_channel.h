// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VOTING_OPTIONAL_VOTING_CHANNEL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VOTING_OPTIONAL_VOTING_CHANNEL_H_

#include <map>
#include <optional>
#include <utility>

#include "base/check.h"
#include "components/performance_manager/public/voting/voting.h"

namespace performance_manager::voting {

// A voting channel that accepts std::nullopt votes. Those votes are not
// upstreamed. This voting channel is always valid unless moved from.
template <class VoteImpl>
class OptionalVotingChannel {
 public:
  using ContextType = typename VoteImpl::ContextType;
  using VoterId = voting::VoterId<VoteImpl>;
  using VotingChannel = voting::VotingChannel<VoteImpl>;
  using VoteType = VoteImpl::VoteType;

  explicit OptionalVotingChannel(VotingChannel upstream_voting_channel);
  ~OptionalVotingChannel();

  OptionalVotingChannel(const OptionalVotingChannel&) = delete;
  OptionalVotingChannel& operator=(const OptionalVotingChannel&) = delete;

  OptionalVotingChannel(OptionalVotingChannel&&);
  OptionalVotingChannel& operator=(OptionalVotingChannel&&);

  // Submits a vote through this voting channel that is only upstreamed if it is
  // not nullopt. Can only be called if this VotingChannel is valid.
  void SubmitVote(const ContextType* context,
                  const std::optional<VoteImpl>& vote);

  // Modifies an existing vote. This can either submit, change or invalidate the
  // vote to the upstream voting channel depending on the optional value of the
  // previous vote and the new vote. Can only be called if this VotingChannel is
  // valid.
  void ChangeVote(const ContextType* context,
                  const std::optional<VoteImpl>& vote);

  // Invalidates an existing vote. Can only be called if this VotingChannel is
  // valid.
  void InvalidateVote(const ContextType* context);

  VoterId voter_id() const { return upstream_voting_channel_.voter_id(); }

 private:
  VotingChannel upstream_voting_channel_;

  std::map<const ContextType*, std::optional<VoteImpl>> votes_;
};

template <class VoteImpl>
OptionalVotingChannel<VoteImpl>::OptionalVotingChannel(
    VotingChannel upstream_voting_channel)
    : upstream_voting_channel_(std::move(upstream_voting_channel)) {
  CHECK(upstream_voting_channel_.IsValid());
}

template <class VoteImpl>
OptionalVotingChannel<VoteImpl>::~OptionalVotingChannel() {
  CHECK(votes_.empty());
}

template <class VoteImpl>
OptionalVotingChannel<VoteImpl>::OptionalVotingChannel(
    OptionalVotingChannel&& other) = default;

template <class VoteImpl>
OptionalVotingChannel<VoteImpl>& OptionalVotingChannel<VoteImpl>::operator=(
    OptionalVotingChannel&& other) = default;

template <class VoteImpl>
void OptionalVotingChannel<VoteImpl>::SubmitVote(
    const ContextType* context,
    const std::optional<VoteImpl>& vote) {
  const auto [_, inserted] = votes_.try_emplace(context, vote);
  CHECK(inserted);

  if (!vote.has_value()) {
    return;
  }

  upstream_voting_channel_.SubmitVote(context, *vote);
}

template <class VoteImpl>
void OptionalVotingChannel<VoteImpl>::ChangeVote(
    const ContextType* context,
    const std::optional<VoteImpl>& vote) {
  auto it = votes_.find(context);
  CHECK(it != votes_.end());

  std::optional<VoteImpl> old_vote = it->second;

  // Nothing to do if the vote did not change.
  if (old_vote == vote) {
    return;
  }

  it->second = vote;

  if (old_vote.has_value() && vote.has_value()) {
    upstream_voting_channel_.ChangeVote(context, *vote);
  } else if (old_vote.has_value()) {
    upstream_voting_channel_.InvalidateVote(context);
  } else if (vote.has_value()) {
    upstream_voting_channel_.SubmitVote(context, *vote);
  }
}

template <class VoteImpl>
void OptionalVotingChannel<VoteImpl>::InvalidateVote(
    const ContextType* context) {
  auto it = votes_.find(context);
  CHECK(it != votes_.end());
  const bool had_value = it->second.has_value();
  votes_.erase(it);

  if (had_value) {
    upstream_voting_channel_.InvalidateVote(context);
  }
}

}  // namespace performance_manager::voting

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VOTING_OPTIONAL_VOTING_CHANNEL_H_

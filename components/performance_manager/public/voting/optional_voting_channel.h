// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VOTING_OPTIONAL_VOTING_CHANNEL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VOTING_OPTIONAL_VOTING_CHANNEL_H_

#include <optional>
#include <utility>

#include "base/check.h"
#include "components/performance_manager/public/voting/voting.h"

namespace performance_manager::voting {

// A thin wrapper around VotingChannel for backwards compatibility.
// VotingChannel natively accepts std::nullopt votes via SetVote().
template <class VoteImpl>
class OptionalVotingChannel {
 public:
  using ContextType = typename VoteImpl::ContextType;
  using VoterId = voting::VoterId<VoteImpl>;
  using VotingChannel = voting::VotingChannel<VoteImpl>;
  using VoteType = VoteImpl::VoteType;

  OptionalVotingChannel();
  explicit OptionalVotingChannel(VotingChannel upstream_voting_channel);
  ~OptionalVotingChannel();

  OptionalVotingChannel(const OptionalVotingChannel&) = delete;
  OptionalVotingChannel& operator=(const OptionalVotingChannel&) = delete;

  OptionalVotingChannel(OptionalVotingChannel&&);
  OptionalVotingChannel& operator=(OptionalVotingChannel&&);

  void SubmitVote(const ContextType* context,
                  const std::optional<VoteImpl>& vote);
  void ChangeVote(const ContextType* context,
                  const std::optional<VoteImpl>& vote);
  void InvalidateVote(const ContextType* context);
  void SetVote(const ContextType* context, const std::optional<VoteImpl>& vote);

  // Returns true if this VotingChannel is valid.
  bool IsValid() const;

  // Resets this voting channel.
  void Reset();

  VoterId voter_id() const { return upstream_voting_channel_.voter_id(); }

 private:
  VotingChannel upstream_voting_channel_;
};

template <class VoteImpl>
OptionalVotingChannel<VoteImpl>::OptionalVotingChannel() = default;

template <class VoteImpl>
OptionalVotingChannel<VoteImpl>::OptionalVotingChannel(
    VotingChannel upstream_voting_channel)
    : upstream_voting_channel_(std::move(upstream_voting_channel)) {
  CHECK(upstream_voting_channel_.IsValid());
}

template <class VoteImpl>
OptionalVotingChannel<VoteImpl>::~OptionalVotingChannel() = default;

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
  if (vote.has_value()) {
    upstream_voting_channel_.SetVote(context, *vote);
  }
}

template <class VoteImpl>
void OptionalVotingChannel<VoteImpl>::ChangeVote(
    const ContextType* context,
    const std::optional<VoteImpl>& vote) {
  upstream_voting_channel_.SetVote(context, vote);
}

template <class VoteImpl>
void OptionalVotingChannel<VoteImpl>::InvalidateVote(
    const ContextType* context) {
  upstream_voting_channel_.SetVote(context, std::nullopt);
}

template <class VoteImpl>
void OptionalVotingChannel<VoteImpl>::SetVote(
    const ContextType* context,
    const std::optional<VoteImpl>& vote) {
  upstream_voting_channel_.SetVote(context, vote);
}

template <class VoteImpl>
bool OptionalVotingChannel<VoteImpl>::IsValid() const {
  return upstream_voting_channel_.IsValid();
}

template <class VoteImpl>
void OptionalVotingChannel<VoteImpl>::Reset() {
  upstream_voting_channel_.Reset();
}

}  // namespace performance_manager::voting

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VOTING_OPTIONAL_VOTING_CHANNEL_H_

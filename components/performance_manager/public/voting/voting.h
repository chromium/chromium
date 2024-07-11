// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VOTING_VOTING_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VOTING_VOTING_H_

// Declares the various structures and formats associated with a templated
// voting system. This is templated on a vote type (e.g. a priority) and a vote
// context (e.g. a specific node type).
//
// There are 4 interrelated classes declared here:
//
// (1) Vote - A simple wrapper for a vote. This is a final concrete class.
// (2) VoteObserver - Destination for Votes. This is an interface.
// (3) VotingChannel - A mechanism by which a voter can submit votes to a
//     VoteObserver. this is a final concrete class.
// (4) VotingChannelFactory - Producer and tracker of VotingChannels, meant to
//     be owned by a VoteObserver, so it can issue VotingChannels associated
//     with the VoteObserver. This is a final concrete class.
//
// Voters register themselves with VoteObservers, which issues them a private
// VotingChannel using their VotingChannelFactory. Voters can then use their
// VotingChannel to submit new votes (SubmitVote()), change an existing vote
// (ChangeVote()) or invalidate an existing vote (InvalidateVote()).
//
// All votes submitted through a VotingChannel must be invalidated before the
// channel is destroyed, and all VotingChannels issued by a
// VotingChannelFactory must be destroyed before the factory is destroyed. This
// is all verified via debug checks.
//
// The VoteObserver will receive a notification that is tagged with the ID of
// originating VotingChannel every time a vote is submitted (OnVoteSubmitted()),
// changed (OnVoteChanged()) or invalidated (OnVoteInvalidated()).
//
// None of these objects are thread-safe, and they should all be used from a
// single sequence. In practice this will be the PM sequence.

#include <cstring>
#include <map>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/types/id_type.h"
#include "base/types/pass_key.h"

namespace performance_manager {
namespace voting {

// Contains a single vote. Specifically allows copying, etc, so as to be STL
// container friendly.
template <typename TContextType, typename TVoteType, TVoteType DefaultVote>
class Vote final {
 public:
  using ContextType = TContextType;
  using VoteType = TVoteType;

  Vote();
  // NOTE: |reason| *must* be a static string.
  Vote(VoteType vote, const char* reason);
  Vote(const Vote& rhs);

  Vote& operator=(const Vote& rhs);

  ~Vote();

  VoteType value() const { return vote_; }
  const char* reason() const { return reason_; }

  bool operator==(const Vote& vote) const;
  bool operator!=(const Vote& vote) const;

  // Returns true if the vote is valid. A valid vote must have a |reason_|.
  bool IsValid() const;

 private:
  VoteType vote_ = DefaultVote;
  const char* reason_ = nullptr;
};

// Identifies a VotingChannel.
template <typename VoteImpl>
using VoterId = base::IdTypeU32<VoteImpl>;

template <class VoteImpl>
class VoteObserver {
 public:
  using ContextType = typename VoteImpl::ContextType;

  virtual ~VoteObserver();

  // Invoked when a |vote| is submitted for |context|. |voter_id| identifies the
  // voting channel.
  virtual void OnVoteSubmitted(VoterId<VoteImpl> voter_id,
                               const ContextType* context,
                               const VoteImpl& vote) = 0;

  // Invoked when the vote for |context| is changed to |new_vote|. |voter_id|
  // identifies the voting channel.
  virtual void OnVoteChanged(VoterId<VoteImpl> voter_id,
                             const ContextType* context,
                             const VoteImpl& new_vote) = 0;

  // Invoked when a vote for |context| is invalided. |voter_id| identifies the
  // voting channel.
  virtual void OnVoteInvalidated(VoterId<VoteImpl> voter_id,
                                 const ContextType* context) = 0;
};

template <class VoteImpl>
class VotingChannelFactory;

// A channel that a voter can use to submit votes to a VoteObserver. A move-only
// type so that it can't be shared by multiple voters. This must be destroyed
// before the issuing VotingChannelFactory.
template <class VoteImpl>
class VotingChannel {
 public:
  using ContextType = typename VoteImpl::ContextType;
  using PassKey = base::PassKey<VotingChannel>;

  VotingChannel();
  VotingChannel(const VotingChannel& rhs) = delete;
  VotingChannel(VotingChannel&& rhs);
  VotingChannel& operator=(const VotingChannel& rhs) = delete;
  VotingChannel& operator=(VotingChannel&& rhs);
  ~VotingChannel();

  // Submits a vote through this voting channel. Can only be called if this
  // VotingChannel is valid.
  void SubmitVote(const ContextType* context, const VoteImpl& vote);

  // Modifies an existing vote. Can only be called if this VotingChannel is
  // valid.
  void ChangeVote(const ContextType* context, const VoteImpl& new_vote);

  // Invalidates an existing vote. Can only be called if this VotingChannel is
  // valid.
  void InvalidateVote(const ContextType* context);

  // Returns true if this VotingChannel is valid.
  bool IsValid() const;

  // Resets this voting channel.
  void Reset();

  VoterId<VoteImpl> voter_id() const { return voter_id_; }

  // VotingChannelFactory is the sole producer of VotingChannels.
  VotingChannel(base::PassKey<VotingChannelFactory<VoteImpl>>,
                VotingChannelFactory<VoteImpl>* factory,
                VoterId<VoteImpl> voter_id);

 private:
  void Take(VotingChannel&& rhs);

  // Used to reach back into the factory to decrement the outstanding
  // VotingChannel count, and for routing votes to the consumer.
  raw_ptr<VotingChannelFactory<VoteImpl>> factory_ = nullptr;
  VoterId<VoteImpl> voter_id_;

#if DCHECK_IS_ON()
  base::flat_map<const ContextType*, VoteImpl> votes_;
#endif  // DCHECK_IS_ON()
};

// A helper for creating VotingChannels that binds a unique VoterId (and
// passes the votes along to the VoteObserver with that VoterId), and a tracking
// token to ensure that the voter disconnects from the VoteObserver before it is
// itself destroyed. Implementations of VoteObservers should own an instance of
// this and use it to emit VotingChannels. This class will DCHECK in its
// destructor if there are outstanding VotingChannels at its death.
template <class VoteImpl>
class VotingChannelFactory final {
 public:
  using PassKey = base::PassKey<VotingChannelFactory>;

  explicit VotingChannelFactory(VoteObserver<VoteImpl>* observer);
  ~VotingChannelFactory();
  VotingChannelFactory(const VotingChannelFactory& rhs) = delete;
  VotingChannelFactory& operator=(const VotingChannelFactory& rhs) = delete;

  // Builds a new VotingChannel that routes votes to the |observer_|.
  VotingChannel<VoteImpl> BuildVotingChannel();

  size_t voting_channels_issued() const { return voting_channels_issued_; }
  size_t voting_channels_outstanding() const {
    return voting_channels_outstanding_;
  }

  // Used by ~VotingChannel to notify the factory that a channel has been
  // torn down.
  void OnVotingChannelDestroyed(base::PassKey<VotingChannel<VoteImpl>>);

  VoteObserver<VoteImpl>* GetObserver(base::PassKey<VotingChannel<VoteImpl>>) {
    return observer_;
  }

 private:
  // The consumer that owns this factory.
  raw_ptr<VoteObserver<VoteImpl>> observer_ = nullptr;

  // The number of voting channels issued, and the number that remain
  // outstanding.
  size_t voting_channels_issued_ = 0u;
  size_t voting_channels_outstanding_ = 0u;
};

/////////////////////////////////////////////////////////////////////
// Vote

template <typename ContextType, typename VoteType, VoteType DefaultVote>
Vote<ContextType, VoteType, DefaultVote>::Vote() = default;

template <typename ContextType, typename VoteType, VoteType DefaultVote>
Vote<ContextType, VoteType, DefaultVote>::Vote(VoteType vote,
                                               const char* reason)
    : vote_(std::move(vote)), reason_(reason) {}

template <typename ContextType, typename VoteType, VoteType DefaultVote>
Vote<ContextType, VoteType, DefaultVote>::Vote(const Vote& rhs) = default;

template <typename ContextType, typename VoteType, VoteType DefaultVote>
Vote<ContextType, VoteType, DefaultVote>&
Vote<ContextType, VoteType, DefaultVote>::operator=(
    const Vote<ContextType, VoteType, DefaultVote>& rhs) = default;

template <typename ContextType, typename VoteType, VoteType DefaultVote>
Vote<ContextType, VoteType, DefaultVote>::~Vote() = default;

template <typename ContextType, typename VoteType, VoteType DefaultVote>
bool Vote<ContextType, VoteType, DefaultVote>::operator==(
    const Vote<ContextType, VoteType, DefaultVote>& vote) const {
  DCHECK(reason_);
  DCHECK(vote.reason_);
  return vote_ == vote.vote_ && ::strcmp(reason_, vote.reason_) == 0;
}

template <typename ContextType, typename VoteType, VoteType DefaultVote>
bool Vote<ContextType, VoteType, DefaultVote>::operator!=(
    const Vote<ContextType, VoteType, DefaultVote>& vote) const {
  return !(*this == vote);
}

template <typename ContextType, typename VoteType, VoteType DefaultVote>
bool Vote<ContextType, VoteType, DefaultVote>::IsValid() const {
  return reason_;
}

/////////////////////////////////////////////////////////////////////
// VoteObserver

template <class VoteImpl>
VoteObserver<VoteImpl>::~VoteObserver() = default;

/////////////////////////////////////////////////////////////////////
// VotingChannel

template <class VoteImpl>
VotingChannel<VoteImpl>::VotingChannel() = default;

template <class VoteImpl>
VotingChannel<VoteImpl>::VotingChannel(VotingChannel<VoteImpl>&& rhs) {
  Take(std::move(rhs));
}

template <class VoteImpl>
VotingChannel<VoteImpl>& VotingChannel<VoteImpl>::operator=(
    VotingChannel<VoteImpl>&& rhs) {
  Take(std::move(rhs));
  return *this;
}

template <class VoteImpl>
VotingChannel<VoteImpl>::~VotingChannel() {
  Reset();
}

template <class VoteImpl>
void VotingChannel<VoteImpl>::SubmitVote(const ContextType* context,
                                         const VoteImpl& vote) {
  DCHECK(IsValid());

#if DCHECK_IS_ON()
  // Ensure that only one vote is submitted for a given |context| at any time.
  bool inserted = votes_.emplace(context, vote).second;
  DCHECK(inserted);
#endif  // DCHECK_IS_ON()

  factory_->GetObserver(PassKey())->OnVoteSubmitted(voter_id_, context, vote);
}

template <class VoteImpl>
void VotingChannel<VoteImpl>::ChangeVote(const ContextType* context,
                                         const VoteImpl& new_vote) {
  DCHECK(IsValid());

#if DCHECK_IS_ON()
  // Ensure that a vote exists for this context.
  auto it = votes_.find(context);
  CHECK(it != votes_.end(), base::NotFatalUntil::M130);

  // Ensure the vote was actually changed.
  DCHECK(new_vote != it->second);
  it->second = new_vote;
#endif  // DCHECK_IS_ON()

  factory_->GetObserver(PassKey())->OnVoteChanged(voter_id_, context, new_vote);
}

template <class VoteImpl>
void VotingChannel<VoteImpl>::InvalidateVote(const ContextType* context) {
  DCHECK(IsValid());

#if DCHECK_IS_ON()
  // Ensure that an existing vote is invalidated.
  size_t removed = votes_.erase(context);
  DCHECK_EQ(removed, 1u);
#endif  // DCHECK_IS_ON()

  factory_->GetObserver(PassKey())->OnVoteInvalidated(voter_id_, context);
}

template <class VoteImpl>
bool VotingChannel<VoteImpl>::IsValid() const {
  return factory_ && voter_id_;
}

template <class VoteImpl>
void VotingChannel<VoteImpl>::Reset() {
  if (!factory_)
    return;
  DCHECK(voter_id_);
  factory_->OnVotingChannelDestroyed(PassKey());
  factory_ = nullptr;
  voter_id_ = VoterId<VoteImpl>();

#if DCHECK_IS_ON()
  // Ensure that all outstanding votes are invalidated before resetting this
  // channel.
  DCHECK(votes_.empty());
#endif  // DCHECK_IS_ON()
}

template <class VoteImpl>
VotingChannel<VoteImpl>::VotingChannel(
    base::PassKey<VotingChannelFactory<VoteImpl>>,
    VotingChannelFactory<VoteImpl>* factory,
    VoterId<VoteImpl> voter_id)
    : factory_(factory), voter_id_(voter_id) {}

template <class VoteImpl>
void VotingChannel<VoteImpl>::Take(VotingChannel<VoteImpl>&& rhs) {
  Reset();
  factory_ = std::exchange(rhs.factory_, nullptr);
  voter_id_ = std::exchange(rhs.voter_id_, VoterId<VoteImpl>());

#if DCHECK_IS_ON()
  // Track outstanding votes across moves.
  votes_ =
      std::exchange(rhs.votes_, base::flat_map<const ContextType*, VoteImpl>());
#endif  // DCHECK_IS_ON()
}

/////////////////////////////////////////////////////////////////////
// VotingChannelFactory
template <class VoteImpl>
VotingChannelFactory<VoteImpl>::VotingChannelFactory(
    VoteObserver<VoteImpl>* observer)
    : observer_(observer) {
  DCHECK(observer_);
}

template <class VoteImpl>
VotingChannelFactory<VoteImpl>::~VotingChannelFactory() {
  // We expect all voters to have severed their VotingChannels before we are
  // torn down.
  DCHECK_EQ(0u, voting_channels_outstanding_);
}

template <class VoteImpl>
VotingChannel<VoteImpl> VotingChannelFactory<VoteImpl>::BuildVotingChannel() {
  ++voting_channels_outstanding_;
  // TODO(sebmarchand): Use VoterId<VoteImpl>::Generator instead of
  // FromUnsafeValue.
  // Note: The pre-increment operator is used so that the value of the first
  // voter ID is 1. This is required because 0 is the value for an invalid
  // VoterId.
  VoterId<VoteImpl> new_voter_id =
      VoterId<VoteImpl>::FromUnsafeValue(++voting_channels_issued_);
  DCHECK(!new_voter_id.is_null());
  return VotingChannel<VoteImpl>(PassKey(), this, new_voter_id);
}

template <class VoteImpl>
void VotingChannelFactory<VoteImpl>::OnVotingChannelDestroyed(
    base::PassKey<VotingChannel<VoteImpl>>) {
  DCHECK_LT(0u, voting_channels_outstanding_);
  --voting_channels_outstanding_;
}

}  // namespace voting
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VOTING_VOTING_H_

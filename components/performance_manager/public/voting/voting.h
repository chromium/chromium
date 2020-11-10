// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VOTING_VOTING_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VOTING_VOTING_H_

// Declares the various structures and formats associated with a templated
// voting system. This is templated on a vote type (e.g. a priority) and a vote
// context (e.g. a specific node type).
//
// There are 6 interrelated classes declared here:
//
// (1) Vote - A simple wrapper for a vote, unattributed, unsubmitted and
//     unowned. This is a final concrete class.
// (2) AcceptedVote - A thin wrapper around a Vote, which gives it ownership
//     (by a VoteConsumer) and tracking (via VoteReceipt). This is a final
//     concrete class.
// (3) VoteReceipt - Counterpart to an AcceptedVote. Issued to a voter when a
//     vote is accepted, and used to allow that voter to change / withdraw /
//     cancel their vote. This is a final concrete class.
// (4) VoteConsumer - Destination for Votes (making them AcceptedVotes) and
//     issuer of VoteReceipts. This is an interface.
// (5) VotingChannel - A mechanism by which a voter can submit votes to
//     VoteConsumer, which endows the vote with a unique voter attribution. This
//     is a final concrete class.
// (6) VotingChannelFactory - Producer and tracker of VotingChannels, meant to
//     be owned by a VoteConsumer. This is a final concrete class.
//
// Voters register themselves with VoteConsumers, which issues them a private
// VotingChannel to use for submitting Votes. Voters then submit raw Votes to
// VoteConsumers via the VotingChannel, which issues them a VoteReceipt. When a
// VoteConsumer accepts a vote it becomes an AcceptedVote. AcceptedVotes and
// VoteReceipts are entangled for their lifetimes, with AcceptedVotes (owned by
// the consumer) outliving their associated VoteReceipts (owned by the voter).
// Both types are move-only, ensuring that one vote had exactly one receipt. An
// AcceptedVote passes through a small lifecycle (no receipt yet issued -> has
// issued a receipt -> receipt has been destroyed and vote is invalidated).
//
// VotingChannels are tracked by a VotingChannelFactory, which itself is owned
// by the VoteConsumer. The VotingChannelFactory is used to track all
// outstanding VotingChannels, and helps to ensure that Voters have been
// torn-down / disconnected before the VoteConsumer disappears.
//
// When a vote is invalidated the consumer owning the vote is notified so that
// they may update internal data structures, aggregates, etc.
//
// None of these objects are thread-safe, and they should all be used from a
// single sequence. In practice this will be the PM sequence.
//
// IMPLEMENTATION NOTES:
//
// AcceptedVote and VoteReceipt maintain explicit back-pointers to each other,
// which is what allows them both to be movable and yet not incur a heap
// allocation. This means that they are both amenable to storage in memory-dense
// containers (flat_map, flat_set, vector) and won't incur further heap
// allocations. It is expected that there could be up to O(1000s) of votes lying
// around, but that they will not often have to move. Thus the pointer
// maintenance is a reasonable trade-off for memory efficiency.

#include <cstring>
#include <map>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/util/type_safety/id_type.h"
#include "base/util/type_safety/pass_key.h"

namespace performance_manager {
namespace voting {

template <class VoteImpl>
class VoteConsumer;

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

  bool IsValid() const;

 private:
  VoteType vote_ = DefaultVote;
  const char* reason_ = nullptr;
};

template <typename VoteImpl>
using VoterId = util::IdTypeU32<VoteImpl>;
template <typename VoteImpl>
constexpr VoterId<VoteImpl> kInvalidVoterId;

// A raw vote becomes an AcceptedVote once a VoteConsumer receives and stores
// it, associating it with a VoterId.
template <class VoteImpl>
class AcceptedVote;

// An issued vote returns a move-only VoteReceipt. Destroying the vote receipt
// is equivalent to invalidating the vote.
template <class VoteImpl>
class VoteReceipt final {
 public:
  using PassKey = util::PassKey<VoteReceipt<VoteImpl>>;

  VoteReceipt();
  VoteReceipt(const VoteReceipt& rhs) = delete;
  VoteReceipt(VoteReceipt&& rhs);

  VoteReceipt& operator=(const VoteReceipt& rhs) = delete;
  VoteReceipt& operator=(VoteReceipt&& rhs);

  ~VoteReceipt();

  // Returns true if this receipt is entangled with a vote.
  bool HasVote() const;
  bool HasVote(const AcceptedVote<VoteImpl>* vote) const;

  // Returns the consumer that this vote was submitted to. Can only be called if
  // HasVote returns true.
  VoteConsumer<VoteImpl>* GetConsumer() const;

  // Returns the voter ID associated with this receipt. Can only be called if
  // HasVote returns true.
  VoterId<VoteImpl> GetVoterId() const;

  // Returns the vote corresponding to this receipt. Can only be called if
  // HasVote returns true.
  const VoteImpl& GetVote() const;

  // Changes the upstream vote associated with this vote receipt. Can only be
  // called if HasVote returns true.
  void ChangeVote(typename VoteImpl::VoteType vote, const char* reason);

  // Rests the vote receipt, canceling the upstream vote.
  void Reset();

  // VoteReceipt and AcceptedVote are tightly intertwined, and maintain
  // back-pointers to each other as one or the other is moved. The 2 following
  // functions are only meant to be used by AcceptedVote.

  // Allows an AcceptedVote to create an entangled receipt.
  VoteReceipt(util::PassKey<AcceptedVote<VoteImpl>>,
              AcceptedVote<VoteImpl>* vote);

  // Allows an AcceptedVote to update its backpointer.
  void MoveVote(util::PassKey<AcceptedVote<VoteImpl>>,
                AcceptedVote<VoteImpl>* old_vote,
                AcceptedVote<VoteImpl>* new_vote);

 private:
  void Take(VoteReceipt&& rhs);

  // A back-pointer to the accepted vote, so that it can be notified when this
  // receipt is destroyed.
  AcceptedVote<VoteImpl>* vote_ = nullptr;
};

// A move-only wrapper for a vote and its associated receipt. AcceptedVotes
// and VoteReceipts exist in pairs, and they update their pointers to each
// other. An AcceptedVote goes through the following lifecycle:
//
// (1) Initial creation. It has a vote, but no receipt. The vote is considered
//     valid at this point.
// (2) Association with a receipt. The vote is considered valid at this point.
// (3) Invalidation. The receipt is destroyed, which reaches back and
//     invalidates the vote. The vote is considered invalid at this point, and
//     no new receipt may be issued.
//
// An AcceptedVote must outlive its associated VoteReceipt, much like
// VoteConsumers must outlive voters. Note that a default constructed
// AcceptedVote is always in the invalidated state.
template <class VoteImpl>
class AcceptedVote final {
 public:
  using ContextType = typename VoteImpl::ContextType;
  using PassKey = util::PassKey<AcceptedVote<VoteImpl>>;

  AcceptedVote();
  AcceptedVote(VoteConsumer<VoteImpl>* consumer,
               VoterId<VoteImpl> voter_id,
               const ContextType* context,
               const VoteImpl& vote);
  AcceptedVote(const AcceptedVote& rhs) = delete;
  AcceptedVote(AcceptedVote&& rhs);

  AcceptedVote& operator=(const AcceptedVote& rhs) = delete;
  AcceptedVote& operator=(AcceptedVote&& rhs);

  ~AcceptedVote();

  // Returns true if this vote is associated with a receipt.
  bool HasReceipt() const;
  bool HasReceipt(const VoteReceipt<VoteImpl>* receipt) const;

  bool IsValid() const;
  VoteReceipt<VoteImpl> IssueReceipt();

  VoteConsumer<VoteImpl>* consumer() const { return consumer_; }
  VoterId<VoteImpl> voter_id() const { return voter_id_; }
  const ContextType* context() const { return context_; }
  const VoteImpl& vote() const { return vote_; }

  // Allows an accepted vote to be updated in place.
  void UpdateVote(const VoteImpl& vote);

  // VoteReceipt and AcceptedVote are tightly intertwined, and maintain
  // back-pointers to each other as one or the other is moved. The following
  // functions are only meant to be used by VoteReceipt.

  // Allows a VoteReceipt to associate itself with this vote.
  void SetReceipt(util::PassKey<VoteReceipt<VoteImpl>>,
                  VoteReceipt<VoteImpl>* receipt);

  // Allows a VoteReceipt to update its backpointer.
  void MoveReceipt(util::PassKey<VoteReceipt<VoteImpl>>,
                   VoteReceipt<VoteImpl>* old_receipt,
                   VoteReceipt<VoteImpl>* new_receipt);

  // Allows a VoteReceipt to change this vote.
  void ChangeVote(util::PassKey<VoteReceipt<VoteImpl>>,
                  typename VoteImpl::VoteType vote,
                  const char* reason);

  // Allows a VoteReceipt to invalidate this vote.
  void InvalidateVote(util::PassKey<VoteReceipt<VoteImpl>>,
                      VoteReceipt<VoteImpl>* receipt);

 private:
  void Take(AcceptedVote&& rhs);

  // The consumer that accepted the vote.
  VoteConsumer<VoteImpl>* consumer_ = nullptr;

  // The ID of the voter that submitted the vote. This is defined by the
  // VoteConsumer.
  VoterId<VoteImpl> voter_id_ = kInvalidVoterId<VoteImpl>;

  const ContextType* context_ = nullptr;

  // The vote that is being wrapped.
  VoteImpl vote_;

  // The associated vote receipt.
  VoteReceipt<VoteImpl>* receipt_ = nullptr;

  // Set to true when an associated receipt is destroyed.
  bool invalidated_ = true;
};

template <class VoteImpl>
class VotingChannelFactory;

// A channel that a voter can use to submit votes to a VoteConsumer. A move-only
// type so that it can't be shared by multiple voters. This must be destroyed
// before the issuing VotingChannelFactory.
template <class VoteImpl>
class VotingChannel final {
 public:
  using ContextType = typename VoteImpl::ContextType;
  using PassKey = util::PassKey<VotingChannel<VoteImpl>>;

  VotingChannel();
  VotingChannel(const VotingChannel& rhs) = delete;
  VotingChannel(VotingChannel&& rhs);
  VotingChannel& operator=(const VotingChannel& rhs) = delete;
  VotingChannel& operator=(VotingChannel&& rhs);
  ~VotingChannel();

  // Submits a vote through this voting channel. Can only be called if this
  // VotingChannel is valid.
  VoteReceipt<VoteImpl> SubmitVote(const ContextType* context,
                                   const VoteImpl& vote);

  // Returns true if this VotingChannel is valid.
  bool IsValid() const;

  // Resets this voting channel.
  void Reset();

  VoterId<VoteImpl> voter_id() const { return voter_id_; }

  VotingChannelFactory<VoteImpl>* factory_for_testing() const {
    return factory_;
  }

  // VotingChannelFactory is the sole producer of VotingChannels.
  VotingChannel(util::PassKey<VotingChannelFactory<VoteImpl>>,
                VotingChannelFactory<VoteImpl>* factory,
                VoterId<VoteImpl> voter_id);

 private:
  void Take(VotingChannel&& rhs);

  // Used to reach back into the factory to decrement the outstanding
  // VotingChannel count, and for routing votes to the consumer.
  VotingChannelFactory<VoteImpl>* factory_ = nullptr;
  VoterId<VoteImpl> voter_id_ = kInvalidVoterId<VoteImpl>;
};

// A helper for creating VotingChannels that binds a unique VoterId (and
// passes the votes along to the VoteConsumer with that VoterId), and a tracking
// token to ensure that the voter disconnects from the VoteConsumer before it is
// itself destroyed. Implementations of VoteConsumers should own an instance of
// this and use it to emit VotingChannels. This class will DCHECK in its
// destructor if there are outstanding VotingChannels at its death.
template <class VoteImpl>
class VotingChannelFactory final {
 public:
  explicit VotingChannelFactory(VoteConsumer<VoteImpl>* consumer);
  ~VotingChannelFactory();
  VotingChannelFactory(const VotingChannelFactory& rhs) = delete;
  VotingChannelFactory& operator=(const VotingChannelFactory& rhs) = delete;

  // Builds a new VotingChannel that routes votes to the |consumer_|.
  VotingChannel<VoteImpl> BuildVotingChannel();

  size_t voting_channels_issued() const { return voting_channels_issued_; }
  size_t voting_channels_outstanding() const {
    return voting_channels_outstanding_;
  }

  // Used by ~VotingChannel to notify the factory that a channel has been
  // torn down.
  void OnVotingChannelDestroyed(util::PassKey<VotingChannel<VoteImpl>>);

  VoteConsumer<VoteImpl>* GetConsumer(util::PassKey<VotingChannel<VoteImpl>>) {
    return consumer_;
  }

 private:
  // The consumer that owns this factory.
  VoteConsumer<VoteImpl>* consumer_ = nullptr;

  // The number of voting channels issued, and the number that remain
  // outstanding.
  size_t voting_channels_issued_ = 0u;
  size_t voting_channels_outstanding_ = 0u;
};

// A consumer of votes. By convention a VoteConsumer exposes mechanisms for
// registering voters with the consumer, and providing them a VotingChannel to
// invoke. This is done via owning a VotingChannelFactory, and building
// VotingChannels. Consumers must outlive any VotingChannels they have issued.
template <class VoteImpl>
class VoteConsumer {
 public:
  using ContextType = typename VoteImpl::ContextType;

  virtual ~VoteConsumer();

  // Used by a VotingChannel to submit votes to this consumer.
  virtual VoteReceipt<VoteImpl> SubmitVote(
      util::PassKey<VotingChannel<VoteImpl>>,
      VoterId<VoteImpl> voter_id,
      const ContextType* context,
      const VoteImpl& vote) = 0;

  // Used by an AcceptedVote to notify a consumer that a previously issued vote
  // has been changed. The consumer should update |old_vote| in-place using the
  // data from |new_vote|.
  virtual void ChangeVote(util::PassKey<AcceptedVote<VoteImpl>>,
                          AcceptedVote<VoteImpl>* old_vote,
                          const VoteImpl& new_vote) = 0;

  // Used by a AcceptedVote to notify a consumer that a previously issued
  // receipt has been destroyed, and the vote is now invalidated. This is kept
  // protected as it is part of a private contract between an AcceptedVote and a
  // VoteConsumer.
  virtual void VoteInvalidated(util::PassKey<AcceptedVote<VoteImpl>>,
                               AcceptedVote<VoteImpl>* vote) = 0;
};

template <class VoteImpl>
class VoteConsumerDefaultImpl;

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
class VoteConsumerDefaultImpl : public VoteConsumer<VoteImpl> {
 public:
  using ContextType = typename VoteImpl::ContextType;
  using PassKey = util::PassKey<VoteConsumerDefaultImpl>;

  explicit VoteConsumerDefaultImpl(VoteObserver<VoteImpl>* vote_observer);
  ~VoteConsumerDefaultImpl() override;

  // Builds a new VotingChannel that routes votes to |vote_observer_|.
  VotingChannel<VoteImpl> BuildVotingChannel();

  size_t voting_channels_issued() const {
    return voting_channel_factory_.voting_channels_issued();
  }

  // VoteConsumer:
  VoteReceipt<VoteImpl> SubmitVote(util::PassKey<VotingChannel<VoteImpl>>,
                                   VoterId<VoteImpl> voter_id,
                                   const ContextType* context,
                                   const VoteImpl& vote) override;
  void ChangeVote(util::PassKey<AcceptedVote<VoteImpl>>,
                  AcceptedVote<VoteImpl>* old_vote,
                  const VoteImpl& new_vote) override;
  void VoteInvalidated(util::PassKey<AcceptedVote<VoteImpl>>,
                       AcceptedVote<VoteImpl>* vote) override;

 private:
  VoteObserver<VoteImpl>* vote_observer_;

  VotingChannelFactory<VoteImpl> voting_channel_factory_;

  std::map<const ContextType*, AcceptedVote<VoteImpl>> accepted_votes_;
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
// VoteReceipt

template <class VoteImpl>
VoteReceipt<VoteImpl>::VoteReceipt() = default;

template <class VoteImpl>
VoteReceipt<VoteImpl>::VoteReceipt(VoteReceipt<VoteImpl>&& rhs) {
  Take(std::move(rhs));
}

template <class VoteImpl>
VoteReceipt<VoteImpl>& VoteReceipt<VoteImpl>::operator=(
    VoteReceipt<VoteImpl>&& rhs) {
  Take(std::move(rhs));
  return *this;
}

template <class VoteImpl>
VoteReceipt<VoteImpl>::~VoteReceipt() {
  Reset();
}

template <class VoteImpl>
bool VoteReceipt<VoteImpl>::HasVote() const {
  return vote_;
}

template <class VoteImpl>
bool VoteReceipt<VoteImpl>::HasVote(const AcceptedVote<VoteImpl>* vote) const {
  return vote_ == vote;
}

template <class VoteImpl>
VoteConsumer<VoteImpl>* VoteReceipt<VoteImpl>::GetConsumer() const {
  return vote_->consumer();
}

template <class VoteImpl>
VoterId<VoteImpl> VoteReceipt<VoteImpl>::GetVoterId() const {
  return vote_->voter_id();
}

template <class VoteImpl>
const VoteImpl& VoteReceipt<VoteImpl>::GetVote() const {
  return vote_->vote();
}

template <class VoteImpl>
void VoteReceipt<VoteImpl>::ChangeVote(typename VoteImpl::VoteType new_vote,
                                       const char* reason) {
  DCHECK(vote_);

  // Do nothing if the vote hasn't actually changed.
  const auto& vote = vote_->vote();
  if (vote.value() == new_vote && vote.reason() == reason)
    return;

  vote_->ChangeVote(PassKey(), new_vote, reason);
}

template <class VoteImpl>
void VoteReceipt<VoteImpl>::Reset() {
  if (vote_) {
    vote_->InvalidateVote(PassKey(), this);
    vote_ = nullptr;
  }
}

template <class VoteImpl>
void VoteReceipt<VoteImpl>::MoveVote(util::PassKey<AcceptedVote<VoteImpl>>,
                                     AcceptedVote<VoteImpl>* old_vote,
                                     AcceptedVote<VoteImpl>* new_vote) {
  DCHECK(old_vote);
  DCHECK(new_vote);
  DCHECK_EQ(vote_, old_vote);
  vote_ = new_vote;

  // The vote should already be associated with this receipt (as the vote
  // initiated the move).
  DCHECK(vote_->HasReceipt(this));
}

template <class VoteImpl>
VoteReceipt<VoteImpl>::VoteReceipt(util::PassKey<AcceptedVote<VoteImpl>>,
                                   AcceptedVote<VoteImpl>* vote)
    : vote_(vote) {
  // The vote should be valid and not be associated with any receipt.
  DCHECK(vote->IsValid());
  DCHECK(vote->HasReceipt(nullptr));

  // Associate the vote with this newly issued receipt.
  vote->SetReceipt(PassKey(), this);
}

template <class VoteImpl>
void VoteReceipt<VoteImpl>::Take(VoteReceipt<VoteImpl>&& rhs) {
  Reset();

  vote_ = rhs.vote_;

  // Update the back-pointer from the vote.
  if (vote_)
    vote_->MoveReceipt(PassKey(), &rhs, this);

  rhs.vote_ = nullptr;
}

/////////////////////////////////////////////////////////////////////
// AcceptedVote

template <class VoteImpl>
AcceptedVote<VoteImpl>::AcceptedVote() = default;

template <class VoteImpl>
AcceptedVote<VoteImpl>::AcceptedVote(VoteConsumer<VoteImpl>* consumer,
                                     VoterId<VoteImpl> voter_id,
                                     const ContextType* context,
                                     const VoteImpl& vote)
    : consumer_(consumer),
      voter_id_(voter_id),
      context_(context),
      vote_(vote),
      invalidated_(false) {
  DCHECK(consumer_);
  DCHECK_NE(voter_id_, kInvalidVoterId<VoteImpl>);
  DCHECK(context_);
  DCHECK(vote_.IsValid());
}

template <class VoteImpl>
AcceptedVote<VoteImpl>::AcceptedVote(AcceptedVote<VoteImpl>&& rhs) {
  Take(std::move(rhs));
}

template <class VoteImpl>
AcceptedVote<VoteImpl>& AcceptedVote<VoteImpl>::operator=(
    AcceptedVote<VoteImpl>&& rhs) {
  Take(std::move(rhs));
  return *this;
}

template <class VoteImpl>
AcceptedVote<VoteImpl>::~AcceptedVote() {
  // A vote should not be destroyed while it has an outstanding receipt. It is
  // up to Voters to destroy their votes when they go out of scope, and
  // consumers must outlive voters.
  DCHECK(!receipt_);
}

template <class VoteImpl>
bool AcceptedVote<VoteImpl>::HasReceipt() const {
  return receipt_;
}

template <class VoteImpl>
bool AcceptedVote<VoteImpl>::HasReceipt(
    const VoteReceipt<VoteImpl>* receipt) const {
  return receipt_ == receipt;
}

template <class VoteImpl>
bool AcceptedVote<VoteImpl>::IsValid() const {
  return consumer_ && voter_id_ != kInvalidVoterId<VoteImpl> &&
         vote_.IsValid() && !invalidated_;
}

template <class VoteImpl>
VoteReceipt<VoteImpl> AcceptedVote<VoteImpl>::IssueReceipt() {
  return VoteReceipt<VoteImpl>(PassKey(), this);
}

template <class VoteImpl>
void AcceptedVote<VoteImpl>::UpdateVote(const VoteImpl& vote) {
  DCHECK(vote_.value() != vote.value() || vote_.reason() != vote.reason());
  vote_ = vote;
}

template <class VoteImpl>
void AcceptedVote<VoteImpl>::SetReceipt(util::PassKey<VoteReceipt<VoteImpl>>,
                                        VoteReceipt<VoteImpl>* receipt) {
  // A receipt can only be set on a vote once in its lifetime.
  DCHECK(!receipt_);
  DCHECK(!invalidated_);
  receipt_ = receipt;

  // The receipt should already be associated with this vote (the association
  // is initiated by the receipt).
  DCHECK(receipt_->HasVote(this));
}

template <class VoteImpl>
void AcceptedVote<VoteImpl>::MoveReceipt(util::PassKey<VoteReceipt<VoteImpl>>,
                                         VoteReceipt<VoteImpl>* old_receipt,
                                         VoteReceipt<VoteImpl>* new_receipt) {
  DCHECK(old_receipt);
  DCHECK(new_receipt);
  DCHECK_EQ(receipt_, old_receipt);
  receipt_ = new_receipt;

  // The receipt should already be associated with this vote (its calling for
  // the move).
  DCHECK(receipt_->HasVote(this));
}

template <class VoteImpl>
void AcceptedVote<VoteImpl>::ChangeVote(util::PassKey<VoteReceipt<VoteImpl>>,
                                        typename VoteImpl::VoteType vote,
                                        const char* reason) {
  DCHECK(!invalidated_);
  DCHECK(vote_.value() != vote || vote_.reason() != reason);

  // Notify the consumer of the new vote.
  VoteImpl new_vote = VoteImpl(vote, reason);
  consumer_->ChangeVote(PassKey(), this, new_vote);
}

template <class VoteImpl>
void AcceptedVote<VoteImpl>::InvalidateVote(
    util::PassKey<VoteReceipt<VoteImpl>>,
    VoteReceipt<VoteImpl>* receipt) {
  DCHECK(receipt);
  DCHECK_EQ(receipt_, receipt);
  DCHECK(!invalidated_);

  // Care has to be taken not to access |receipt|, as it is running its
  // destructor. It is only passed as a parameter to ensure that the proper
  // receipt is notifying this vote.
  receipt_ = nullptr;
  invalidated_ = true;
  consumer_->VoteInvalidated(PassKey(), this);
}

template <class VoteImpl>
void AcceptedVote<VoteImpl>::Take(AcceptedVote<VoteImpl>&& rhs) {
  // An AcceptedVote can't be overwritten while it has a pending receipt.
  DCHECK(!receipt_);

  consumer_ = std::exchange(rhs.consumer_, nullptr);
  voter_id_ = std::exchange(rhs.voter_id_, kInvalidVoterId<VoteImpl>);
  context_ = std::exchange(rhs.context_, nullptr);
  vote_ = std::exchange(rhs.vote_, VoteImpl());
  receipt_ = std::exchange(rhs.receipt_, nullptr);
  invalidated_ = std::exchange(rhs.invalidated_, true);

  // Update the back-pointer from the receipt.
  if (receipt_)
    receipt_->MoveVote(PassKey(), &rhs, this);
}

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
VoteReceipt<VoteImpl> VotingChannel<VoteImpl>::SubmitVote(
    const ContextType* context,
    const VoteImpl& vote) {
  // Pass the vote along to the consumer with the bound |voter_id_|.
  return factory_->GetConsumer(PassKey())->SubmitVote(PassKey(), voter_id_,
                                                      context, vote);
}

template <class VoteImpl>
bool VotingChannel<VoteImpl>::IsValid() const {
  return factory_ && voter_id_ != kInvalidVoterId<VoteImpl>;
}

template <class VoteImpl>
void VotingChannel<VoteImpl>::Reset() {
  if (!factory_)
    return;
  DCHECK_NE(kInvalidVoterId<VoteImpl>, voter_id_);
  factory_->OnVotingChannelDestroyed(PassKey());
  factory_ = nullptr;
  voter_id_ = kInvalidVoterId<VoteImpl>;
}

template <class VoteImpl>
VotingChannel<VoteImpl>::VotingChannel(
    util::PassKey<VotingChannelFactory<VoteImpl>>,
    VotingChannelFactory<VoteImpl>* factory,
    VoterId<VoteImpl> voter_id)
    : factory_(factory), voter_id_(voter_id) {}

template <class VoteImpl>
void VotingChannel<VoteImpl>::Take(VotingChannel<VoteImpl>&& rhs) {
  Reset();
  factory_ = std::exchange(rhs.factory_, nullptr);
  voter_id_ = std::exchange(rhs.voter_id_, kInvalidVoterId<VoteImpl>);
}

/////////////////////////////////////////////////////////////////////
// VotingChannelFactory
template <class VoteImpl>
VotingChannelFactory<VoteImpl>::VotingChannelFactory(
    VoteConsumer<VoteImpl>* consumer)
    : consumer_(consumer) {
  DCHECK(consumer);
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
  VoterId<VoteImpl> new_voter_id =
      VoterId<VoteImpl>::FromUnsafeValue(++voting_channels_issued_);
  return VotingChannel<VoteImpl>(
      util::PassKey<VotingChannelFactory<VoteImpl>>(), this, new_voter_id);
}

template <class VoteImpl>
void VotingChannelFactory<VoteImpl>::OnVotingChannelDestroyed(
    util::PassKey<VotingChannel<VoteImpl>>) {
  DCHECK_LT(0u, voting_channels_outstanding_);
  --voting_channels_outstanding_;
}

/////////////////////////////////////////////////////////////////////
// VoteConsumer

template <class VoteImpl>
VoteConsumer<VoteImpl>::~VoteConsumer() = default;

/////////////////////////////////////////////////////////////////////
// VoteObserver

template <class VoteImpl>
VoteObserver<VoteImpl>::~VoteObserver() = default;

/////////////////////////////////////////////////////////////////////
// VoteConsumerDefaultImpl

template <class VoteImpl>
VoteConsumerDefaultImpl<VoteImpl>::VoteConsumerDefaultImpl(
    VoteObserver<VoteImpl>* vote_observer)
    : vote_observer_(vote_observer), voting_channel_factory_(this) {}

template <class VoteImpl>
VoteConsumerDefaultImpl<VoteImpl>::~VoteConsumerDefaultImpl() = default;

template <class VoteImpl>
VotingChannel<VoteImpl>
VoteConsumerDefaultImpl<VoteImpl>::BuildVotingChannel() {
  return voting_channel_factory_.BuildVotingChannel();
}

template <class VoteImpl>
VoteReceipt<VoteImpl> VoteConsumerDefaultImpl<VoteImpl>::SubmitVote(
    util::PassKey<VotingChannel<VoteImpl>>,
    VoterId<VoteImpl> voter_id,
    const ContextType* context,
    const VoteImpl& vote) {
  AcceptedVote<VoteImpl> accepted_vote(this, voter_id, context, vote);

  VoteReceipt<VoteImpl> vote_receipt = accepted_vote.IssueReceipt();

  bool inserted =
      accepted_votes_.emplace(context, std::move(accepted_vote)).second;
  DCHECK(inserted);

  vote_observer_->OnVoteSubmitted(voter_id, context, vote);

  return vote_receipt;
}

template <class VoteImpl>
void VoteConsumerDefaultImpl<VoteImpl>::ChangeVote(
    util::PassKey<AcceptedVote<VoteImpl>>,
    AcceptedVote<VoteImpl>* old_vote,
    const VoteImpl& new_vote) {
  auto it = accepted_votes_.find(old_vote->context());
  DCHECK(it != accepted_votes_.end());
  auto* accepted_vote = &it->second;
  DCHECK_EQ(accepted_vote, old_vote);

  accepted_vote->UpdateVote(new_vote);

  vote_observer_->OnVoteChanged(accepted_vote->voter_id(),
                                accepted_vote->context(), new_vote);
}

template <class VoteImpl>
void VoteConsumerDefaultImpl<VoteImpl>::VoteInvalidated(
    util::PassKey<AcceptedVote<VoteImpl>>,
    AcceptedVote<VoteImpl>* vote) {
  auto it = accepted_votes_.find(vote->context());
  DCHECK(it != accepted_votes_.end());
  auto* accepted_vote = &it->second;
  DCHECK_EQ(accepted_vote, vote);

  vote_observer_->OnVoteInvalidated(accepted_vote->voter_id(),
                                    accepted_vote->context());

  accepted_votes_.erase(it);
}

}  // namespace voting
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VOTING_VOTING_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FRAME_PRIORITY_FRAME_PRIORITY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FRAME_PRIORITY_FRAME_PRIORITY_H_

// Declares the various structures and formats associated with the frame
// priority voting system.
//
// There are 6 interrelated classes declared here:
//
// (1) Vote - A simple wrapper for a FramePriority vote, unattributed,
//     unsubmittted and unowned. This is a final concrete class.
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
// Both types are move-only, ensuring that one vote had exactly one receipt. A
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
// allocations. It is expected that there be O(1000s) of votes lying around, but
// that they will not often have to move. Thus the pointer maintenance is a
// reasonable trade-off for memory efficiency.
//
// TODO(chrisha): Once workers are added to the graph this should be an
// "execution context priority" voting system, where an execution context is a
// worker or a frame (document), and a common base-class of those node types.

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/task/task_traits.h"

namespace performance_manager {

class FrameNode;

namespace frame_priority {

class VoteConsumer;

using VoterId = uint32_t;
constexpr VoterId kInvalidVoterId = 0;

// Helper function equivalent to strcmp, but that is safe to use with nullptr.
int ReasonCompare(const char* reason1, const char* reason2);

// Helper class for storing a priority and a reason.
class PriorityAndReason {
 public:
  PriorityAndReason() = default;
  constexpr PriorityAndReason(base::TaskPriority priority, const char* reason)
      : priority_(priority), reason_(reason) {}
  PriorityAndReason(const PriorityAndReason&) = default;
  PriorityAndReason& operator=(const PriorityAndReason&) = default;
  ~PriorityAndReason() = default;

  base::TaskPriority priority() const { return priority_; }
  const char* reason() const { return reason_; }

  // Returns -1, 0 or 1 indicating the outcome of a comparison of this value
  // and |other|.
  int Compare(const PriorityAndReason& other) const;

  bool operator==(const PriorityAndReason& other) const;
  bool operator!=(const PriorityAndReason& other) const;
  bool operator<=(const PriorityAndReason& other) const;
  bool operator>=(const PriorityAndReason& other) const;
  bool operator<(const PriorityAndReason& other) const;
  bool operator>(const PriorityAndReason& other) const;

 private:
  base::TaskPriority priority_ = base::TaskPriority::LOWEST;
  const char* reason_ = nullptr;
};

// Contains a single vote. Specifically allows copying, etc, so as to be STL
// container friendly.
class Vote final {
 public:
  Vote();
  Vote(const FrameNode* frame_node,
       base::TaskPriority priority,
       const char* reason);
  Vote(const Vote& rhs);

  Vote& operator=(const Vote& rhs);

  ~Vote();

  const FrameNode* frame_node() const { return frame_node_; }
  base::TaskPriority priority() const { return priority_; }
  const char* reason() const { return reason_; }

  bool operator==(const Vote& vote) const;
  bool operator!=(const Vote& vote) const;

  bool IsValid() const;

 private:
  const FrameNode* frame_node_ = nullptr;
  base::TaskPriority priority_ = base::TaskPriority::LOWEST;
  const char* reason_ = nullptr;
};

// A raw vote becomes an AcceptedVote once a VoteConsumer receives and stores
// it, associating it with a VoterId.
class AcceptedVote;

// An issued vote returns a move-only VoteReceipt. Destroying the vote receipt
// is equivalent to invalidating the vote.
class VoteReceipt final {
 public:
  VoteReceipt();
  VoteReceipt(const VoteReceipt& rhs) = delete;
  VoteReceipt(VoteReceipt&& rhs);

  VoteReceipt& operator=(const VoteReceipt& rhs) = delete;
  VoteReceipt& operator=(VoteReceipt&& rhs);

  ~VoteReceipt();

  // Returns true if this receipt is entangled with a vote.
  bool HasVote() const;
  bool HasVote(const AcceptedVote* vote) const;

  // Returns the consumer that this vote was submitted to. Can only be called if
  // HasVote returns true.
  VoteConsumer* GetConsumer() const;

  // Returns the voter ID associated with this receipt. Can only be called if
  // HasVote returns true.
  VoterId GetVoterId() const;

  // Returns the vote corresponding to this receipt. Can only be called if
  // HasVote returns true.
  const Vote& GetVote() const;

  // Changes the upstream vote associated with this vote receipt. Can only be
  // called if HasVote returns true.
  void ChangeVote(base::TaskPriority priority, const char* reason);

  // Rests the vote receipt, canceling the upstream vote.
  void Reset();

 protected:
  // VoteReceipt and AcceptedVote are tightly intertwined, and maintain
  // back-pointers to each other as one or the other is moved. The friendship
  // allows AcceptedVote to reach in and update its VoteReceipt counterpart. No
  // other class needs access to these functions.
  friend class AcceptedVote;

  // Allows a AcceptedVote to create an entangled receipt.
  explicit VoteReceipt(AcceptedVote* vote);

  // Allows a AcceptedVote to update its backpointer.
  void MoveVote(AcceptedVote* old_vote, AcceptedVote* new_vote);

 private:
  void Take(VoteReceipt&& rhs);

  // A back-pointer to the accepted vote, so that it can be notified when this
  // receipt is destroyed.
  AcceptedVote* vote_ = nullptr;
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
class AcceptedVote final {
 public:
  AcceptedVote();
  AcceptedVote(VoteConsumer* consumer, VoterId voter_id, const Vote& vote);
  AcceptedVote(const AcceptedVote& rhs) = delete;
  AcceptedVote(AcceptedVote&& rhs);

  AcceptedVote& operator=(const AcceptedVote& rhs) = delete;
  AcceptedVote& operator=(AcceptedVote&& rhs);

  ~AcceptedVote();

  // Returns true if this vote is associated with a receipt.
  bool HasReceipt() const;
  bool HasReceipt(const VoteReceipt* receipt) const;

  bool IsValid() const;
  VoteReceipt IssueReceipt();

  VoteConsumer* consumer() const { return consumer_; }
  VoterId voter_id() const { return voter_id_; }
  const Vote& vote() const { return vote_; }

  // Allows an accepted vote to be updated in place.
  void UpdateVote(const Vote& vote);

 protected:
  // VoteReceipt and AcceptedVote are tightly intertwined, and maintain
  // back-pointers to each other as one or the other is moved. The friendship
  // allows VoteReceipt to reach in and update its AcceptedVote counterpart. No
  // other class needs access to these functions.
  friend class VoteReceipt;

  // Allows a VoteReceipt to associate itself with this vote.
  void SetReceipt(VoteReceipt* receipt);

  // Allows a VoteReceipt to update its backpointer.
  void MoveReceipt(VoteReceipt* old_receipt, VoteReceipt* new_receipt);

  // Allows a VoteReceipt to change this vote. The vote receipt gives up its
  // receipt only to be returned a new one.
  VoteReceipt ChangeVote(VoteReceipt receipt,
                         base::TaskPriority priority,
                         const char* reason);

  // Allows a VoteReceipt to invalidate this vote.
  void InvalidateVote(VoteReceipt* receipt);

 private:
  void Take(AcceptedVote&& rhs);

  // The consumer that accepted the vote.
  VoteConsumer* consumer_ = nullptr;

  // The ID of the voter that submitted the vote. This is defined by the
  // VoteConsumer.
  VoterId voter_id_ = kInvalidVoterId;

  // The vote that is being wrapped.
  Vote vote_;

  // The associated vote receipt.
  VoteReceipt* receipt_ = nullptr;

  // Set to true when an associated receipt is destroyed.
  bool invalidated_ = true;
};

class VotingChannelFactory;

// A channel that a voter can use to submit votes to a VoteConsumer. A move-only
// type so that it can't be shared by multiple voters. This must be destroyed
// before the issuing VotingChannelFactory.
class VotingChannel final {
 public:
  VotingChannel();
  VotingChannel(const VotingChannel& rhs) = delete;
  VotingChannel(VotingChannel&& rhs);
  VotingChannel& operator=(const VotingChannel& rhs) = delete;
  VotingChannel& operator=(VotingChannel&& rhs);
  ~VotingChannel();

  // Submits a vote through this voting channel. Can only be called if this
  // VotingChannel is valid.
  VoteReceipt SubmitVote(const Vote& vote);

  // Returns true if this VotingChannel is valid.
  bool IsValid() const;

  // Resets this voting channel.
  void Reset();

  VoterId voter_id() const { return voter_id_; }

  VotingChannelFactory* factory_for_testing() const { return factory_; }

 protected:
  friend class VotingChannelFactory;

  // VotingChannelFactory is the sole producer of VotingChannels.
  VotingChannel(VotingChannelFactory* factory, VoterId voter_id);

 private:
  void Take(VotingChannel&& rhs);

  // Used to reach back into the factory to decrement the outstanding
  // VotingChannel count, and for routing votes to the consumer.
  VotingChannelFactory* factory_ = nullptr;
  VoterId voter_id_ = kInvalidVoterId;
};

// A helper for creating VotingChannels that binds a unique VoterId (and
// passes the votes along to the VoteConsumer with that VoterId), and a tracking
// token to ensure that the voter disconnects from the VoteConsumer before it is
// itself destroyed. Implementations of VoteConsumers should own an instance of
// this and use it to emit VotingChannels. This class will DCHECK in its
// destructor if there are outstanding VotingChannels at its death.
class VotingChannelFactory final {
 public:
  explicit VotingChannelFactory(VoteConsumer* consumer);
  ~VotingChannelFactory();

  // Builds a new VotingChannel that routes votes to the |consumer_|.
  VotingChannel BuildVotingChannel();

  size_t voting_channels_issued() const { return voting_channels_issued_; }
  size_t voting_channels_outstanding() const {
    return voting_channels_outstanding_;
  }

 protected:
  friend class VotingChannel;

  // Used by ~VotingChannel to notify the factory that a channel has been
  // torn down.
  void OnVotingChannelDestroyed();

 private:
  // The consumer that owns this factory.
  VoteConsumer* consumer_ = nullptr;

  // The number of voting channels issued, and the number that remain
  // outstanding.
  size_t voting_channels_issued_ = 0u;
  size_t voting_channels_outstanding_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(VotingChannelFactory);
};

// A consumer of votes. By convention a VoteConsumer exposes mechanisms for
// registering voters with the consumer, and providing them a VotingChannel to
// invoke. This is done via owning a VotingChannelFactory, and building
// VotingChannels. Consumers must outlive any VotingChannels they have issued.
class VoteConsumer {
 public:
  VoteConsumer();
  virtual ~VoteConsumer();

 protected:
  friend class AcceptedVote;
  friend class VotingChannel;

  // Used by a VotingChannel to submit votes to this consumer.
  virtual VoteReceipt SubmitVote(VoterId voter_id, const Vote& vote) = 0;

  // Used by an AcceptedVote to notify a consumer that a previously issued vote
  // has been changed. Both the |new_vote| and the |receipt| are provided to the
  // consumer, as it may be necessary for the consumer to create an entirely
  // new vote and issue a new receipt for it (although ideally it does not do so
  // for the sake of efficiency). Alternatively, the consumer can choose to
  // update the |old_vote| in-place, using the data from |new_vote|. This is
  // kept protected as it is part of a private contract between an AcceptedVote
  // and a VoteConsumer. A naive implementation of this would be the following:
  //
  //   // Tear down the old vote before submitting a new one in order to prevent
  //   // the voter from having 2 simultaneous votes for the same frame.
  //   auto voter_id = receipt.GetVoterId();
  //   receipt.Reset();
  //   return SubmitVote(voter_id, new_vote);
  virtual VoteReceipt ChangeVote(VoteReceipt receipt,
                                 AcceptedVote* old_vote,
                                 const Vote& new_vote) = 0;

  // Used by a AcceptedVote to notify a consumer that a previously issued
  // receipt has been destroyed, and the vote is now invalidated. This is kept
  // protected as it is part of a private contract between an AcceptedVote and a
  // VoteConsumer.
  virtual void VoteInvalidated(AcceptedVote* vote) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(VoteConsumer);
};

// Provides a default implementation of VoteConsumer that implements a naive
// (less efficient) version of "ChangeVote".
class VoteConsumerDefaultImpl : public VoteConsumer {
 public:
  VoteConsumerDefaultImpl();
  ~VoteConsumerDefaultImpl() override;

  // VoteConsumer implementation left to the derived class:
  VoteReceipt SubmitVote(VoterId voter_id, const Vote& vote) override = 0;
  void VoteInvalidated(AcceptedVote* vote) override = 0;

  // VoteConsumer implementation provided by this class:
  VoteReceipt ChangeVote(VoteReceipt receipt,
                         AcceptedVote* existing_vote,
                         const Vote& new_vote) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VoteConsumerDefaultImpl);
};

}  // namespace frame_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FRAME_PRIORITY_FRAME_PRIORITY_H_

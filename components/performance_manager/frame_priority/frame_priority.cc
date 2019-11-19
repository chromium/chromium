// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/frame_priority/frame_priority.h"

#include <cstring>
#include <utility>

namespace performance_manager {
namespace frame_priority {

int ReasonCompare(const char* reason1, const char* reason2) {
  if (reason1 == reason2)
    return 0;
  if (reason1 == nullptr)
    return -1;
  if (reason2 == nullptr)
    return 1;
  return ::strcmp(reason1, reason2);
}

/////////////////////////////////////////////////////////////////////
// PriorityAndReason

int PriorityAndReason::Compare(const PriorityAndReason& other) const {
  if (priority_ > other.priority_)
    return 1;
  if (priority_ < other.priority_)
    return -1;
  return ReasonCompare(reason_, other.reason_);
}

bool PriorityAndReason::operator==(const PriorityAndReason& other) const {
  return Compare(other) == 0;
}

bool PriorityAndReason::operator!=(const PriorityAndReason& other) const {
  return Compare(other) != 0;
}

bool PriorityAndReason::operator<=(const PriorityAndReason& other) const {
  return Compare(other) <= 0;
}

bool PriorityAndReason::operator>=(const PriorityAndReason& other) const {
  return Compare(other) >= 0;
}

bool PriorityAndReason::operator<(const PriorityAndReason& other) const {
  return Compare(other) < 0;
}

bool PriorityAndReason::operator>(const PriorityAndReason& other) const {
  return Compare(other) > 0;
}

/////////////////////////////////////////////////////////////////////
// Vote

Vote::Vote() = default;

Vote::Vote(const FrameNode* frame_node,
           base::TaskPriority priority,
           const char* reason)
    : frame_node_(frame_node), priority_(priority), reason_(reason) {}

Vote::Vote(const Vote& rhs) = default;

Vote& Vote::operator=(const Vote& rhs) = default;

Vote::~Vote() = default;

bool Vote::operator==(const Vote& vote) const {
  return frame_node_ == vote.frame_node_ && priority_ == vote.priority_ &&
         ::strcmp(reason_, vote.reason_) == 0;
}

bool Vote::operator!=(const Vote& vote) const {
  return !(*this == vote);
}

bool Vote::IsValid() const {
  return frame_node_ && reason_;
}

/////////////////////////////////////////////////////////////////////
// VoteReceipt

VoteReceipt::VoteReceipt() = default;

VoteReceipt::VoteReceipt(VoteReceipt&& rhs) {
  Take(std::move(rhs));
}

VoteReceipt& VoteReceipt::operator=(VoteReceipt&& rhs) {
  Take(std::move(rhs));
  return *this;
}

VoteReceipt::~VoteReceipt() {
  Reset();
}

bool VoteReceipt::HasVote() const {
  return vote_;
}

bool VoteReceipt::HasVote(const AcceptedVote* vote) const {
  return vote_ == vote;
}

VoteConsumer* VoteReceipt::GetConsumer() const {
  return vote_->consumer();
}

VoterId VoteReceipt::GetVoterId() const {
  return vote_->voter_id();
}

const Vote& VoteReceipt::GetVote() const {
  return vote_->vote();
}

void VoteReceipt::ChangeVote(base::TaskPriority priority, const char* reason) {
  DCHECK(vote_);

  // Do nothing if the vote hasn't actually changed.
  const auto& vote = vote_->vote();
  if (vote.priority() == priority && vote.reason() == reason)
    return;

  *this = vote_->ChangeVote(std::move(*this), priority, reason);
}

void VoteReceipt::Reset() {
  if (vote_) {
    vote_->InvalidateVote(this);
    vote_ = nullptr;
  }
}

void VoteReceipt::MoveVote(AcceptedVote* old_vote, AcceptedVote* new_vote) {
  DCHECK(old_vote);
  DCHECK(new_vote);
  DCHECK_EQ(vote_, old_vote);
  vote_ = new_vote;

  // The vote should already be associated with this receipt (as the vote
  // initiated the move).
  DCHECK(vote_->HasReceipt(this));
}

VoteReceipt::VoteReceipt(AcceptedVote* vote) : vote_(vote) {
  // The vote should be valid and not be associated with any receipt.
  DCHECK(vote->IsValid());
  DCHECK(vote->HasReceipt(nullptr));

  // Associate the vote with this newly issued receipt.
  vote->SetReceipt(this);
}

void VoteReceipt::Take(VoteReceipt&& rhs) {
  vote_ = rhs.vote_;

  // Update the back-pointer from the vote.
  if (vote_)
    vote_->MoveReceipt(&rhs, this);

  rhs.vote_ = nullptr;
}

/////////////////////////////////////////////////////////////////////
// AcceptedVote

AcceptedVote::AcceptedVote() = default;

AcceptedVote::AcceptedVote(VoteConsumer* consumer,
                           VoterId voter_id,
                           const Vote& vote)
    : consumer_(consumer),
      voter_id_(voter_id),
      vote_(vote),
      invalidated_(false) {
  DCHECK(consumer);
  DCHECK_NE(kInvalidVoterId, voter_id_);
  DCHECK(vote.IsValid());
}

AcceptedVote::AcceptedVote(AcceptedVote&& rhs) {
  Take(std::move(rhs));
}

AcceptedVote& AcceptedVote::operator=(AcceptedVote&& rhs) {
  Take(std::move(rhs));
  return *this;
}

AcceptedVote::~AcceptedVote() {
  // A vote should not be destroyed while it has an outstanding receipt. It is
  // up to Voters to destroy their votes when they go out of scope, and
  // consumers must outlive voters.
  DCHECK(!receipt_);
}

bool AcceptedVote::HasReceipt() const {
  return receipt_;
}

bool AcceptedVote::HasReceipt(const VoteReceipt* receipt) const {
  return receipt_ == receipt;
}

bool AcceptedVote::IsValid() const {
  return consumer_ && voter_id_ != kInvalidVoterId && vote_.IsValid() &&
         !invalidated_;
}

VoteReceipt AcceptedVote::IssueReceipt() {
  return VoteReceipt(this);
}

void AcceptedVote::UpdateVote(const Vote& vote) {
  DCHECK_EQ(vote_.frame_node(), vote.frame_node());
  DCHECK(vote_.priority() != vote.priority() ||
         vote_.reason() != vote.reason());
  vote_ = vote;
}

void AcceptedVote::SetReceipt(VoteReceipt* receipt) {
  // A receipt can only be set on a vote once in its lifetime.
  DCHECK(!receipt_);
  DCHECK(!invalidated_);
  receipt_ = receipt;

  // The receipt should already be associated with this vote (the association
  // is initiated by the receipt).
  DCHECK(receipt_->HasVote(this));
}

void AcceptedVote::MoveReceipt(VoteReceipt* old_receipt,
                               VoteReceipt* new_receipt) {
  DCHECK(old_receipt);
  DCHECK(new_receipt);
  DCHECK_EQ(receipt_, old_receipt);
  receipt_ = new_receipt;

  // The receipt should already be associated with this vote (its calling for
  // the move).
  DCHECK(receipt_->HasVote(this));
}

VoteReceipt AcceptedVote::ChangeVote(VoteReceipt receipt,
                                     base::TaskPriority priority,
                                     const char* reason) {
  DCHECK_EQ(receipt_, &receipt);
  DCHECK(!invalidated_);
  DCHECK(vote_.priority() != priority || vote_.reason() != reason);

  // Explicitly save a copy of |vote_| as the consumer might overwrite it
  // directly.
  Vote old_vote = vote_;

  // Notify the consumer of the new vote.
  Vote new_vote = Vote(old_vote.frame_node(), priority, reason);
  receipt = consumer_->ChangeVote(std::move(receipt), this, new_vote);

  // Ensure that the returned receipt refers to a vote with the expected
  // properties.
  const Vote& returned_vote = receipt.GetVote();
  DCHECK_EQ(new_vote.frame_node(), returned_vote.frame_node());
  DCHECK_EQ(new_vote.priority(), returned_vote.priority());
  DCHECK_EQ(new_vote.reason(), returned_vote.reason());

  return receipt;
}

void AcceptedVote::InvalidateVote(VoteReceipt* receipt) {
  DCHECK(receipt);
  DCHECK_EQ(receipt_, receipt);
  DCHECK(!invalidated_);

  // Care has to be taken not to access |receipt|, as it is running its
  // destructor. It is only passed as a parameter to ensure that the proper
  // receipt is notifying this vote.
  receipt_ = nullptr;
  invalidated_ = true;
  consumer_->VoteInvalidated(this);
}

void AcceptedVote::Take(AcceptedVote&& rhs) {
  // An AcceptedVote can't be overwritten while it has a pending receipt.
  DCHECK(!receipt_);

  consumer_ = std::exchange(rhs.consumer_, nullptr);
  voter_id_ = std::exchange(rhs.voter_id_, kInvalidVoterId);
  vote_ = std::exchange(rhs.vote_, Vote());
  receipt_ = std::exchange(rhs.receipt_, nullptr);
  invalidated_ = std::exchange(rhs.invalidated_, true);

  // Update the back-pointer from the receipt.
  if (receipt_)
    receipt_->MoveVote(&rhs, this);
}

/////////////////////////////////////////////////////////////////////
// VotingChannel

VotingChannel::VotingChannel() = default;

VotingChannel::VotingChannel(VotingChannel&& rhs) {
  Take(std::move(rhs));
}

VotingChannel& VotingChannel::operator=(VotingChannel&& rhs) {
  Take(std::move(rhs));
  return *this;
}

VotingChannel::~VotingChannel() {
  Reset();
}

VoteReceipt VotingChannel::SubmitVote(const Vote& vote) {
  // Pass the vote along to the consumer with the bound |voter_id_|.
  return factory_->consumer_->SubmitVote(voter_id_, vote);
}

bool VotingChannel::IsValid() const {
  return factory_ && voter_id_ != kInvalidVoterId;
}

void VotingChannel::Reset() {
  if (!factory_)
    return;
  DCHECK_NE(kInvalidVoterId, voter_id_);
  factory_->OnVotingChannelDestroyed();
  factory_ = nullptr;
  voter_id_ = kInvalidVoterId;
}

VotingChannel::VotingChannel(VotingChannelFactory* factory, VoterId voter_id)
    : factory_(factory), voter_id_(voter_id) {}

void VotingChannel::Take(VotingChannel&& rhs) {
  Reset();
  factory_ = std::exchange(rhs.factory_, nullptr);
  voter_id_ = std::exchange(rhs.voter_id_, kInvalidVoterId);
}

/////////////////////////////////////////////////////////////////////
// VotingChannelFactory

VotingChannelFactory::VotingChannelFactory(VoteConsumer* consumer)
    : consumer_(consumer) {
  DCHECK(consumer);
}

VotingChannelFactory::~VotingChannelFactory() {
  // We expect all voters to have severed their VotingChannels before we are
  // torn down.
  DCHECK_EQ(0u, voting_channels_outstanding_);
}

VotingChannel VotingChannelFactory::BuildVotingChannel() {
  ++voting_channels_outstanding_;
  VoterId new_voter_id = ++voting_channels_issued_;
  return VotingChannel(this, new_voter_id);
}

void VotingChannelFactory::OnVotingChannelDestroyed() {
  DCHECK_LT(0u, voting_channels_outstanding_);
  --voting_channels_outstanding_;
}

/////////////////////////////////////////////////////////////////////
// VoteConsumer

VoteConsumer::VoteConsumer() = default;
VoteConsumer::~VoteConsumer() = default;

/////////////////////////////////////////////////////////////////////
// VoteConsumerDefaultImpl

VoteConsumerDefaultImpl::VoteConsumerDefaultImpl() = default;
VoteConsumerDefaultImpl::~VoteConsumerDefaultImpl() = default;

VoteReceipt VoteConsumerDefaultImpl::ChangeVote(VoteReceipt receipt,
                                                AcceptedVote* old_vote,
                                                const Vote& new_vote) {
  // The receipt and vote should be entangled, and the vote should be valid.
  DCHECK(receipt.HasVote(old_vote));
  DCHECK(old_vote->IsValid());

  // Tear down the old vote before submitting a new one in order to prevent
  // the voter from having 2 simultaneous votes for the same frame.
  auto voter_id = receipt.GetVoterId();
  receipt.Reset();
  return SubmitVote(voter_id, new_vote);
}

}  // namespace frame_priority
}  // namespace performance_manager

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/frame_priority.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace frame_priority {
namespace test {

DummyVoteConsumer::DummyVoteConsumer() : voting_channel_factory_(this) {}

DummyVoteConsumer::~DummyVoteConsumer() = default;

VoteReceipt DummyVoteConsumer::SubmitVote(VoterId voter_id, const Vote& vote) {
  // Accept the vote.
  votes_.emplace_back(AcceptedVote(this, voter_id, vote));
  EXPECT_FALSE(votes_.back().HasReceipt());
  EXPECT_TRUE(votes_.back().IsValid());
  ++valid_vote_count_;
  EXPECT_LE(valid_vote_count_, votes_.size());

  // Issue a receipt.
  auto receipt = votes_.back().IssueReceipt();
  EXPECT_TRUE(votes_.back().HasReceipt());
  EXPECT_TRUE(votes_.back().IsValid());
  return receipt;
}

VoteReceipt DummyVoteConsumer::ChangeVote(VoteReceipt receipt,
                                          AcceptedVote* old_vote,
                                          const Vote& new_vote) {
  // We should own this vote and it should be valid.
  EXPECT_TRUE(receipt.HasVote(old_vote));
  EXPECT_LE(votes_.data(), old_vote);
  EXPECT_LT(old_vote, votes_.data() + votes_.size());
  EXPECT_TRUE(old_vote->IsValid());
  EXPECT_LT(0u, valid_vote_count_);

  // Just update the vote in-place, and return the existing receipt for it.
  old_vote->UpdateVote(new_vote);
  return receipt;
}

void DummyVoteConsumer::VoteInvalidated(AcceptedVote* vote) {
  // We should own this vote.
  EXPECT_LE(votes_.data(), vote);
  EXPECT_LT(vote, votes_.data() + votes_.size());
  EXPECT_FALSE(vote->IsValid());
  EXPECT_LT(0u, valid_vote_count_);
  --valid_vote_count_;
}

void DummyVoteConsumer::ExpectInvalidVote(size_t index) {
  EXPECT_LT(index, votes_.size());
  const AcceptedVote& accepted_vote = votes_[index];
  EXPECT_EQ(this, accepted_vote.consumer());
  EXPECT_FALSE(accepted_vote.IsValid());
}

void DummyVoteConsumer::ExpectValidVote(size_t index,
                                        VoterId voter_id,
                                        const FrameNode* frame_node,
                                        base::TaskPriority priority,
                                        const char* reason) {
  EXPECT_LT(index, votes_.size());
  const AcceptedVote& accepted_vote = votes_[index];
  EXPECT_EQ(this, accepted_vote.consumer());
  EXPECT_TRUE(accepted_vote.IsValid());
  EXPECT_EQ(voter_id, accepted_vote.voter_id());
  const Vote& vote = accepted_vote.vote();
  EXPECT_EQ(frame_node, vote.frame_node());
  EXPECT_EQ(priority, vote.priority());
  EXPECT_TRUE(vote.reason());
  if (reason)
    EXPECT_EQ(reason, vote.reason());
}

// static
const char DummyVoter::kReason[] = "dummmy reason";

DummyVoter::DummyVoter() = default;
DummyVoter::~DummyVoter() = default;

void DummyVoter::SetVotingChannel(VotingChannel&& voting_channel) {
  voting_channel_ = std::move(voting_channel);
}

void DummyVoter::EmitVote(const FrameNode* frame_node,
                          base::TaskPriority priority,
                          const char* reason) {
  EXPECT_TRUE(voting_channel_.IsValid());
  receipts_.emplace_back(
      voting_channel_.SubmitVote(Vote(frame_node, priority, reason)));
}

}  // namespace test
}  // namespace frame_priority
}  // namespace performance_manager

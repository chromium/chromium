// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/frame_priority/frame_priority.h"

#include "components/performance_manager/test_support/frame_priority.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace frame_priority {

namespace {

// Some dummy frames.
const FrameNode* kDummyFrame1 = reinterpret_cast<const FrameNode*>(0xDEADBEEF);
const FrameNode* kDummyFrame2 = reinterpret_cast<const FrameNode*>(0xBAADF00D);

void ExpectEntangled(const VoteReceipt& receipt, const AcceptedVote& vote) {
  EXPECT_TRUE(receipt.HasVote(&vote));
  EXPECT_TRUE(vote.HasReceipt(&receipt));
  EXPECT_TRUE(vote.IsValid());
}

void ExpectNotEntangled(const VoteReceipt& receipt, const AcceptedVote& vote) {
  EXPECT_FALSE(receipt.HasVote());
  EXPECT_FALSE(vote.HasReceipt());
  EXPECT_FALSE(vote.IsValid());
}

static const char kReason1[] = "reason1";
static const char kReason2[] = "reason2";
static const char kReason3[] = "reason1";  // Equal to kReason1 on purpose!

}  // namespace

TEST(FramePriorityTest, ReasonCompare) {
  // Comparison with nullptr.
  EXPECT_GT(0, ReasonCompare(nullptr, kReason1));
  EXPECT_EQ(0, ReasonCompare(nullptr, nullptr));
  EXPECT_LT(0, ReasonCompare(kReason1, nullptr));

  // Comparisons where the addresses and string content are different.
  EXPECT_GT(0, ReasonCompare(kReason1, kReason2));
  EXPECT_LT(0, ReasonCompare(kReason2, kReason1));

  // Comparison with identical addresses.
  EXPECT_EQ(0, ReasonCompare(kReason1, kReason1));

  // Comparison where the addresses are different, but string content is the
  // same.
  EXPECT_EQ(0, ReasonCompare(kReason1, kReason3));
}

TEST(FramePriorityTest, PriorityAndReason) {
  // Default constructor
  PriorityAndReason par1;
  EXPECT_EQ(base::TaskPriority::LOWEST, par1.priority());
  EXPECT_EQ(nullptr, par1.reason());

  // Explicit initialization.
  PriorityAndReason par2(base::TaskPriority::HIGHEST, kReason1);
  EXPECT_EQ(base::TaskPriority::HIGHEST, par2.priority());
  EXPECT_EQ(kReason1, par2.reason());

  // Identical comparison.
  EXPECT_TRUE(par1 == par1);
  EXPECT_FALSE(par1 != par1);
  EXPECT_TRUE(par1 <= par1);
  EXPECT_TRUE(par1 >= par1);
  EXPECT_FALSE(par1 < par1);
  EXPECT_FALSE(par1 > par1);

  // Comparison with distinct priorities.
  EXPECT_FALSE(par1 == par2);
  EXPECT_TRUE(par1 != par2);
  EXPECT_TRUE(par1 <= par2);
  EXPECT_FALSE(par1 >= par2);
  EXPECT_TRUE(par1 < par2);
  EXPECT_FALSE(par1 > par2);

  // Comparison with identical priorities and reasons strings, but at different
  // locations.
  PriorityAndReason par3(base::TaskPriority::HIGHEST, kReason3);
  EXPECT_EQ(base::TaskPriority::HIGHEST, par3.priority());
  EXPECT_EQ(kReason3, par3.reason());
  EXPECT_TRUE(par2 == par3);
  EXPECT_FALSE(par2 != par3);
  EXPECT_TRUE(par2 <= par3);
  EXPECT_TRUE(par2 >= par3);
  EXPECT_FALSE(par2 < par3);
  EXPECT_FALSE(par2 > par3);

  // Comparison with identical priorities, and different reason strings.
  PriorityAndReason par4(base::TaskPriority::LOWEST, kReason2);
  EXPECT_FALSE(par1 == par4);
  EXPECT_TRUE(par1 != par4);
  EXPECT_TRUE(par1 <= par4);
  EXPECT_FALSE(par1 >= par4);
  EXPECT_TRUE(par1 < par4);
  EXPECT_FALSE(par1 > par4);

  // Copy constructor.
  PriorityAndReason par5(par3);
  EXPECT_EQ(base::TaskPriority::HIGHEST, par5.priority());
  EXPECT_EQ(kReason3, par5.reason());

  // Assignment.
  par1 = par3;
  EXPECT_EQ(base::TaskPriority::HIGHEST, par1.priority());
  EXPECT_EQ(kReason3, par1.reason());
}

TEST(FramePriorityTest, DefaultAcceptedVoteIsInvalid) {
  AcceptedVote vote;
  EXPECT_FALSE(vote.IsValid());
}

TEST(FramePriorityTest, VoteReceiptsWork) {
  test::DummyVoteConsumer consumer;
  test::DummyVoter voter;

  EXPECT_FALSE(voter.voting_channel_.IsValid());
  voter.SetVotingChannel(consumer.voting_channel_factory_.BuildVotingChannel());
  EXPECT_EQ(&consumer.voting_channel_factory_,
            voter.voting_channel_.factory_for_testing());
  EXPECT_NE(kInvalidVoterId, voter.voting_channel_.voter_id());
  EXPECT_TRUE(voter.voting_channel_.IsValid());

  voter.EmitVote(kDummyFrame1);
  EXPECT_EQ(1u, voter.receipts_.size());
  EXPECT_EQ(1u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  EXPECT_EQ(voter.voting_channel_.voter_id(), consumer.votes_[0].voter_id());
  EXPECT_EQ(kDummyFrame1, consumer.votes_[0].vote().frame_node());
  EXPECT_TRUE(consumer.votes_[0].IsValid());
  ExpectEntangled(voter.receipts_[0], consumer.votes_[0]);

  // Move the vote and the receipt out of their containers and back in.
  // All should be well.
  {
    VoteReceipt receipt = std::move(voter.receipts_[0]);
    EXPECT_FALSE(voter.receipts_[0].HasVote());
    ExpectEntangled(receipt, consumer.votes_[0]);

    AcceptedVote vote = std::move(consumer.votes_[0]);
    EXPECT_FALSE(consumer.votes_[0].IsValid());
    ExpectEntangled(receipt, vote);

    voter.receipts_[0] = std::move(receipt);
    EXPECT_FALSE(receipt.HasVote());
    ExpectEntangled(voter.receipts_[0], vote);

    consumer.votes_[0] = std::move(vote);
    EXPECT_FALSE(vote.IsValid());
    ExpectEntangled(voter.receipts_[0], consumer.votes_[0]);
  }

  voter.EmitVote(kDummyFrame2);
  EXPECT_EQ(2u, voter.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  EXPECT_EQ(kDummyFrame1, consumer.votes_[0].vote().frame_node());
  EXPECT_EQ(kDummyFrame2, consumer.votes_[1].vote().frame_node());
  ExpectEntangled(voter.receipts_[0], consumer.votes_[0]);
  ExpectEntangled(voter.receipts_[1], consumer.votes_[1]);

  // Change a vote, but making no change.
  ExpectEntangled(voter.receipts_[0], consumer.votes_[0]);
  EXPECT_EQ(kDummyFrame1, consumer.votes_[0].vote().frame_node());
  EXPECT_EQ(base::TaskPriority::LOWEST, consumer.votes_[0].vote().priority());
  EXPECT_EQ(test::DummyVoter::kReason, consumer.votes_[0].vote().reason());
  voter.receipts_[0].ChangeVote(base::TaskPriority::LOWEST,
                                test::DummyVoter::kReason);
  ExpectEntangled(voter.receipts_[0], consumer.votes_[0]);
  EXPECT_EQ(kDummyFrame1, consumer.votes_[0].vote().frame_node());
  EXPECT_EQ(base::TaskPriority::LOWEST, consumer.votes_[0].vote().priority());
  EXPECT_EQ(test::DummyVoter::kReason, consumer.votes_[0].vote().reason());

  // Change the vote and expect the change to propagate.
  static const char kReason[] = "another reason";
  voter.receipts_[0].ChangeVote(base::TaskPriority::HIGHEST, kReason);
  ExpectEntangled(voter.receipts_[0], consumer.votes_[0]);
  EXPECT_EQ(kDummyFrame1, consumer.votes_[0].vote().frame_node());
  EXPECT_EQ(base::TaskPriority::HIGHEST, consumer.votes_[0].vote().priority());
  EXPECT_EQ(kReason, consumer.votes_[0].vote().reason());

  // Cancel a vote.
  voter.receipts_[0].Reset();
  EXPECT_EQ(2u, voter.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  EXPECT_EQ(kDummyFrame1, consumer.votes_[0].vote().frame_node());
  EXPECT_EQ(kDummyFrame2, consumer.votes_[1].vote().frame_node());
  ExpectNotEntangled(voter.receipts_[0], consumer.votes_[0]);
  ExpectEntangled(voter.receipts_[1], consumer.votes_[1]);

  // Cause the votes to be moved by deleting the invalid one.
  consumer.votes_.erase(consumer.votes_.begin());
  EXPECT_EQ(2u, voter.receipts_.size());
  EXPECT_EQ(1u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  EXPECT_EQ(kDummyFrame2, consumer.votes_[0].vote().frame_node());
  EXPECT_FALSE(voter.receipts_[0].HasVote());
  ExpectEntangled(voter.receipts_[1], consumer.votes_[0]);

  // Cause the receipts to be moved by deleting the empty one.
  voter.receipts_.erase(voter.receipts_.begin());
  EXPECT_EQ(1u, voter.receipts_.size());
  EXPECT_EQ(1u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  EXPECT_EQ(kDummyFrame2, consumer.votes_[0].vote().frame_node());
  ExpectEntangled(voter.receipts_[0], consumer.votes_[0]);

  // Cancel the remaining vote by deleting the receipt.
  voter.receipts_.clear();
  EXPECT_EQ(0u, voter.receipts_.size());
  EXPECT_EQ(1u, consumer.votes_.size());
  EXPECT_EQ(0u, consumer.valid_vote_count_);
  EXPECT_EQ(kDummyFrame2, consumer.votes_[0].vote().frame_node());
  EXPECT_FALSE(consumer.votes_[0].HasReceipt());
  EXPECT_FALSE(consumer.votes_[0].IsValid());
}

}  // namespace frame_priority
}  // namespace performance_manager

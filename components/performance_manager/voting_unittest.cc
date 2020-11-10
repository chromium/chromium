// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/voting/voting.h"

#include "components/performance_manager/test_support/voting.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

using TestVote = voting::Vote<void, int, 0>;
using TestVoteReceipt = voting::VoteReceipt<TestVote>;
using TestVotingChannel = voting::VotingChannel<TestVote>;
using TestVotingChannelFactory = voting::VotingChannelFactory<TestVote>;
using TestVoteConsumer = voting::VoteConsumer<TestVote>;
using TestAcceptedVote = voting::AcceptedVote<TestVote>;

using DummyVoter = voting::test::DummyVoter<TestVote>;
using DummyVoteConsumer = voting::test::DummyVoteConsumer<TestVote>;
using DummyVoteObserver = voting::test::DummyVoteObserver<TestVote>;

// Some dummy contexts.
const void* kDummyContext1 = reinterpret_cast<const void*>(0xDEADBEEF);
const void* kDummyContext2 = reinterpret_cast<const void*>(0xBAADF00D);

AssertionResult IsEntangled(const TestVoteReceipt& receipt,
                            const TestAcceptedVote& vote) {
  if (!receipt.HasVote(&vote))
    return AssertionFailure() << "Receipt has wrong vote";
  if (!vote.HasReceipt(&receipt))
    return AssertionFailure() << "Vote has wrong receipt";
  if (!vote.IsValid())
    return AssertionFailure() << "Vote is not valid";
  return AssertionSuccess();
}

AssertionResult IsNotEntangled(const TestVoteReceipt& receipt,
                               const TestAcceptedVote& vote) {
  if (receipt.HasVote(&vote))
    return AssertionFailure() << "Receipt has unexpected vote";
  if (vote.HasReceipt(&receipt))
    return AssertionFailure() << "Vote has unexpected receipt";
  if (vote.IsValid())
    return AssertionFailure() << "Vote is unexpectedly valid";
  return AssertionSuccess();
}

static const char kReason[] = "reason";

}  // namespace

TEST(VotingTest, DefaultAcceptedVoteIsInvalid) {
  TestAcceptedVote vote;
  EXPECT_FALSE(vote.IsValid());
}

TEST(VotingTest, VoteReceiptsWork) {
  DummyVoteConsumer consumer;
  DummyVoter voter;

  EXPECT_FALSE(voter.voting_channel_.IsValid());
  voter.SetVotingChannel(consumer.voting_channel_factory_.BuildVotingChannel());
  EXPECT_EQ(&consumer.voting_channel_factory_,
            voter.voting_channel_.factory_for_testing());
  EXPECT_NE(voting::kInvalidVoterId<TestVote>,
            voter.voting_channel_.voter_id());
  EXPECT_TRUE(voter.voting_channel_.IsValid());

  voter.EmitVote(kDummyContext1, 0);
  EXPECT_EQ(1u, voter.receipts_.size());
  EXPECT_EQ(1u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  EXPECT_EQ(voter.voting_channel_.voter_id(), consumer.votes_[0].voter_id());
  EXPECT_EQ(kDummyContext1, consumer.votes_[0].context());
  EXPECT_TRUE(consumer.votes_[0].IsValid());
  EXPECT_TRUE(IsEntangled(voter.receipts_[0], consumer.votes_[0]));

  // Move the vote and the receipt out of their containers and back in.
  // All should be well.
  {
    TestVoteReceipt receipt = std::move(voter.receipts_[0]);
    EXPECT_FALSE(voter.receipts_[0].HasVote());
    EXPECT_TRUE(IsEntangled(receipt, consumer.votes_[0]));

    TestAcceptedVote vote = std::move(consumer.votes_[0]);
    EXPECT_FALSE(consumer.votes_[0].IsValid());
    EXPECT_TRUE(IsEntangled(receipt, vote));

    voter.receipts_[0] = std::move(receipt);
    EXPECT_FALSE(receipt.HasVote());
    EXPECT_TRUE(IsEntangled(voter.receipts_[0], vote));

    consumer.votes_[0] = std::move(vote);
    EXPECT_FALSE(vote.IsValid());
    EXPECT_TRUE(IsEntangled(voter.receipts_[0], consumer.votes_[0]));
  }

  voter.EmitVote(kDummyContext2, 0);
  EXPECT_EQ(2u, voter.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  EXPECT_EQ(kDummyContext1, consumer.votes_[0].context());
  EXPECT_EQ(kDummyContext2, consumer.votes_[1].context());
  EXPECT_TRUE(IsEntangled(voter.receipts_[0], consumer.votes_[0]));
  EXPECT_TRUE(IsEntangled(voter.receipts_[1], consumer.votes_[1]));

  // Change a vote, but making no change.
  EXPECT_TRUE(IsEntangled(voter.receipts_[0], consumer.votes_[0]));
  EXPECT_EQ(kDummyContext1, consumer.votes_[0].context());
  EXPECT_EQ(0, consumer.votes_[0].vote().value());
  EXPECT_EQ(DummyVoter::kReason, consumer.votes_[0].vote().reason());
  voter.receipts_[0].ChangeVote(0, DummyVoter::kReason);
  EXPECT_TRUE(IsEntangled(voter.receipts_[0], consumer.votes_[0]));
  EXPECT_EQ(kDummyContext1, consumer.votes_[0].context());
  EXPECT_EQ(0, consumer.votes_[0].vote().value());
  EXPECT_EQ(DummyVoter::kReason, consumer.votes_[0].vote().reason());

  // Change the vote and expect the change to propagate.
  static const char kReason[] = "another reason";
  voter.receipts_[0].ChangeVote(5, kReason);
  EXPECT_TRUE(IsEntangled(voter.receipts_[0], consumer.votes_[0]));
  EXPECT_EQ(kDummyContext1, consumer.votes_[0].context());
  EXPECT_EQ(5, consumer.votes_[0].vote().value());
  EXPECT_EQ(kReason, consumer.votes_[0].vote().reason());

  // Cancel a vote.
  voter.receipts_[0].Reset();
  EXPECT_EQ(2u, voter.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  EXPECT_EQ(kDummyContext1, consumer.votes_[0].context());
  EXPECT_EQ(kDummyContext2, consumer.votes_[1].context());
  EXPECT_TRUE(IsNotEntangled(voter.receipts_[0], consumer.votes_[0]));
  EXPECT_TRUE(IsEntangled(voter.receipts_[1], consumer.votes_[1]));

  // Cause the votes to be moved by deleting the invalid one.
  consumer.votes_.erase(consumer.votes_.begin());
  EXPECT_EQ(2u, voter.receipts_.size());
  EXPECT_EQ(1u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  EXPECT_EQ(kDummyContext2, consumer.votes_[0].context());
  EXPECT_FALSE(voter.receipts_[0].HasVote());
  EXPECT_TRUE(IsEntangled(voter.receipts_[1], consumer.votes_[0]));

  // Cause the receipts to be moved by deleting the empty one.
  voter.receipts_.erase(voter.receipts_.begin());
  EXPECT_EQ(1u, voter.receipts_.size());
  EXPECT_EQ(1u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  EXPECT_EQ(kDummyContext2, consumer.votes_[0].context());
  EXPECT_TRUE(IsEntangled(voter.receipts_[0], consumer.votes_[0]));

  // Cancel the remaining vote by deleting the receipt.
  voter.receipts_.clear();
  EXPECT_EQ(0u, voter.receipts_.size());
  EXPECT_EQ(1u, consumer.votes_.size());
  EXPECT_EQ(0u, consumer.valid_vote_count_);
  EXPECT_EQ(kDummyContext2, consumer.votes_[0].context());
  EXPECT_FALSE(consumer.votes_[0].HasReceipt());
  EXPECT_FALSE(consumer.votes_[0].IsValid());
}

// Tests that an overwritten vote receipt will property clean up its state.
TEST(VotingTest, OverwriteVoteReceipt) {
  DummyVoteConsumer consumer;

  TestVotingChannel voting_channel =
      consumer.voting_channel_factory_.BuildVotingChannel();

  TestVoteReceipt receipt =
      voting_channel.SubmitVote(kDummyContext1, TestVote(5, kReason));
  receipt = voting_channel.SubmitVote(kDummyContext2, TestVote(5, kReason));

  // The first vote was invalidated because its vote receipt was cleaned up.
  consumer.ExpectInvalidVote(0);
}

TEST(VotingTest, VoteObserver) {
  DummyVoteObserver observer;

  TestVotingChannel voting_channel =
      observer.vote_consumer_default_impl_.BuildVotingChannel();
  voting::VoterId<TestVote> voter_id = voting_channel.voter_id();

  {
    TestVoteReceipt receipt =
        voting_channel.SubmitVote(kDummyContext1, TestVote(5, kReason));
    EXPECT_TRUE(observer.HasVote(voter_id, kDummyContext1, 5, kReason));
  }

  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext1, 5, kReason));
}

}  // namespace performance_manager

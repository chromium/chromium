// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/voting/voting.h"

#include "base/test/gtest_util.h"
#include "components/performance_manager/test_support/voting.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

using TestVote = voting::Vote<void, int, 0>;
using TestVotingChannel = voting::VotingChannel<TestVote>;
using TestVotingChannelFactory = voting::VotingChannelFactory<TestVote>;

using DummyVoteObserver = voting::test::DummyVoteObserver<TestVote>;

// Some dummy contexts.
const void* kDummyContext1 = reinterpret_cast<const void*>(0xDEADBEEF);
const void* kDummyContext2 = reinterpret_cast<const void*>(0xBAADF00D);

static const char kReason[] = "reason";

}  // namespace

TEST(VotingTest, SimpleVoter) {
  DummyVoteObserver observer;

  TestVotingChannel voting_channel = observer.BuildVotingChannel();
  voting::VoterId<TestVote> voter_id = voting_channel.voter_id();

  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext1));

  voting_channel.SubmitVote(kDummyContext1, TestVote(5, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id, kDummyContext1, 5, kReason));

  voting_channel.ChangeVote(kDummyContext1, TestVote(10, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id, kDummyContext1, 10, kReason));

  voting_channel.InvalidateVote(kDummyContext1);
  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext1));
}

// Tests that an observer can receive votes for different contexts from the same
// voting channel.
TEST(VotingTest, OneVoterMultipleContexts) {
  DummyVoteObserver observer;

  TestVotingChannel voting_channel = observer.BuildVotingChannel();
  voting::VoterId<TestVote> voter_id = voting_channel.voter_id();

  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext1));

  voting_channel.SubmitVote(kDummyContext1, TestVote(5, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id, kDummyContext1, 5, kReason));
  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext2));

  voting_channel.SubmitVote(kDummyContext2, TestVote(100, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id, kDummyContext1, 5, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id, kDummyContext2, 100, kReason));

  voting_channel.ChangeVote(kDummyContext1, TestVote(10, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id, kDummyContext1, 10, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id, kDummyContext2, 100, kReason));

  voting_channel.InvalidateVote(kDummyContext1);
  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext1));
  EXPECT_TRUE(observer.HasVote(voter_id, kDummyContext2, 100, kReason));

  voting_channel.InvalidateVote(kDummyContext2);
  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext1));
  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext2));
}

// Tests that an observer can receive votes from more than one voting channel.
TEST(VotingTest, TwoVoter) {
  DummyVoteObserver observer;

  TestVotingChannel voting_channel_1 = observer.BuildVotingChannel();
  voting::VoterId<TestVote> voter_id_1 = voting_channel_1.voter_id();

  TestVotingChannel voting_channel_2 = observer.BuildVotingChannel();
  voting::VoterId<TestVote> voter_id_2 = voting_channel_2.voter_id();

  EXPECT_FALSE(observer.HasVote(voter_id_1, kDummyContext1));
  EXPECT_FALSE(observer.HasVote(voter_id_2, kDummyContext1));

  voting_channel_1.SubmitVote(kDummyContext1, TestVote(5, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id_1, kDummyContext1, 5, kReason));
  EXPECT_FALSE(observer.HasVote(voter_id_2, kDummyContext1));

  voting_channel_2.SubmitVote(kDummyContext1, TestVote(5, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id_1, kDummyContext1, 5, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id_2, kDummyContext1, 5, kReason));

  voting_channel_1.ChangeVote(kDummyContext1, TestVote(10, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id_1, kDummyContext1, 10, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id_2, kDummyContext1, 5, kReason));

  voting_channel_2.ChangeVote(kDummyContext1, TestVote(10, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id_1, kDummyContext1, 10, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id_2, kDummyContext1, 10, kReason));

  voting_channel_1.InvalidateVote(kDummyContext1);
  EXPECT_FALSE(observer.HasVote(voter_id_1, kDummyContext1));
  EXPECT_TRUE(observer.HasVote(voter_id_2, kDummyContext1, 10, kReason));

  voting_channel_2.InvalidateVote(kDummyContext1);
  EXPECT_FALSE(observer.HasVote(voter_id_1, kDummyContext1));
  EXPECT_FALSE(observer.HasVote(voter_id_2, kDummyContext1));
}

TEST(VotingTest, ResetVotingChannel) {
  DummyVoteObserver observer;

  TestVotingChannel voting_channel = observer.BuildVotingChannel();
  EXPECT_TRUE(voting_channel.IsValid());

  voting_channel.Reset();

  EXPECT_FALSE(voting_channel.IsValid());
}

// Tests that VotingChannel supports move sementics.
TEST(VotingTest, MoveVotingChannel) {
  DummyVoteObserver observer;

  // Build the voting channel.
  TestVotingChannel voting_channel_1 = observer.BuildVotingChannel();
  voting::VoterId<TestVote> voter_id = voting_channel_1.voter_id();
  EXPECT_TRUE(voting_channel_1.IsValid());

  // Cast a vote with that voting channel.
  voting_channel_1.SubmitVote(kDummyContext1, TestVote(5, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id, kDummyContext1, 5, kReason));

  // Move the voting channel.
  TestVotingChannel voting_channel_2 = std::move(voting_channel_1);
  EXPECT_TRUE(voting_channel_2.IsValid());

  // Use the second variable to change the vote.
  voting_channel_2.ChangeVote(kDummyContext1, TestVote(10, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id, kDummyContext1, 10, kReason));

  // Move the voting channel back using the move assignment operator.
  voting_channel_1 = std::move(voting_channel_2);
  EXPECT_TRUE(voting_channel_1.IsValid());

  // Invalidate the vote.
  voting_channel_1.InvalidateVote(kDummyContext1);
  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext1));
}

// Tests that submitting 2 votes for the same context using a VotingChannel
// results in a DCHECK.
TEST(VotingTest, SubmitDuplicateVote) {
  DummyVoteObserver observer;

  TestVotingChannel voting_channel = observer.BuildVotingChannel();
  voting::VoterId<TestVote> voter_id = voting_channel.voter_id();

  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext1));

  voting_channel.SubmitVote(kDummyContext1, TestVote(5, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id, kDummyContext1, 5, kReason));

  EXPECT_DCHECK_DEATH(
      voting_channel.SubmitVote(kDummyContext1, TestVote(10, kReason)));

  // Clean up.
  voting_channel.InvalidateVote(kDummyContext1);
}

// Tests that calling ChangeVote() for a context before a vote was submitted for
// that context results in a DCHECK.
TEST(VotingTest, ChangeNonExisting) {
  DummyVoteObserver observer;

  TestVotingChannel voting_channel = observer.BuildVotingChannel();
  voting::VoterId<TestVote> voter_id = voting_channel.voter_id();

  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext1));
  EXPECT_DCHECK_DEATH(
      voting_channel.ChangeVote(kDummyContext1, TestVote(5, kReason)));
}

// Tests that calling InvalidateVote() for a context before a vote was submitted
// for that context results in a DCHECK.
TEST(VotingTest, InvalidateNonExisting) {
  DummyVoteObserver observer;

  TestVotingChannel voting_channel = observer.BuildVotingChannel();
  voting::VoterId<TestVote> voter_id = voting_channel.voter_id();

  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext1));
  EXPECT_DCHECK_DEATH(voting_channel.InvalidateVote(kDummyContext1));
}

// Tests that destroying a VotingChannelFactory before all of its VotingChannels
// results in a DCHECK.
TEST(VotingTest, DestroyFactoryBeforeChannel) {
  TestVotingChannel voting_channel;

  auto observer = std::make_unique<DummyVoteObserver>();

  voting_channel = observer->BuildVotingChannel();

  EXPECT_DCHECK_DEATH(observer.reset());

  // Clean up.
  voting_channel.Reset();
}

}  // namespace performance_manager

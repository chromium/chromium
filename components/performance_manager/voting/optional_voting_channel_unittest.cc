// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/voting/optional_voting_channel.h"

#include "components/performance_manager/test_support/voting.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using TestVote = voting::Vote<void, int, 0>;
using TestVotingChannel = voting::VotingChannel<TestVote>;
using TestVotingChannelFactory = voting::VotingChannelFactory<TestVote>;
using TestOptionalVotingChannel = voting::OptionalVotingChannel<TestVote>;

using DummyVoteObserver = voting::test::DummyVoteObserver<TestVote>;

// Some dummy contexts.
const void* kDummyContext1 = reinterpret_cast<const void*>(0xDEADBEEF);

static const char kReason[] = "reason";

}  // namespace

TEST(OptionalVotingChannelTest, UpstreamingWorks) {
  DummyVoteObserver observer;

  TestOptionalVotingChannel voting_channel(observer.BuildVotingChannel());
  voting::VoterId<TestVote> voter_id = voting_channel.voter_id();

  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext1));

  voting_channel.SubmitVote(kDummyContext1, TestVote(0, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id, kDummyContext1));

  voting_channel.ChangeVote(kDummyContext1, TestVote(1, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id, kDummyContext1, 1, kReason));

  voting_channel.ChangeVote(kDummyContext1, TestVote(0, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id, kDummyContext1));

  voting_channel.InvalidateVote(kDummyContext1);
  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext1));
}

TEST(OptionalVotingChannelTest, OptionalVoteNotUpstreamed) {
  DummyVoteObserver observer;

  TestOptionalVotingChannel voting_channel(observer.BuildVotingChannel());
  voting::VoterId<TestVote> voter_id = voting_channel.voter_id();

  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext1));

  voting_channel.SubmitVote(kDummyContext1, std::nullopt);
  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext1));

  voting_channel.ChangeVote(kDummyContext1, TestVote(1, kReason));
  EXPECT_TRUE(observer.HasVote(voter_id, kDummyContext1, 1, kReason));

  voting_channel.ChangeVote(kDummyContext1, std::nullopt);
  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext1));

  voting_channel.InvalidateVote(kDummyContext1);
  EXPECT_FALSE(observer.HasVote(voter_id, kDummyContext1));
}

}  // namespace performance_manager

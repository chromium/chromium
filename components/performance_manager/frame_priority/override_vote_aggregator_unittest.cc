// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/frame_priority/override_vote_aggregator.h"

#include "components/performance_manager/test_support/frame_priority.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace frame_priority {

// Some dummy frames.
const FrameNode* kDummyFrame1 = reinterpret_cast<const FrameNode*>(0xDEADBEEF);
const FrameNode* kDummyFrame2 = reinterpret_cast<const FrameNode*>(0xBAADF00D);

TEST(OverrideVoteAggregatorTest, BlackboxTest) {
  // Priorities to use for default and override votes. The override priority is
  // lower on purpose.
  static constexpr base::TaskPriority kDefaultPriority =
      base::TaskPriority::HIGHEST;
  static constexpr base::TaskPriority kOverridePriority =
      base::TaskPriority::LOWEST;

  static const char kReason[] = "another reason";

  // Builds the small hierarchy of voters as follows:
  //
  //        consumer
  //           |
  //          agg
  //         /   \
  //        /     \
  //   voter0     voter1
  // (override) (default)
  test::DummyVoteConsumer consumer;
  OverrideVoteAggregator agg;
  test::DummyVoter voter0;
  test::DummyVoter voter1;

  VoterId agg_id = kInvalidVoterId;
  {
    auto channel = consumer.voting_channel_factory_.BuildVotingChannel();
    agg_id = channel.voter_id();
    agg.SetUpstreamVotingChannel(std::move(channel));
  }

  voter0.SetVotingChannel(agg.GetOverrideVotingChannel());
  voter1.SetVotingChannel(agg.GetDefaultVotingChannel());
  EXPECT_TRUE(agg.IsSetup());

  // Submitting a default vote should immediately propagate to the consumer.
  voter1.EmitVote(kDummyFrame1, kDefaultPriority);
  EXPECT_EQ(0u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(1u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kDummyFrame1, kDefaultPriority,
                           test::DummyVoter::kReason);

  // Canceling the default vote should clear all votes.
  voter1.receipts_.clear();
  EXPECT_EQ(0u, voter0.receipts_.size());
  EXPECT_EQ(0u, voter1.receipts_.size());
  EXPECT_EQ(0u, agg.GetSizeForTesting());
  EXPECT_EQ(1u, consumer.votes_.size());
  EXPECT_EQ(0u, consumer.valid_vote_count_);

  // Resubmitting the default vote should propagate to the consumer.
  voter1.EmitVote(kDummyFrame1, kDefaultPriority);
  EXPECT_EQ(0u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyFrame1, kDefaultPriority,
                           test::DummyVoter::kReason);

  // Submitting an override vote should override it and propagate to the
  // consumer. This should update the existing vote in place.
  voter0.EmitVote(kDummyFrame1, kOverridePriority);
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyFrame1, kOverridePriority,
                           test::DummyVoter::kReason);

  // Canceling the override vote should drop back to using the default vote.
  // This will again reuse the existing upstream vote.
  voter0.receipts_.clear();
  EXPECT_EQ(0u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyFrame1, kDefaultPriority,
                           test::DummyVoter::kReason);

  // Changing the default vote should propagate, as there's no override vote.
  voter1.receipts_[0].ChangeVote(kDefaultPriority, kReason);
  EXPECT_EQ(0u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyFrame1, kDefaultPriority, kReason);

  // Changing back should also propagate.
  voter1.receipts_[0].ChangeVote(kDefaultPriority, test::DummyVoter::kReason);
  EXPECT_EQ(0u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyFrame1, kDefaultPriority,
                           test::DummyVoter::kReason);

  // Submitting an override vote should override it and propagate to the
  // consumer.
  voter0.EmitVote(kDummyFrame1, kOverridePriority);
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyFrame1, kOverridePriority,
                           test::DummyVoter::kReason);

  // Canceling the default vote should do nothing.
  voter1.receipts_.clear();
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(0u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyFrame1, kOverridePriority,
                           test::DummyVoter::kReason);

  // Submitting another default vote should do nothing.
  voter1.EmitVote(kDummyFrame1, kDefaultPriority);
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyFrame1, kOverridePriority,
                           test::DummyVoter::kReason);

  // Changing the default vote should do nothing.
  voter1.receipts_.back().ChangeVote(kDefaultPriority, kReason);
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyFrame1, kOverridePriority,
                           test::DummyVoter::kReason);

  // Changing the override vote should change the upstream vote.
  voter0.receipts_.back().ChangeVote(kOverridePriority, kReason);
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyFrame1, kOverridePriority, kReason);

  // Canceling the default vote should do nothing.
  voter1.receipts_.clear();
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(0u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyFrame1, kOverridePriority, kReason);

  // Finally, canceling the override vote should cancel the upstream vote.
  voter0.receipts_.clear();
  EXPECT_EQ(0u, voter0.receipts_.size());
  EXPECT_EQ(0u, voter1.receipts_.size());
  EXPECT_EQ(0u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(0u, consumer.valid_vote_count_);
}

}  // namespace frame_priority
}  // namespace performance_manager

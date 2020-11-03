// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/execution_context_priority/override_vote_aggregator.h"

#include "components/performance_manager/test_support/voting.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace execution_context_priority {

using DummyVoter = voting::test::DummyVoter<Vote>;
using DummyVoteConsumer = voting::test::DummyVoteConsumer<Vote>;

// Some dummy execution contexts.
const ExecutionContext* kDummyExecutionContext =
    reinterpret_cast<const ExecutionContext*>(0xDEADBEEF);

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
  DummyVoteConsumer consumer;
  OverrideVoteAggregator agg;
  DummyVoter voter0;
  DummyVoter voter1;

  voting::VoterId<Vote> agg_id = voting::kInvalidVoterId<Vote>;
  {
    auto channel = consumer.voting_channel_factory_.BuildVotingChannel();
    agg_id = channel.voter_id();
    agg.SetUpstreamVotingChannel(std::move(channel));
  }

  voter0.SetVotingChannel(agg.GetOverrideVotingChannel());
  voter1.SetVotingChannel(agg.GetDefaultVotingChannel());
  EXPECT_TRUE(agg.IsSetup());

  // Submitting a default vote should immediately propagate to the consumer.
  voter1.EmitVote(kDummyExecutionContext, kDefaultPriority);
  EXPECT_EQ(0u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(1u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kDummyExecutionContext, kDefaultPriority,
                           DummyVoter::kReason);

  // Canceling the default vote should clear all votes.
  voter1.receipts_.clear();
  EXPECT_EQ(0u, voter0.receipts_.size());
  EXPECT_EQ(0u, voter1.receipts_.size());
  EXPECT_EQ(0u, agg.GetSizeForTesting());
  EXPECT_EQ(1u, consumer.votes_.size());
  EXPECT_EQ(0u, consumer.valid_vote_count_);

  // Resubmitting the default vote should propagate to the consumer.
  voter1.EmitVote(kDummyExecutionContext, kDefaultPriority);
  EXPECT_EQ(0u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyExecutionContext, kDefaultPriority,
                           DummyVoter::kReason);

  // Submitting an override vote should override it and propagate to the
  // consumer. This should update the existing vote in place.
  voter0.EmitVote(kDummyExecutionContext, kOverridePriority);
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyExecutionContext, kOverridePriority,
                           DummyVoter::kReason);

  // Canceling the override vote should drop back to using the default vote.
  // This will again reuse the existing upstream vote.
  voter0.receipts_.clear();
  EXPECT_EQ(0u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyExecutionContext, kDefaultPriority,
                           DummyVoter::kReason);

  // Changing the default vote should propagate, as there's no override vote.
  voter1.receipts_[0].ChangeVote(kDefaultPriority, kReason);
  EXPECT_EQ(0u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyExecutionContext, kDefaultPriority,
                           kReason);

  // Changing back should also propagate.
  voter1.receipts_[0].ChangeVote(kDefaultPriority, DummyVoter::kReason);
  EXPECT_EQ(0u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyExecutionContext, kDefaultPriority,
                           DummyVoter::kReason);

  // Submitting an override vote should override it and propagate to the
  // consumer.
  voter0.EmitVote(kDummyExecutionContext, kOverridePriority);
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyExecutionContext, kOverridePriority,
                           DummyVoter::kReason);

  // Canceling the default vote should do nothing.
  voter1.receipts_.clear();
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(0u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyExecutionContext, kOverridePriority,
                           DummyVoter::kReason);

  // Submitting another default vote should do nothing.
  voter1.EmitVote(kDummyExecutionContext, kDefaultPriority);
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyExecutionContext, kOverridePriority,
                           DummyVoter::kReason);

  // Changing the default vote should do nothing.
  voter1.receipts_.back().ChangeVote(kDefaultPriority, kReason);
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyExecutionContext, kOverridePriority,
                           DummyVoter::kReason);

  // Changing the override vote should change the upstream vote.
  voter0.receipts_.back().ChangeVote(kOverridePriority, kReason);
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyExecutionContext, kOverridePriority,
                           kReason);

  // Canceling the default vote should do nothing.
  voter1.receipts_.clear();
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(0u, voter1.receipts_.size());
  EXPECT_EQ(1u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(1, agg_id, kDummyExecutionContext, kOverridePriority,
                           kReason);

  // Finally, canceling the override vote should cancel the upstream vote.
  voter0.receipts_.clear();
  EXPECT_EQ(0u, voter0.receipts_.size());
  EXPECT_EQ(0u, voter1.receipts_.size());
  EXPECT_EQ(0u, agg.GetSizeForTesting());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(0u, consumer.valid_vote_count_);
}

}  // namespace execution_context_priority
}  // namespace performance_manager

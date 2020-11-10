// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/freezing_vote_aggregator.h"

#include "base/rand_util.h"
#include "components/performance_manager/test_support/voting.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace freezing {

// Expose the FreezingVoteData type for testing.
class FreezingVoteAggregatorTestAccess {
 public:
  using FreezingVoteData = FreezingVoteAggregator::FreezingVoteData;

  static const FreezingVoteData::AcceptedVotesDeque& GetAllVotes(
      FreezingVoteAggregator* agg,
      const PageNode* node) {
    return agg->GetVoteData(node)->second.GetAcceptedVotesForTesting();
  }
};
using VoteData = FreezingVoteAggregatorTestAccess::FreezingVoteData;

namespace {

using DummyFreezingVoter = voting::test::DummyVoter<FreezingVote>;
using DummyFreezingVoteConsumer = voting::test::DummyVoteConsumer<FreezingVote>;

// Some dummy page nodes.
const PageNode* kPageNode0 = reinterpret_cast<const PageNode*>(0xDEADBEEF);
const PageNode* kPageNode1 = reinterpret_cast<const PageNode*>(0xBAADF00D);

static const char kReason0[] = "a reason";
static const char kReason1[] = "another reason";
static const char kReason2[] = "yet another reason";

}  // namespace

TEST(FreezingVoteAggregatorTest, EndToEnd) {
  // Builds the small hierarchy of voters as follows:
  //
  //        consumer
  //           |
  //          agg
  //         / |  \
  //        /  |   \
  //  voter0 voter1 voter2
  DummyFreezingVoteConsumer consumer;
  FreezingVoteAggregator agg;
  DummyFreezingVoter voter0;
  DummyFreezingVoter voter1;
  DummyFreezingVoter voter2;

  voting::VoterId<FreezingVote> agg_id = voting::kInvalidVoterId<FreezingVote>;
  {
    auto channel = consumer.voting_channel_factory_.BuildVotingChannel();
    agg_id = channel.voter_id();
    agg.SetUpstreamVotingChannel(std::move(channel));
  }

  voter0.SetVotingChannel(agg.GetVotingChannel());
  voter1.SetVotingChannel(agg.GetVotingChannel());
  voter2.SetVotingChannel(agg.GetVotingChannel());

  // Create some dummy votes for each PageNode and immediately expect
  // them to propagate upwards.
  voter0.EmitVote(kPageNode0, FreezingVoteValue::kCannotFreeze, kReason0);
  voter1.EmitVote(kPageNode1, FreezingVoteValue::kCanFreeze, kReason1);
  // Current state and expectations:
  //    - kPageNode0: 1 x kCannotFreeze  =>  kCannotFreeze
  //    - kPageNode1: 1 x kCanFreeze     =>  kCanFreeze
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(0u, voter2.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kPageNode0,
                           FreezingVoteValue::kCannotFreeze, kReason0);
  consumer.ExpectValidVote(1, agg_id, kPageNode1, FreezingVoteValue::kCanFreeze,
                           kReason1);

  // Change an existing vote, and expect it to propagate upwards.
  voter0.receipts_[0].ChangeVote(FreezingVoteValue::kCanFreeze, kReason2);
  // Current state and expectations:
  //    - kPageNode0: 1 x kCanFreeze     =>  kCanFreeze
  //    - kPageNode1: 1 x kCanFreeze     =>  kCanFreeze
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(0u, voter2.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kPageNode0, FreezingVoteValue::kCanFreeze,
                           kReason2);
  consumer.ExpectValidVote(1, agg_id, kPageNode1, FreezingVoteValue::kCanFreeze,
                           kReason1);

  // Submit a new kCanFreeze vote and expect no change.
  voter2.EmitVote(kPageNode1, FreezingVoteValue::kCanFreeze, kReason0);
  // Current state and expectations:
  //    - kPageNode0: 1 x kCanFreeze     =>  kCanFreeze
  //    - kPageNode1: 2 x kCanFreeze     =>  kCanFreeze
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, voter2.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kPageNode0, FreezingVoteValue::kCanFreeze,
                           kReason2);
  consumer.ExpectValidVote(1, agg_id, kPageNode1, FreezingVoteValue::kCanFreeze,
                           kReason1);

  // Submit a new vote with a different value and expect it to propagate
  // upwards.
  voter2.EmitVote(kPageNode1, FreezingVoteValue::kCannotFreeze, kReason0);
  // Current state and expectations:
  //    - kPageNode0: 1 x kCanFreeze     =>  kCanFreeze
  //    - kPageNode1: 2 x kCanFreeze + 1 x kCannotFreeze    =>  kCannotFreeze
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(2u, voter2.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kPageNode0, FreezingVoteValue::kCanFreeze,
                           kReason2);
  consumer.ExpectValidVote(1, agg_id, kPageNode1,
                           FreezingVoteValue::kCannotFreeze, kReason0);

  // Invalidate the only kCannotFreeze vote for a given PageNode and expect it
  // to propagate upwards.
  voter2.receipts_.clear();
  // Current state and expectations:
  //    - kPageNode0: 1 x kCanFreeze     =>  kCanFreeze
  //    - kPageNode1: 1 x kCanFreeze     =>  kCanFreeze
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(0u, voter2.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kPageNode0, FreezingVoteValue::kCanFreeze,
                           kReason2);
  consumer.ExpectValidVote(1, agg_id, kPageNode1, FreezingVoteValue::kCanFreeze,
                           kReason1);

  // Invalidate the remaining vote for one of the PageNode.
  voter1.receipts_.clear();
  // Current state and expectations:
  //    - kPageNode0: 1 x kCanFreeze     =>  kCanFreeze
  //    - kPageNode1: No vote            =>  Invalidated
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(0u, voter1.receipts_.size());
  EXPECT_EQ(0u, voter2.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kPageNode0, FreezingVoteValue::kCanFreeze,
                           kReason2);
  consumer.ExpectInvalidVote(1);

  // Emit a new vote for the PageNode that had no remaining vote
  voter1.EmitVote(kPageNode1, FreezingVoteValue::kCannotFreeze, kReason2);
  // Current state and expectations:
  //    - kPageNode0: 1 x kCanFreeze     =>  kCanFreeze
  //    - kPageNode1: 1 x kCanFreeze     =>  kCanFreeze
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(0u, voter2.receipts_.size());
  EXPECT_EQ(3u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kPageNode0, FreezingVoteValue::kCanFreeze,
                           kReason2);
  consumer.ExpectInvalidVote(1);
  consumer.ExpectValidVote(2, agg_id, kPageNode1,
                           FreezingVoteValue::kCannotFreeze, kReason2);
}

TEST(FreezingVoteAggregatorTest, VoteIntegrity) {
  DummyFreezingVoteConsumer consumer;
  FreezingVoteAggregator agg;
  DummyFreezingVoter voter0;
  DummyFreezingVoter voter1;

  voting::VoterId<FreezingVote> agg_id = voting::kInvalidVoterId<FreezingVote>;
  {
    auto channel = consumer.voting_channel_factory_.BuildVotingChannel();
    agg_id = channel.voter_id();
    agg.SetUpstreamVotingChannel(std::move(channel));
  }

  voter0.SetVotingChannel(agg.GetVotingChannel());
  voter1.SetVotingChannel(agg.GetVotingChannel());

  // Submit a first vote, this should be the only vote tracked by the
  // aggregator.
  voter0.EmitVote(kPageNode0, FreezingVoteValue::kCanFreeze, kReason0);
  EXPECT_EQ(
      1u,
      FreezingVoteAggregatorTestAccess::GetAllVotes(&agg, kPageNode0).size());
  EXPECT_EQ(FreezingVoteValue::kCanFreeze,
            FreezingVoteAggregatorTestAccess::GetAllVotes(&agg, kPageNode0)
                .front()
                .vote()
                .value());
  EXPECT_EQ(kReason0,
            FreezingVoteAggregatorTestAccess::GetAllVotes(&agg, kPageNode0)
                .front()
                .vote()
                .reason());
  consumer.ExpectValidVote(0, agg_id, kPageNode0, FreezingVoteValue::kCanFreeze,
                           kReason0);

  // Emit a second vote that should be ordered before the previous one, ensure
  // that this is the case.
  voter1.EmitVote(kPageNode0, FreezingVoteValue::kCannotFreeze, kReason1);
  EXPECT_EQ(
      2u,
      FreezingVoteAggregatorTestAccess::GetAllVotes(&agg, kPageNode0).size());
  EXPECT_EQ(FreezingVoteValue::kCannotFreeze,
            FreezingVoteAggregatorTestAccess::GetAllVotes(&agg, kPageNode0)
                .begin()
                ->vote()
                .value());
  EXPECT_EQ(kReason1,
            FreezingVoteAggregatorTestAccess::GetAllVotes(&agg, kPageNode0)
                .begin()
                ->vote()
                .reason());
  EXPECT_EQ(FreezingVoteValue::kCanFreeze,
            FreezingVoteAggregatorTestAccess::GetAllVotes(&agg, kPageNode0)
                .back()
                .vote()
                .value());
  EXPECT_EQ(kReason0,
            FreezingVoteAggregatorTestAccess::GetAllVotes(&agg, kPageNode0)
                .back()
                .vote()
                .reason());
  consumer.ExpectValidVote(0, agg_id, kPageNode0,
                           FreezingVoteValue::kCannotFreeze, kReason1);

  // Removing the second vote should restore things back to the state they were
  // before casting it.
  voter1.receipts_.clear();
  EXPECT_EQ(
      1u,
      FreezingVoteAggregatorTestAccess::GetAllVotes(&agg, kPageNode0).size());
  EXPECT_EQ(FreezingVoteValue::kCanFreeze,
            FreezingVoteAggregatorTestAccess::GetAllVotes(&agg, kPageNode0)
                .front()
                .vote()
                .value());
  EXPECT_EQ(kReason0,
            FreezingVoteAggregatorTestAccess::GetAllVotes(&agg, kPageNode0)
                .front()
                .vote()
                .reason());
  consumer.ExpectValidVote(0, agg_id, kPageNode0, FreezingVoteValue::kCanFreeze,
                           kReason0);

  // Removing the last vote should cause the upstreamed vote to be invalidated.
  voter0.receipts_.clear();
  consumer.ExpectInvalidVote(0);
}

}  // namespace freezing
}  // namespace performance_manager

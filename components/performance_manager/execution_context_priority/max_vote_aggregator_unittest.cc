// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/execution_context_priority/max_vote_aggregator.h"

#include "base/rand_util.h"
#include "components/performance_manager/test_support/voting.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace execution_context_priority {

// Expose the VoteData type for testing.
class MaxVoteAggregatorTestAccess {
 public:
  using VoteData = MaxVoteAggregator::VoteData;
  using StampedVote = MaxVoteAggregator::StampedVote;
};
using VoteData = MaxVoteAggregatorTestAccess::VoteData;
using StampedVote = MaxVoteAggregatorTestAccess::StampedVote;

namespace {

using DummyVoter = voting::test::DummyVoter<Vote>;
using DummyVoteConsumer = voting::test::DummyVoteConsumer<Vote>;

// Some dummy execution contexts.
const ExecutionContext* kExecutionContext0 =
    reinterpret_cast<const ExecutionContext*>(0xDEADBEEF);
const ExecutionContext* kExecutionContext1 =
    reinterpret_cast<const ExecutionContext*>(0xBAADF00D);

static constexpr base::TaskPriority kPriority0 = base::TaskPriority::LOWEST;
static constexpr base::TaskPriority kPriority1 =
    base::TaskPriority::USER_VISIBLE;
static constexpr base::TaskPriority kPriority2 = base::TaskPriority::HIGHEST;

static_assert(kPriority0 < kPriority1 && kPriority1 < kPriority2,
              "priorities must be well ordered");

static const char kReason0[] = "a reason";
static const char kReason1[] = "another reason";
static const char kReason2[] = "yet another reason";

size_t RandIndex(const VoteData& vote_data) {
  DCHECK(!vote_data.IsEmpty());
  int i = base::RandInt(0, static_cast<int>(vote_data.GetSize() - 1));
  return static_cast<size_t>(i);
}

base::TaskPriority RandPriority() {
  int i = base::RandInt(static_cast<int>(base::TaskPriority::LOWEST),
                        static_cast<int>(base::TaskPriority::HIGHEST));
  return static_cast<base::TaskPriority>(i);
}

const char* RandReason() {
  int i = base::RandInt(0, 2);
  if (i == 0)
    return kReason0;
  if (i == 1)
    return kReason1;
  DCHECK_EQ(2, i);
  return kReason2;
}

class FakeVoteConsumer : public DummyVoteConsumer {
 public:
  FakeVoteConsumer() = default;
  ~FakeVoteConsumer() override = default;

 protected:
  // Deliberately override VoteInvalidated so that this consumer silently
  // ignores these notifications.
  void VoteInvalidated(util::PassKey<AcceptedVote>,
                       AcceptedVote* vote) override {
    return;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeVoteConsumer);
};

}  // namespace

TEST(MaxVoteAggregatorTest, StampedVoteCast) {
  StampedVote stamped_vote;

  EXPECT_EQ(&stamped_vote, StampedVote::FromAcceptedVote(&stamped_vote.vote));
}

TEST(MaxVoteAggregatorTest, VoteDataHeapStressTest) {
  // Build a simple consumer/voter chain so that we generate an actual
  // voting::VoterId.
  FakeVoteConsumer consumer;
  DummyVoter voter;
  voting::VoterId<Vote> voter_id;
  {
    auto channel = consumer.voting_channel_factory_.BuildVotingChannel();
    voter_id = channel.voter_id();
    voter.SetVotingChannel(std::move(channel));
  }

  MaxVoteAggregatorTestAccess::VoteData vd;

  // Parameters controlling the test.
  static constexpr int kInsert = 0;
  static constexpr int kMove = 1;
  static constexpr int kRemove = 2;
  static constexpr size_t kInitialInserts = 100;
  static constexpr size_t kNops = 10000;

  uint32_t next_vote_id = 0;

  for (size_t i = 0; i < kNops; ++i) {
    const size_t ops_left = kNops - i;

    // Determine the type of operation to perform.
    int operation = base::RandInt(kInsert, kRemove);
    if (vd.GetSize() == 0 || i < kInitialInserts)
      operation = kInsert;

    // If an insertion will make it impossible to remove all elements with the
    // remaining nops, make it a move instead. This guarantees that we finish
    // with zero elements in the heap, although we may actually get to that
    // point before we completely run out of operations.
    if (operation == kInsert && vd.GetSize() + 1 > ops_left - 1)
      operation = kMove;

    // If there are as many operations left as elements then do a removal.
    if (ops_left == vd.GetSize())
      operation = kRemove;

    switch (operation) {
      case kInsert: {
        auto priority = RandPriority();
        auto* reason = RandReason();
        vd.AddVote(AcceptedVote(&consumer, voter_id, kExecutionContext0,
                                Vote(priority, reason)),
                   next_vote_id++);
      } break;

      case kMove: {
        // Choose a vote and generate a new priority/reason for it.
        size_t index = RandIndex(vd);
        auto& vote = vd.GetVoteForTesting(index);
        auto priority = RandPriority();
        auto* reason = RandReason();
        while (priority == vote.vote().value() &&
               reason == vote.vote().reason()) {
          priority = RandPriority();
          reason = RandReason();
        }

        // Update the vote.
        vote.UpdateVote(Vote(priority, reason));
        vd.UpdateVote(index, next_vote_id++);
      } break;

      case kRemove: {
        // Choose a vote and remove it.
        size_t index = RandIndex(vd);

        // Issue a receipt that is immediately destroyed, so that the vote is no
        // longer valid. Then remove the vote.
        vd.GetVoteForTesting(index).IssueReceipt();
        vd.RemoveVote(index);
      } break;

      default:
        NOTREACHED();
    }
  }

  // Expect the heap to be empty at the end of the test.
  EXPECT_TRUE(vd.IsEmpty());
}

TEST(MaxVoteAggregatorTest, BlackboxTest) {
  // Builds the small hierarchy of voters as follows:
  //
  //        consumer
  //           |
  //          agg
  //         / |  \
  //        /  |   \
  //  voter0 voter1 voter2
  DummyVoteConsumer consumer;
  MaxVoteAggregator agg;
  DummyVoter voter0;
  DummyVoter voter1;
  DummyVoter voter2;

  voting::VoterId<Vote> agg_id = voting::kInvalidVoterId<Vote>;
  {
    auto channel = consumer.voting_channel_factory_.BuildVotingChannel();
    agg_id = channel.voter_id();
    agg.SetUpstreamVotingChannel(std::move(channel));
  }

  voter0.SetVotingChannel(agg.GetVotingChannel());
  voter1.SetVotingChannel(agg.GetVotingChannel());
  voter2.SetVotingChannel(agg.GetVotingChannel());

  // Create some dummy votes for each execution context and immediately expect
  // them to propagate upwards.
  voter0.EmitVote(kExecutionContext0, kPriority0, kReason0);
  voter1.EmitVote(kExecutionContext1, kPriority1, kReason0);
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(0u, voter2.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kExecutionContext0, kPriority0, kReason0);
  consumer.ExpectValidVote(1, agg_id, kExecutionContext1, kPriority1, kReason0);

  // Change an existing vote, and expect it to propagate upwards.
  voter0.receipts_[0].ChangeVote(kPriority0, kReason1);
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(0u, voter2.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kExecutionContext0, kPriority0, kReason1);
  consumer.ExpectValidVote(1, agg_id, kExecutionContext1, kPriority1, kReason0);

  // Submit a new vote with lower priority than the upstream vote and expect no
  // change.
  voter2.EmitVote(kExecutionContext1, kPriority0, kReason0);
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(1u, voter2.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kExecutionContext0, kPriority0, kReason1);
  consumer.ExpectValidVote(1, agg_id, kExecutionContext1, kPriority1, kReason0);

  // Submit a new vote with a higher priority than the upstream vote and expect
  // it to propagate.
  voter2.EmitVote(kExecutionContext0, kPriority2, kReason0);
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(2u, voter2.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kExecutionContext0, kPriority2, kReason0);
  consumer.ExpectValidVote(1, agg_id, kExecutionContext1, kPriority1, kReason0);

  // Invalidate a lower priority vote that is not upstreamed. Expect no
  // upstream change.
  voter2.receipts_[0].Reset();
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(1u, voter1.receipts_.size());
  EXPECT_EQ(2u, voter2.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kExecutionContext0, kPriority2, kReason0);
  consumer.ExpectValidVote(1, agg_id, kExecutionContext1, kPriority1, kReason0);

  // Create a third vote for kExecutionContext0 with yet another priority.
  // Expect this not to propagate.
  voter1.EmitVote(kExecutionContext0, kPriority1, kReason0);
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(2u, voter1.receipts_.size());
  EXPECT_EQ(2u, voter2.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kExecutionContext0, kPriority2, kReason0);
  consumer.ExpectValidVote(1, agg_id, kExecutionContext1, kPriority1, kReason0);

  // Invalidate the highest priority vote that is upstreamed. Expect the vote to
  // revert to the next highest priority.
  voter2.receipts_.clear();
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(2u, voter1.receipts_.size());
  EXPECT_EQ(0u, voter2.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kExecutionContext0, kPriority1, kReason0);
  consumer.ExpectValidVote(1, agg_id, kExecutionContext1, kPriority1, kReason0);

  // Invalidate the next highest vote and expect it to revert to the lowest
  // vote.
  voter1.receipts_.back().Reset();
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(2u, voter1.receipts_.size());
  EXPECT_EQ(0u, voter2.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  consumer.ExpectValidVote(0, agg_id, kExecutionContext0, kPriority0, kReason1);
  consumer.ExpectValidVote(1, agg_id, kExecutionContext1, kPriority1, kReason0);

  // Clear the last vote for |kExecutionContext0| and expect the upstream vote
  // to be invalidated.
  voter0.receipts_[0].Reset();
  EXPECT_EQ(1u, voter0.receipts_.size());
  EXPECT_EQ(2u, voter1.receipts_.size());
  EXPECT_EQ(0u, voter2.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  EXPECT_FALSE(consumer.votes_[0].IsValid());
  consumer.ExpectValidVote(1, agg_id, kExecutionContext1, kPriority1, kReason0);

  // Clear the last outstanding votes and expect all upstream votes to have
  // been canceled.
  voter0.receipts_.clear();
  voter1.receipts_.clear();
  EXPECT_EQ(0u, voter0.receipts_.size());
  EXPECT_EQ(0u, voter1.receipts_.size());
  EXPECT_EQ(0u, voter2.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(0u, consumer.valid_vote_count_);
  EXPECT_FALSE(consumer.votes_[0].IsValid());
  EXPECT_FALSE(consumer.votes_[1].IsValid());
}

}  // namespace execution_context_priority
}  // namespace performance_manager

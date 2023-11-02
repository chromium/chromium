// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/freezing_vote_decorator.h"

#include "components/performance_manager/freezing/freezing_vote_aggregator.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {
static const freezing::FreezingVote kCannotFreezeVote(
    freezing::FreezingVoteValue::kCannotFreeze,
    "cannot freeze");
static const freezing::FreezingVote kCanFreezeVote(
    freezing::FreezingVoteValue::kCanFreeze,
    "can freeze");
}  // namespace

class FreezingVoteDecoratorTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  FreezingVoteDecoratorTest() = default;
  ~FreezingVoteDecoratorTest() override = default;

  void SetUp() override {
    Super::SetUp();
    graph()->PassToGraph(std::make_unique<FreezingVoteDecorator>());
  }
};

TEST_F(FreezingVoteDecoratorTest, VotesAreForwarded) {
  auto page_node = CreateNode<PageNodeImpl>();
  EXPECT_FALSE(page_node->freezing_vote());

  freezing::FreezingVotingChannel voter =
      graph()
          ->GetRegisteredObjectAs<freezing::FreezingVoteAggregator>()
          ->GetVotingChannel();

  voter.SubmitVote(page_node.get(), kCannotFreezeVote);
  ASSERT_TRUE(page_node->freezing_vote().has_value());
  EXPECT_EQ(kCannotFreezeVote, page_node->freezing_vote().value());

  voter.ChangeVote(page_node.get(), kCanFreezeVote);
  ASSERT_TRUE(page_node->freezing_vote().has_value());
  EXPECT_EQ(kCanFreezeVote, page_node->freezing_vote().value());

  voter.ChangeVote(page_node.get(), kCannotFreezeVote);
  ASSERT_TRUE(page_node->freezing_vote().has_value());
  EXPECT_EQ(kCannotFreezeVote, page_node->freezing_vote().value());

  voter.InvalidateVote(page_node.get());
  EXPECT_FALSE(page_node->freezing_vote());
}

}  // namespace performance_manager

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/boosting_vote_aggregator.h"

#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/test_support/voting.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager {
namespace execution_context_priority {

namespace {

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;

class DummyExecutionContext : public ExecutionContext {
 public:
  DummyExecutionContext() = default;
  ~DummyExecutionContext() override = default;

  execution_context::ExecutionContextType GetType() const override {
    return execution_context::ExecutionContextType();
  }
  blink::ExecutionContextToken GetToken() const override {
    return blink::ExecutionContextToken();
  }
  Graph* GetGraph() const override { return nullptr; }
  const GURL& GetUrl() const override { return url_; }
  const ProcessNode* GetProcessNode() const override { return nullptr; }
  const PriorityAndReason& GetPriorityAndReason() const override {
    return par_;
  }
  const FrameNode* GetFrameNode() const override { return nullptr; }
  const WorkerNode* GetWorkerNode() const override { return nullptr; }

 private:
  GURL url_;
  PriorityAndReason par_;
};

static const char kReasonBoost[] = "boosted!";

static const Vote kLowPriorityVote0(base::TaskPriority::LOWEST, "low reason 0");
static const Vote kLowPriorityVote1(base::TaskPriority::LOWEST, "low reason 1");

static const Vote kMediumPriorityVote0(base::TaskPriority::USER_VISIBLE,
                                       "medium reason 0");
static const Vote kMediumPriorityVote1(base::TaskPriority::USER_VISIBLE,
                                       "medium reason 1");

static const Vote kHighPriorityVote0(base::TaskPriority::HIGHEST,
                                     "high reason 0");
static const Vote kHighPriorityVote1(base::TaskPriority::HIGHEST,
                                     "high reason 1");

class TestBoostingVoteAggregator : public BoostingVoteAggregator {
 public:
  using BoostingVoteAggregator::forward_edges_;
  using BoostingVoteAggregator::NodeData;
  using BoostingVoteAggregator::nodes_;
  using BoostingVoteAggregator::reverse_edges_;
};

using NodeData = TestBoostingVoteAggregator::NodeData;

class BoostingVoteAggregatorTest : public testing::Test {
 public:
  void SetUp() override {
    // Set up |aggregator_| so that it upstreams votes to |observer_|.
    auto channel = observer_.BuildVotingChannel();
    aggregator_voter_id_ = channel.voter_id();
    aggregator_.SetUpstreamVotingChannel(std::move(channel));

    voter_ = aggregator_.GetVotingChannel();
    EXPECT_TRUE(aggregator_.nodes_.empty());
    EXPECT_TRUE(aggregator_.forward_edges_.empty());
    EXPECT_TRUE(aggregator_.reverse_edges_.empty());
  }

  VoterId aggregator_voter_id() const { return aggregator_voter_id_; }

  const DummyVoteObserver& observer() const { return observer_; }

  TestBoostingVoteAggregator* aggregator() { return &aggregator_; }

  VotingChannel* voter() { return &voter_; }

  void ExpectEdges(size_t count) {
    EXPECT_EQ(count, aggregator_.forward_edges_.size());
    EXPECT_EQ(count, aggregator_.reverse_edges_.size());
  }

  void ExpectIsActive(const NodeData& node_data,
                      bool mid_priority,
                      bool high_priority) {
    EXPECT_EQ(mid_priority, node_data.IsActive(1));
    EXPECT_EQ(high_priority, node_data.IsActive(2));
  }

  // Some dummy execution contexts.
  DummyExecutionContext dummy_contexts[5];
  raw_ptr<const ExecutionContext> kExecutionContext0 = &dummy_contexts[0];
  raw_ptr<const ExecutionContext> kExecutionContext1 = &dummy_contexts[1];
  raw_ptr<const ExecutionContext> kExecutionContext2 = &dummy_contexts[2];
  raw_ptr<const ExecutionContext> kExecutionContext3 = &dummy_contexts[3];
  raw_ptr<const ExecutionContext> kExecutionContext4 = &dummy_contexts[4];

 private:
  // The id of |aggregator_| as seen by its upstream |observer_|.
  voting::VoterId<Vote> aggregator_voter_id_;
  DummyVoteObserver observer_;
  TestBoostingVoteAggregator aggregator_;
  VotingChannel voter_;
};

}  // namespace

TEST_F(BoostingVoteAggregatorTest, VotesUpstreamingWorks) {
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));

  // Submit a default vote to the boosting aggregator, and expect it not to be
  // upstreamed.
  voter()->SubmitVote(kExecutionContext0, kLowPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));

  // Change the priority to a non-default one..
  voter()->ChangeVote(kExecutionContext0, kHighPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote0));

  // Change only the reason.
  voter()->ChangeVote(kExecutionContext0, kHighPriorityVote1);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote1));

  // Add a non-default vote for a different execution context.
  voter()->SubmitVote(kExecutionContext1, kMediumPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote1));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kMediumPriorityVote0));

  // Change the vote for the second execution context to another non-default
  // value.
  voter()->ChangeVote(kExecutionContext1, kHighPriorityVote0);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote1));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kHighPriorityVote0));

  // Change the vote for the second execution context to the default vote value
  // and expect it to be invalidated upstream.
  voter()->ChangeVote(kExecutionContext1, kLowPriorityVote1);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote1));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext1));

  // Invalidate vote for the first execution context.
  voter()->InvalidateVote(kExecutionContext0);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));

  // Then the second.
  voter()->InvalidateVote(kExecutionContext1);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));
}

TEST_F(BoostingVoteAggregatorTest, BoostingWorks) {
  // Add a boosting vote, with no actual incoming votes. This should produce
  // the two nodes associated with the edge but not upstream any votes.
  BoostingVote boost01a(aggregator(), kExecutionContext0, kExecutionContext1,
                        kReasonBoost);
  const auto& data0 = aggregator()->nodes_.find(kExecutionContext0)->second;
  const auto& data1 = aggregator()->nodes_.find(kExecutionContext1)->second;
  ExpectEdges(1);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_EQ(2u, aggregator()->nodes_.size());
  EXPECT_EQ(1u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  ExpectIsActive(data0, false, false);
  ExpectIsActive(data1, false, false);

  // Create a second boosting vote. This duplicates the edge.
  BoostingVote boost01b(aggregator(), kExecutionContext0, kExecutionContext1,
                        kReasonBoost);
  ExpectEdges(1);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_EQ(2u, aggregator()->nodes_.size());
  EXPECT_EQ(1u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  ExpectIsActive(data0, false, false);
  ExpectIsActive(data1, false, false);

  // Create a mid priority vote for execution context 1. This should cause a
  // single vote to be emitted for that node.
  voter()->SubmitVote(kExecutionContext1, kMediumPriorityVote1);
  EXPECT_EQ(2u, aggregator()->nodes_.size());
  ExpectEdges(1);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kMediumPriorityVote1));
  EXPECT_EQ(1u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  ExpectIsActive(data0, false, false);
  ExpectIsActive(data1, true, false);

  // Create a mid priority vote for execution context 0. This should cause
  // another vote to be emitted.
  voter()->SubmitVote(kExecutionContext0, kMediumPriorityVote0);
  EXPECT_EQ(2u, aggregator()->nodes_.size());
  ExpectEdges(1);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kMediumPriorityVote0));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kMediumPriorityVote1));
  EXPECT_EQ(1u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data1, true, false);

  // Cancel the priority 1 vote for execution context 1. The boosting should
  // maintain the output priority for that node.
  voter()->InvalidateVote(kExecutionContext1);
  EXPECT_EQ(2u, aggregator()->nodes_.size());
  ExpectEdges(1);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kMediumPriorityVote0));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kMediumPriorityVote0.value(), kReasonBoost));
  EXPECT_EQ(1u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data1, true, false);

  // Create a default vote for a third execution context. Other than creating
  // the node data and the vote this shouldn't do anything.
  voter()->SubmitVote(kExecutionContext2, kLowPriorityVote0);
  const auto& data2 = aggregator()->nodes_.find(kExecutionContext2)->second;
  EXPECT_EQ(3u, aggregator()->nodes_.size());
  ExpectEdges(1);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kMediumPriorityVote0));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kMediumPriorityVote0.value(), kReasonBoost));
  EXPECT_EQ(1u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  EXPECT_EQ(0u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data1, true, false);
  ExpectIsActive(data2, false, false);

  // Create a boosting vote from execution context 2 to execution context 0.
  // This should create an edge.
  BoostingVote boost20(aggregator(), kExecutionContext2, kExecutionContext0,
                       kReasonBoost);
  EXPECT_EQ(3u, aggregator()->nodes_.size());
  ExpectEdges(2);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kMediumPriorityVote0));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kMediumPriorityVote0.value(), kReasonBoost));
  EXPECT_EQ(2u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  EXPECT_EQ(1u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data1, true, false);
  ExpectIsActive(data2, false, false);

  // Change the vote for execution context 2 to a higher one. This should boost
  // execution contexts 0 and 1 as well.
  voter()->ChangeVote(kExecutionContext2, kHighPriorityVote0);
  EXPECT_EQ(3u, aggregator()->nodes_.size());
  ExpectEdges(2);
  EXPECT_EQ(observer().GetVoteCount(), 3u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote0.value(), kReasonBoost));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kHighPriorityVote0.value(), kReasonBoost));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext2,
                                 kHighPriorityVote0));
  EXPECT_EQ(2u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  EXPECT_EQ(1u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, true);
  ExpectIsActive(data1, true, true);
  ExpectIsActive(data2, false, true);

  // Emit a highest priority vote for execution context 1. This should change
  // the vote reason.
  voter()->SubmitVote(kExecutionContext1, kHighPriorityVote1);
  EXPECT_EQ(3u, aggregator()->nodes_.size());
  ExpectEdges(2);
  EXPECT_EQ(observer().GetVoteCount(), 3u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kHighPriorityVote0.value(), kReasonBoost));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kHighPriorityVote1));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext2,
                                 kHighPriorityVote0));
  EXPECT_EQ(2u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  EXPECT_EQ(1u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, true);
  ExpectIsActive(data1, true, true);
  ExpectIsActive(data2, false, true);

  // Kill the vote for execution context 2. This should kill the upstream vote
  // for execution context 2 entirely, reduce the priority of execution context
  // 0, and keep execution context 1 the same.
  voter()->InvalidateVote(kExecutionContext2);
  EXPECT_EQ(3u, aggregator()->nodes_.size());
  ExpectEdges(2);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kMediumPriorityVote0));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kHighPriorityVote1));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext2));
  EXPECT_EQ(2u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  EXPECT_EQ(1u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data1, true, true);
  ExpectIsActive(data2, false, false);

  // Kill the direct vote for execution context 1 so it goes back to being
  // boosted by execution context 0.
  voter()->InvalidateVote(kExecutionContext1);
  EXPECT_EQ(3u, aggregator()->nodes_.size());
  ExpectEdges(2);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kMediumPriorityVote0));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kMediumPriorityVote0.value(), kReasonBoost));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext2));
  EXPECT_EQ(2u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  EXPECT_EQ(1u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data1, true, false);
  ExpectIsActive(data2, false, false);

  // Kill the first boosting vote from 0 to 1. This should do nothing but change
  // the multiplicity of the edge.
  boost01a.Reset();
  EXPECT_EQ(3u, aggregator()->nodes_.size());
  ExpectEdges(2);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kMediumPriorityVote0));
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext1,
                                 kMediumPriorityVote0.value(), kReasonBoost));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext2));
  EXPECT_EQ(2u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data1.edge_count_for_testing());
  EXPECT_EQ(1u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data1, true, false);
  ExpectIsActive(data2, false, false);

  // Kill the second boosting vote from 0 to 1. This should change edge counts,
  // and remove both the vote and the node data. The variable |data1| is now
  // invalid.
  boost01b.Reset();
  EXPECT_EQ(2u, aggregator()->nodes_.size());
  ExpectEdges(1);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kMediumPriorityVote0));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext1));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext2));
  EXPECT_EQ(1u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data2, false, false);

  // Move the boosting vote. The move should not cause any outwardly visible
  // changes.
  BoostingVote boost20b(std::move(boost20));
  EXPECT_EQ(aggregator(), boost20b.aggregator());
  EXPECT_EQ(kExecutionContext2, boost20b.input_execution_context());
  EXPECT_EQ(kExecutionContext0, boost20b.output_execution_context());
  EXPECT_EQ(kReasonBoost, boost20b.reason());
  EXPECT_FALSE(boost20.aggregator());
  EXPECT_FALSE(boost20.input_execution_context());
  EXPECT_FALSE(boost20.output_execution_context());
  EXPECT_FALSE(boost20.reason());
  EXPECT_EQ(2u, aggregator()->nodes_.size());
  ExpectEdges(1);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kMediumPriorityVote0));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext1));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext2));
  EXPECT_EQ(1u, data0.edge_count_for_testing());
  EXPECT_EQ(1u, data2.edge_count_for_testing());
  ExpectIsActive(data0, true, false);
  ExpectIsActive(data2, false, false);

  // Remove the boosting vote from 2 to 0. This should change edge counts, and
  // also remove the node data associated with node 2. |data2| is now invalid.
  boost20b.Reset();
  EXPECT_EQ(1u, aggregator()->nodes_.size());
  ExpectEdges(0);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(aggregator_voter_id(), kExecutionContext0,
                                 kMediumPriorityVote0));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext1));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext2));
  EXPECT_EQ(0u, data0.edge_count_for_testing());
  ExpectIsActive(data0, true, false);

  // Finally remove the last vote. The aggregator should effectively be empty at
  // this point. |data0| also becomes invalid after this.
  voter()->InvalidateVote(kExecutionContext0);
  EXPECT_EQ(0u, aggregator()->nodes_.size());
  ExpectEdges(0);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext0));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext1));
  EXPECT_FALSE(observer().HasVote(aggregator_voter_id(), kExecutionContext2));
}

TEST_F(BoostingVoteAggregatorTest, DiamondPattern) {
  // Create a diamond boosting vote pattern:
  //
  //   1
  //  / \
  // 0   3
  //  \ /
  //   2
  BoostingVote boost01(aggregator(), kExecutionContext0, kExecutionContext1,
                       kReasonBoost);
  BoostingVote boost02(aggregator(), kExecutionContext0, kExecutionContext2,
                       kReasonBoost);
  BoostingVote boost13(aggregator(), kExecutionContext1, kExecutionContext3,
                       kReasonBoost);
  BoostingVote boost23(aggregator(), kExecutionContext2, kExecutionContext3,
                       kReasonBoost);

  const auto& data0 = aggregator()->nodes_.find(kExecutionContext0)->second;
  const auto& data1 = aggregator()->nodes_.find(kExecutionContext1)->second;
  const auto& data2 = aggregator()->nodes_.find(kExecutionContext2)->second;
  const auto& data3 = aggregator()->nodes_.find(kExecutionContext3)->second;
  ExpectIsActive(data0, false, false);
  ExpectIsActive(data1, false, false);
  ExpectIsActive(data2, false, false);
  ExpectIsActive(data3, false, false);

  // Add a vote to node 0. This should cause all nodes to be boosted.
  voter()->SubmitVote(kExecutionContext0, kHighPriorityVote0);
  ExpectIsActive(data0, false, true);
  ExpectIsActive(data1, false, true);
  ExpectIsActive(data2, false, true);
  ExpectIsActive(data3, false, true);

  // Cancel the vote. All boosting should disappear.
  voter()->InvalidateVote(kExecutionContext0);
  ExpectIsActive(data0, false, false);
  ExpectIsActive(data1, false, false);
  ExpectIsActive(data2, false, false);
  ExpectIsActive(data3, false, false);
}

TEST_F(BoostingVoteAggregatorTest, DiamondPatternMultipleVotes) {
  // Create another diamond boosting vote pattern:
  //
  //       1
  //      / \
  // 4 - 0   3
  //      \ /
  //       2
  BoostingVote boost01(aggregator(), kExecutionContext0, kExecutionContext1,
                       kReasonBoost);
  BoostingVote boost02(aggregator(), kExecutionContext0, kExecutionContext2,
                       kReasonBoost);
  BoostingVote boost13(aggregator(), kExecutionContext1, kExecutionContext3,
                       kReasonBoost);
  BoostingVote boost23(aggregator(), kExecutionContext2, kExecutionContext3,
                       kReasonBoost);

  const auto& data0 = aggregator()->nodes_.find(kExecutionContext0)->second;
  const auto& data1 = aggregator()->nodes_.find(kExecutionContext1)->second;
  const auto& data2 = aggregator()->nodes_.find(kExecutionContext2)->second;
  const auto& data3 = aggregator()->nodes_.find(kExecutionContext3)->second;
  ExpectIsActive(data0, false, false);
  ExpectIsActive(data1, false, false);
  ExpectIsActive(data2, false, false);
  ExpectIsActive(data3, false, false);

  // Add a vote to node 0. This should cause all downstream nodes to be boosted.
  voter()->SubmitVote(kExecutionContext0, kHighPriorityVote0);
  ExpectIsActive(data0, false, true);
  ExpectIsActive(data1, false, true);
  ExpectIsActive(data2, false, true);
  ExpectIsActive(data3, false, true);

  // Add a lower vote to execution context 0 via execution context 4. This
  // should also propagate through the network in a similar way.
  BoostingVote boost40(aggregator(), kExecutionContext4, kExecutionContext0,
                       kReasonBoost);
  const auto& data4 = aggregator()->nodes_.find(kExecutionContext4)->second;
  voter()->SubmitVote(kExecutionContext4, kMediumPriorityVote0);
  ExpectIsActive(data0, true, true);
  ExpectIsActive(data1, true, true);
  ExpectIsActive(data2, true, true);
  ExpectIsActive(data3, true, true);
  ExpectIsActive(data4, true, false);

  // Cleanup.
  voter()->InvalidateVote(kExecutionContext0);
  voter()->InvalidateVote(kExecutionContext4);
}

TEST_F(BoostingVoteAggregatorTest, RemoveEdgeFromCycle) {
  BoostingVote boost01(aggregator(), kExecutionContext0, kExecutionContext1,
                       kReasonBoost);
  BoostingVote boost12(aggregator(), kExecutionContext1, kExecutionContext2,
                       kReasonBoost);
  BoostingVote boost23(aggregator(), kExecutionContext2, kExecutionContext3,
                       kReasonBoost);
  BoostingVote boost30(aggregator(), kExecutionContext3, kExecutionContext0,
                       kReasonBoost);

  const auto& data0 = aggregator()->nodes_.find(kExecutionContext0)->second;
  const auto& data1 = aggregator()->nodes_.find(kExecutionContext1)->second;
  const auto& data2 = aggregator()->nodes_.find(kExecutionContext2)->second;
  const auto& data3 = aggregator()->nodes_.find(kExecutionContext3)->second;
  ExpectIsActive(data0, false, false);
  ExpectIsActive(data1, false, false);
  ExpectIsActive(data2, false, false);
  ExpectIsActive(data3, false, false);

  // Add a vote to node 0.
  voter()->SubmitVote(kExecutionContext0, kHighPriorityVote0);
  ExpectIsActive(data0, false, true);
  ExpectIsActive(data1, false, true);
  ExpectIsActive(data2, false, true);
  ExpectIsActive(data3, false, true);

  // Remove an edge from the cycle. The first half of the cycle should still
  // be boosted, the second half should not.
  boost12.Reset();
  ExpectIsActive(data0, false, true);
  ExpectIsActive(data1, false, true);
  ExpectIsActive(data2, false, false);
  ExpectIsActive(data3, false, false);

  // Cleanup.
  voter()->InvalidateVote(kExecutionContext0);
}

TEST_F(BoostingVoteAggregatorTest, MoveCancelsPreviousBoostingVote) {
  BoostingVote boost01(aggregator(), kExecutionContext0, kExecutionContext1,
                       kReasonBoost);
  BoostingVote boost12(aggregator(), kExecutionContext1, kExecutionContext2,
                       kReasonBoost);

  // Expect nodes to have been created for all nodes involved in boosting votes.
  EXPECT_TRUE(aggregator()->nodes_.count(kExecutionContext0));
  EXPECT_TRUE(aggregator()->nodes_.count(kExecutionContext1));
  EXPECT_TRUE(aggregator()->nodes_.count(kExecutionContext2));

  // Move one boosting vote into the other. This should cause the latter to be
  // canceled. In this case that means node0 should be removed.
  boost01 = std::move(boost12);
  EXPECT_FALSE(aggregator()->nodes_.count(kExecutionContext0));
  EXPECT_TRUE(aggregator()->nodes_.count(kExecutionContext1));
  EXPECT_TRUE(aggregator()->nodes_.count(kExecutionContext2));
}

TEST_F(BoostingVoteAggregatorTest, BoostingVoteAfterNormalVotes) {
  voter()->SubmitVote(kExecutionContext0, kHighPriorityVote0);
  EXPECT_TRUE(aggregator()->nodes_.count(kExecutionContext0));
  EXPECT_EQ(1u, aggregator()->nodes_.size());
  const auto& data0 = aggregator()->nodes_.find(kExecutionContext0)->second;
  ExpectIsActive(data0, false, true);

  BoostingVote boost12(aggregator(), kExecutionContext1, kExecutionContext2,
                       kReasonBoost);
  EXPECT_TRUE(aggregator()->nodes_.count(kExecutionContext0));
  EXPECT_TRUE(aggregator()->nodes_.count(kExecutionContext1));
  EXPECT_TRUE(aggregator()->nodes_.count(kExecutionContext2));
  EXPECT_EQ(3u, aggregator()->nodes_.size());
  const auto& data1 = aggregator()->nodes_.find(kExecutionContext1)->second;
  const auto& data2 = aggregator()->nodes_.find(kExecutionContext2)->second;
  ExpectIsActive(data0, false, true);
  ExpectIsActive(data1, false, false);
  ExpectIsActive(data2, false, false);

  BoostingVote boost01(aggregator(), kExecutionContext0, kExecutionContext1,
                       kReasonBoost);
  EXPECT_TRUE(aggregator()->nodes_.count(kExecutionContext0));
  EXPECT_TRUE(aggregator()->nodes_.count(kExecutionContext1));
  EXPECT_TRUE(aggregator()->nodes_.count(kExecutionContext2));
  EXPECT_EQ(3u, aggregator()->nodes_.size());
  ExpectIsActive(data0, false, true);
  ExpectIsActive(data1, false, true);
  ExpectIsActive(data2, false, true);

  // Cleanup.
  voter()->InvalidateVote(kExecutionContext0);
}

}  // namespace execution_context_priority
}  // namespace performance_manager

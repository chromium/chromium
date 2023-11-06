// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/frame_capturing_video_stream_voter.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/voting.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::execution_context_priority {

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  return execution_context::ExecutionContext::From(frame_node);
}

// Both the voting channel and the FrameCapturingVideoStreamVoter are expected
// to live on the graph, without being actual GraphOwned objects. This class
// wraps both to allow this.
class GraphOwnedWrapper : public GraphOwned {
 public:
  GraphOwnedWrapper() {
    VotingChannel voting_channel = observer_.BuildVotingChannel();
    voter_id_ = voting_channel.voter_id();
    frame_capturing_video_stream_voter_.SetVotingChannel(
        std::move(voting_channel));
  }

  ~GraphOwnedWrapper() override = default;

  GraphOwnedWrapper(const GraphOwnedWrapper&) = delete;
  GraphOwnedWrapper& operator=(const GraphOwnedWrapper&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override {
    graph->AddInitializingFrameNodeObserver(
        &frame_capturing_video_stream_voter_);
  }
  void OnTakenFromGraph(Graph* graph) override {
    graph->RemoveInitializingFrameNodeObserver(
        &frame_capturing_video_stream_voter_);
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return observer_; }

  VoterId voter_id() const { return voter_id_; }

 private:
  DummyVoteObserver observer_;
  FrameCapturingVideoStreamVoter frame_capturing_video_stream_voter_;
  VoterId voter_id_;
};

}  // namespace

class FrameCapturingVideoStreamVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  FrameCapturingVideoStreamVoterTest() = default;
  ~FrameCapturingVideoStreamVoterTest() override = default;

  FrameCapturingVideoStreamVoterTest(
      const FrameCapturingVideoStreamVoterTest&) = delete;
  FrameCapturingVideoStreamVoterTest& operator=(
      const FrameCapturingVideoStreamVoterTest&) = delete;

  void SetUp() override {
    GetGraphFeatures().EnableExecutionContextRegistry();
    Super::SetUp();
    wrapper_ = graph()->PassToGraph(std::make_unique<GraphOwnedWrapper>());
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return wrapper_->observer(); }

  VoterId voter_id() const { return wrapper_->voter_id(); }

 private:
  raw_ptr<GraphOwnedWrapper> wrapper_ = nullptr;
};

// Tests that the FrameCapturingVideoStreamVoter correctly casts a vote for a
// frame depending on its capturing video stream state.
TEST_F(FrameCapturingVideoStreamVoterTest, CapturingVideoStreamChanged) {
  // Create a graph with a single frame page. Its initial audible state should
  // be false, resulting in a low priority.
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto& frame_node = mock_graph.frame;
  EXPECT_FALSE(frame_node->is_audible());
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(frame_node.get()),
      base::TaskPriority::LOWEST,
      FrameCapturingVideoStreamVoter::kFrameCapturingVideoStreamReason));

  // Now set the frame as capturing a video stream. This should increase the
  // priority.
  mock_graph.frame->SetIsCapturingVideoStream(true);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(frame_node.get()),
      base::TaskPriority::USER_VISIBLE,
      FrameCapturingVideoStreamVoter::kFrameCapturingVideoStreamReason));

  // Deleting the frame should invalidate the vote.
  frame_node.reset();
  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

}  // namespace performance_manager::execution_context_priority

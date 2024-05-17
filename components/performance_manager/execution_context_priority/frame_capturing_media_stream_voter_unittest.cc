// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/frame_capturing_media_stream_voter.h"

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

// Both the voting channel and the FrameCapturingMediaStreamVoter are expected
// to live on the graph, without being actual GraphOwned objects. This class
// wraps both to allow this.
class GraphOwnedWrapper : public GraphOwned {
 public:
  GraphOwnedWrapper()
      : frame_capturing_media_stream_voter_(observer_.BuildVotingChannel()),
        voter_id_(frame_capturing_media_stream_voter_.voter_id()) {}

  ~GraphOwnedWrapper() override = default;

  GraphOwnedWrapper(const GraphOwnedWrapper&) = delete;
  GraphOwnedWrapper& operator=(const GraphOwnedWrapper&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override {
    frame_capturing_media_stream_voter_.InitializeOnGraph(graph);
  }
  void OnTakenFromGraph(Graph* graph) override {
    frame_capturing_media_stream_voter_.TearDownOnGraph(graph);
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return observer_; }

  VoterId voter_id() const { return voter_id_; }

 private:
  DummyVoteObserver observer_;
  FrameCapturingMediaStreamVoter frame_capturing_media_stream_voter_;
  VoterId voter_id_;
};

}  // namespace

class FrameCapturingMediaStreamVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  FrameCapturingMediaStreamVoterTest() = default;
  ~FrameCapturingMediaStreamVoterTest() override = default;

  FrameCapturingMediaStreamVoterTest(
      const FrameCapturingMediaStreamVoterTest&) = delete;
  FrameCapturingMediaStreamVoterTest& operator=(
      const FrameCapturingMediaStreamVoterTest&) = delete;

  void SetUp() override {
    Super::SetUp();
    wrapper_ = graph()->PassToGraph(std::make_unique<GraphOwnedWrapper>());
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return wrapper_->observer(); }

  VoterId voter_id() const { return wrapper_->voter_id(); }

 private:
  raw_ptr<GraphOwnedWrapper> wrapper_ = nullptr;
};

// Tests that the FrameCapturingMediaStreamVoter correctly casts a vote for a
// frame depending on its capturing media stream state.
TEST_F(FrameCapturingMediaStreamVoterTest, CapturingMediaStreamChanged) {
  // Create a graph with a single frame page. Initially, it's not capturing any
  // media stream,resulting in a low priority.
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto& frame_node = mock_graph.frame;
  EXPECT_FALSE(frame_node->IsCapturingMediaStream());
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(frame_node.get()),
      base::TaskPriority::LOWEST,
      FrameCapturingMediaStreamVoter::kFrameCapturingMediaStreamReason));

  // Now set the frame as capturing a media stream. This should increase the
  // priority.
  mock_graph.frame->SetIsCapturingMediaStream(true);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(frame_node.get()),
      base::TaskPriority::USER_BLOCKING,
      FrameCapturingMediaStreamVoter::kFrameCapturingMediaStreamReason));

  // Deleting the frame should invalidate the vote.
  frame_node.reset();
  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

}  // namespace performance_manager::execution_context_priority

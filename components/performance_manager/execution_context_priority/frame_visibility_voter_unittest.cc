// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/frame_visibility_voter.h"

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/voting.h"

namespace performance_manager {
namespace execution_context_priority {

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  return execution_context::ExecutionContext::From(frame_node);
}

// Both the voting channel and the FrameVisibilityVoter are expected live on the
// graph, without being actual GraphOwned objects. This class wraps both to
// allow this.
class GraphOwnedWrapper : public GraphOwned {
 public:
  GraphOwnedWrapper() {
    VotingChannel voting_channel = observer_.BuildVotingChannel();
    voter_id_ = voting_channel.voter_id();
    frame_visibility_voter_.SetVotingChannel(std::move(voting_channel));
  }

  ~GraphOwnedWrapper() override = default;

  GraphOwnedWrapper(const GraphOwnedWrapper&) = delete;
  GraphOwnedWrapper& operator=(const GraphOwnedWrapper&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override {
    graph->AddInitializingFrameNodeObserver(&frame_visibility_voter_);
  }
  void OnTakenFromGraph(Graph* graph) override {
    graph->RemoveInitializingFrameNodeObserver(&frame_visibility_voter_);
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return observer_; }

  VoterId voter_id() const { return voter_id_; }

 private:
  DummyVoteObserver observer_;
  FrameVisibilityVoter frame_visibility_voter_;
  VoterId voter_id_;
};

}  // namespace

class FrameVisibilityVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  FrameVisibilityVoterTest() = default;
  ~FrameVisibilityVoterTest() override = default;

  FrameVisibilityVoterTest(const FrameVisibilityVoterTest&) = delete;
  FrameVisibilityVoterTest& operator=(const FrameVisibilityVoterTest&) = delete;

  void SetUp() override {
    GetGraphFeatures().EnableExecutionContextRegistry();
    GetGraphFeatures().EnableFrameVisibilityDecorator();
    Super::SetUp();
    auto wrapper = std::make_unique<GraphOwnedWrapper>();
    wrapper_ = wrapper.get();
    graph()->PassToGraph(std::move(wrapper));
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return wrapper_->observer(); }

  VoterId voter_id() const { return wrapper_->voter_id(); }

 private:
  raw_ptr<GraphOwnedWrapper> wrapper_ = nullptr;
};

// Tests that the FrameVisibilityVoter correctly casts a vote for a frame
// depending on its visibility.
TEST_F(FrameVisibilityVoterTest, ChangeFrameVisibility) {
  // Create a graph with a single frame in a non-visible page. Its initial
  // visibility should be kNotVisible, resulting in a low priority.
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto& frame_node = mock_graph.frame;
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::TaskPriority::LOWEST,
                                 FrameVisibilityVoter::kFrameVisibilityReason));

  // Make the frame visible. This should increase the priority.
  mock_graph.frame->SetVisibility(FrameNode::Visibility::kVisible);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::TaskPriority::USER_VISIBLE,
                                 FrameVisibilityVoter::kFrameVisibilityReason));

  // Deleting the frame should invalidate the vote.
  frame_node.reset();
  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

}  // namespace execution_context_priority
}  // namespace performance_manager

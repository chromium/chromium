// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/loading_page_voter.h"

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/voting.h"

namespace performance_manager::execution_context_priority {

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  return execution_context::ExecutionContext::From(frame_node);
}

// Both the voting channel and the LoadingPageVoter are expected live on the
// graph, without being actual GraphOwned objects. This class wraps both to
// allow this.
class GraphOwnedWrapper : public GraphOwned {
 public:
  GraphOwnedWrapper()
      : loading_page_voter_(observer_.BuildVotingChannel()),
        voter_id_(loading_page_voter_.voter_id()) {}

  ~GraphOwnedWrapper() override = default;

  GraphOwnedWrapper(const GraphOwnedWrapper&) = delete;
  GraphOwnedWrapper& operator=(const GraphOwnedWrapper&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override {
    loading_page_voter_.InitializeOnGraph(graph);
  }
  void OnTakenFromGraph(Graph* graph) override {
    loading_page_voter_.TearDownOnGraph(graph);
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return observer_; }

  VoterId voter_id() const { return voter_id_; }

 private:
  DummyVoteObserver observer_;
  LoadingPageVoter loading_page_voter_;
  VoterId voter_id_;
};

}  // namespace

class LoadingPageVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  LoadingPageVoterTest() = default;
  ~LoadingPageVoterTest() override = default;

  LoadingPageVoterTest(const LoadingPageVoterTest&) = delete;
  LoadingPageVoterTest& operator=(const LoadingPageVoterTest&) = delete;

  void SetUp() override {
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

// Tests that the LoadingPageVoter correctly casts a vote for every frame when
// the page is loading.
TEST_F(LoadingPageVoterTest, VoteIfLoading) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto& frame_node = mock_graph.frame;
  auto& child_frame_node = mock_graph.child_frame;

  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(frame_node.get())));
  EXPECT_FALSE(observer().HasVote(voter_id(),
                                  GetExecutionContext(child_frame_node.get())));

  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoading);

  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::TaskPriority::USER_VISIBLE,
                                 LoadingPageVoter::kPageIsLoadingReason));
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(child_frame_node.get()),
                                 base::TaskPriority::USER_VISIBLE,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Still voting when the page is in the state kLoadedBusy.
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoadedBusy);

  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::TaskPriority::USER_VISIBLE,
                                 LoadingPageVoter::kPageIsLoadingReason));
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(child_frame_node.get()),
                                 base::TaskPriority::USER_VISIBLE,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Add a frame while the page is loading.
  auto other_child_frame_node = graph()->CreateFrameNodeAutoId(
      mock_graph.process.get(), mock_graph.page.get(), frame_node.get());

  EXPECT_EQ(observer().GetVoteCount(), 3u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::TaskPriority::USER_VISIBLE,
                                 LoadingPageVoter::kPageIsLoadingReason));
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(child_frame_node.get()),
                                 base::TaskPriority::USER_VISIBLE,
                                 LoadingPageVoter::kPageIsLoadingReason));
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(other_child_frame_node.get()),
      base::TaskPriority::USER_VISIBLE,
      LoadingPageVoter::kPageIsLoadingReason));

  // Remove a frame while the page is loading.
  other_child_frame_node.reset();

  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::TaskPriority::USER_VISIBLE,
                                 LoadingPageVoter::kPageIsLoadingReason));
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(child_frame_node.get()),
                                 base::TaskPriority::USER_VISIBLE,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Finish loading.
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);

  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(frame_node.get())));
  EXPECT_FALSE(observer().HasVote(voter_id(),
                                  GetExecutionContext(child_frame_node.get())));
}

}  // namespace performance_manager::execution_context_priority

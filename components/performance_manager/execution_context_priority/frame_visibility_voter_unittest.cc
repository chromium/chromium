// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/frame_visibility_voter.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/features.h"
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

}  // namespace

class FrameVisibilityVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  explicit FrameVisibilityVoterTest(bool ignore_main_frame_visibility = false)
      : frame_visibility_voter_(ignore_main_frame_visibility) {}
  ~FrameVisibilityVoterTest() override = default;

  FrameVisibilityVoterTest(const FrameVisibilityVoterTest&) = delete;
  FrameVisibilityVoterTest& operator=(const FrameVisibilityVoterTest&) = delete;

  void SetUp() override {
    Super::SetUp();
    frame_visibility_voter_.InitializeOnGraph(graph(),
                                              observer_.BuildVotingChannel());
  }

  void TearDown() override {
    frame_visibility_voter_.TearDownOnGraph(graph());
    Super::TearDown();
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return observer_; }

  VoterId voter_id() const { return frame_visibility_voter_.voter_id(); }

 private:
  DummyVoteObserver observer_;
  FrameVisibilityVoter frame_visibility_voter_;
};

// Tests that the FrameVisibilityVoter correctly casts a vote for a frame
// depending on its visibility.
TEST_F(FrameVisibilityVoterTest, ChangeFrameVisibility) {
  // Create a graph with a single frame. Its initial visibility should be
  // kUnknown, resulting in a high priority.
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto& frame_node = mock_graph.frame;
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kUnknown);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::Process::Priority::kUserBlocking,
                                 FrameVisibilityVoter::kFrameVisibilityReason));

  // Make the frame not visible. This should lower the priority.
  frame_node->SetVisibility(FrameNode::Visibility::kNotVisible);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::Process::Priority::kBestEffort,
                                 FrameVisibilityVoter::kFrameVisibilityReason));

  // Make the frame visible. This should increase the priority.
  frame_node->SetVisibility(FrameNode::Visibility::kVisible);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::Process::Priority::kUserBlocking,
                                 FrameVisibilityVoter::kFrameVisibilityReason));

  // Deleting the frame should invalidate the vote.
  frame_node.reset();
  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

// Tests that the FrameVisibilityVoter correctly casts a USER_VISIBLE vote for a
// unimportant frame that is visible.
TEST_F(FrameVisibilityVoterTest, UnimportantFrames) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kUnimportantFramesPriority);

  // Create a graph with a child frame as only child frames can be unimportant.
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());

  auto& frame_node = mock_graph.child_frame;

  // Make the frame visible and unimportant.
  frame_node->SetVisibility(FrameNode::Visibility::kVisible);
  frame_node->SetIsImportant(false);

  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::Process::Priority::kUserVisible,
                                 FrameVisibilityVoter::kFrameVisibilityReason));
}

// Tests that a speculative frame replacing a visible active frame receives
// USER_BLOCKING priority.
TEST_F(FrameVisibilityVoterTest, SpeculativeFrameReplacingVisibleFrame) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.frame->SetVisibility(FrameNode::Visibility::kVisible);

  auto speculative_frame = CreateSpeculativeFrameNodeAutoId(
      mock_graph.process.get(), mock_graph.page.get(),
      /*parent_frame_node=*/nullptr, kBrowsingInstanceForPage,
      mock_graph.frame->GetFrameTreeNodeId());

  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(speculative_frame.get()),
      base::Process::Priority::kUserBlocking,
      FrameVisibilityVoter::kSpeculativeFrameReason));
}

// Tests that a speculative frame replacing an active frame with unknown
// visibility receives USER_BLOCKING priority.
TEST_F(FrameVisibilityVoterTest,
       SpeculativeFrameReplacingUnknownVisibilityFrame) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.frame->SetVisibility(FrameNode::Visibility::kUnknown);

  auto speculative_frame = CreateSpeculativeFrameNodeAutoId(
      mock_graph.process.get(), mock_graph.page.get(),
      /*parent_frame_node=*/nullptr, kBrowsingInstanceForPage,
      mock_graph.frame->GetFrameTreeNodeId());

  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(speculative_frame.get()),
      base::Process::Priority::kUserBlocking,
      FrameVisibilityVoter::kSpeculativeFrameReason));
}

// Tests that a speculative frame replacing a non-visible active frame receives
// BEST_EFFORT priority.
TEST_F(FrameVisibilityVoterTest, SpeculativeFrameReplacingNonVisibleFrame) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.frame->SetVisibility(FrameNode::Visibility::kNotVisible);

  auto speculative_frame = CreateSpeculativeFrameNodeAutoId(
      mock_graph.process.get(), mock_graph.page.get(),
      /*parent_frame_node=*/nullptr, kBrowsingInstanceForPage,
      mock_graph.frame->GetFrameTreeNodeId());

  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(speculative_frame.get()),
                                 base::Process::Priority::kBestEffort,
                                 FrameVisibilityVoter::kFrameVisibilityReason));
}

// Tests that changes to an active frame's visibility update any speculative
// frame replacing it.
TEST_F(FrameVisibilityVoterTest, ActiveFrameVisibilityChanges) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.frame->SetVisibility(FrameNode::Visibility::kVisible);

  auto speculative_frame = CreateSpeculativeFrameNodeAutoId(
      mock_graph.process.get(), mock_graph.page.get(),
      /*parent_frame_node=*/nullptr, kBrowsingInstanceForPage,
      mock_graph.frame->GetFrameTreeNodeId());

  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(speculative_frame.get()),
      base::Process::Priority::kUserBlocking,
      FrameVisibilityVoter::kSpeculativeFrameReason));

  // Hide the active frame. The speculative frame's priority should drop.
  mock_graph.frame->SetVisibility(FrameNode::Visibility::kNotVisible);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(speculative_frame.get()),
                                 base::Process::Priority::kBestEffort,
                                 FrameVisibilityVoter::kFrameVisibilityReason));

  // Show the active frame again. The speculative frame's priority should
  // increase.
  mock_graph.frame->SetVisibility(FrameNode::Visibility::kVisible);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(speculative_frame.get()),
      base::Process::Priority::kUserBlocking,
      FrameVisibilityVoter::kSpeculativeFrameReason));
}

// Tests that changes to an active frame's importance update any speculative
// frame replacing it.
TEST_F(FrameVisibilityVoterTest, ActiveFrameImportanceChanges) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kUnimportantFramesPriority);

  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  mock_graph.child_frame->SetVisibility(FrameNode::Visibility::kVisible);

  auto speculative_frame = CreateSpeculativeFrameNodeAutoId(
      mock_graph.other_process.get(), mock_graph.page.get(),
      mock_graph.frame.get(), kBrowsingInstanceForPage,
      mock_graph.child_frame->GetFrameTreeNodeId());

  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(speculative_frame.get()),
      base::Process::Priority::kUserBlocking,
      FrameVisibilityVoter::kSpeculativeFrameReason));

  // Mark the active child frame as unimportant.
  mock_graph.child_frame->SetIsImportant(false);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(speculative_frame.get()),
      base::Process::Priority::kUserVisible,
      FrameVisibilityVoter::kSpeculativeFrameReason));

  // Mark it as important again.
  mock_graph.child_frame->SetIsImportant(true);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(speculative_frame.get()),
      base::Process::Priority::kUserBlocking,
      FrameVisibilityVoter::kSpeculativeFrameReason));
}

// Tests that frame swap correctly updates votes when a speculative frame
// becomes active and the old frame becomes inactive.
TEST_F(FrameVisibilityVoterTest, SpeculativeFrameSwap) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.frame->SetVisibility(FrameNode::Visibility::kVisible);

  auto speculative_frame = CreateSpeculativeFrameNodeAutoId(
      mock_graph.process.get(), mock_graph.page.get(),
      /*parent_frame_node=*/nullptr, kBrowsingInstanceForPage,
      mock_graph.frame->GetFrameTreeNodeId());

  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(speculative_frame.get()),
      base::Process::Priority::kUserBlocking,
      FrameVisibilityVoter::kSpeculativeFrameReason));

  // Simulate seamless navigation commit: speculative becomes active and
  // visible.
  speculative_frame->SetIsActive(true);
  speculative_frame->SetVisibility(FrameNode::Visibility::kVisible);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(speculative_frame.get()),
                                 base::Process::Priority::kUserBlocking,
                                 FrameVisibilityVoter::kFrameVisibilityReason));

  // Old frame becomes inactive and not visible.
  mock_graph.frame->SetIsActive(false);
  mock_graph.frame->SetVisibility(FrameNode::Visibility::kNotVisible);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(mock_graph.frame.get()),
                                 base::Process::Priority::kBestEffort,
                                 FrameVisibilityVoter::kFrameVisibilityReason));
}

// Tests that an inactive frame that does not correspond to an active frame
// receives BEST_EFFORT priority.
TEST_F(FrameVisibilityVoterTest, PrerenderedOrDetachedFrame) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());

  auto prerender_frame = CreateSpeculativeFrameNodeAutoId(
      mock_graph.process.get(), mock_graph.page.get(),
      /*parent_frame_node=*/nullptr, kBrowsingInstanceForPage,
      /*frame_tree_node_id=*/NextTestFrameTreeNodeId());

  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(prerender_frame.get()),
                                 base::Process::Priority::kBestEffort,
                                 FrameVisibilityVoter::kFrameVisibilityReason));
}

// Tests that removing an active frame downgrades any speculative frame
// replacing it to BEST_EFFORT priority.
TEST_F(FrameVisibilityVoterTest, ActiveFrameRemovedEarly) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.frame->SetVisibility(FrameNode::Visibility::kVisible);

  auto speculative_frame = CreateSpeculativeFrameNodeAutoId(
      mock_graph.process.get(), mock_graph.page.get(),
      /*parent_frame_node=*/nullptr, kBrowsingInstanceForPage,
      mock_graph.frame->GetFrameTreeNodeId());

  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(speculative_frame.get()),
      base::Process::Priority::kUserBlocking,
      FrameVisibilityVoter::kSpeculativeFrameReason));

  // Remove the active frame. The speculative frame's priority should drop.
  mock_graph.frame.reset();
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(speculative_frame.get()),
                                 base::Process::Priority::kBestEffort,
                                 FrameVisibilityVoter::kFrameVisibilityReason));
}

class FrameVisibilityVoterIgnoreMainFrameTest
    : public FrameVisibilityVoterTest {
 public:
  FrameVisibilityVoterIgnoreMainFrameTest()
      : FrameVisibilityVoterTest(/*ignore_main_frame_visibility=*/true) {}
};

TEST_F(FrameVisibilityVoterIgnoreMainFrameTest, IgnoreMainFrameVisibility) {
  {
    MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
    EXPECT_EQ(mock_graph.frame->GetVisibility(),
              FrameNode::Visibility::kUnknown);
    // Main frame should have no vote.
    EXPECT_FALSE(observer().HasVote(
        voter_id(), GetExecutionContext(mock_graph.frame.get())));

    EXPECT_EQ(mock_graph.child_frame->GetVisibility(),
              FrameNode::Visibility::kUnknown);
    // Child frame should have a vote!
    EXPECT_TRUE(observer().HasVote(
        voter_id(), GetExecutionContext(mock_graph.child_frame.get()),
        base::Process::Priority::kUserBlocking,
        FrameVisibilityVoter::kFrameVisibilityReason));

    // Changing visibility of main frame should not cast any vote.
    mock_graph.frame->SetVisibility(FrameNode::Visibility::kVisible);
    EXPECT_FALSE(observer().HasVote(
        voter_id(), GetExecutionContext(mock_graph.frame.get())));
    mock_graph.frame->SetVisibility(FrameNode::Visibility::kNotVisible);
    EXPECT_FALSE(observer().HasVote(
        voter_id(), GetExecutionContext(mock_graph.frame.get())));

    // Changing visibility of child frame should update its vote.
    mock_graph.child_frame->SetVisibility(FrameNode::Visibility::kNotVisible);
    EXPECT_TRUE(observer().HasVote(
        voter_id(), GetExecutionContext(mock_graph.child_frame.get()),
        base::Process::Priority::kBestEffort,
        FrameVisibilityVoter::kFrameVisibilityReason));
  }
}

TEST_F(FrameVisibilityVoterIgnoreMainFrameTest, SpeculativeFrames) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  mock_graph.frame->SetVisibility(FrameNode::Visibility::kVisible);
  mock_graph.child_frame->SetVisibility(FrameNode::Visibility::kVisible);

  // Speculative main frame should receive no vote because main frame visibility
  // is ignored.
  auto speculative_main_frame = CreateSpeculativeFrameNodeAutoId(
      mock_graph.process.get(), mock_graph.page.get(),
      /*parent_frame_node=*/nullptr, kBrowsingInstanceForPage,
      mock_graph.frame->GetFrameTreeNodeId());
  EXPECT_FALSE(observer().HasVote(
      voter_id(), GetExecutionContext(speculative_main_frame.get())));

  // Speculative child frame should receive a USER_BLOCKING vote.
  auto speculative_child_frame = CreateSpeculativeFrameNodeAutoId(
      mock_graph.other_process.get(), mock_graph.page.get(),
      mock_graph.frame.get(), kBrowsingInstanceForPage,
      mock_graph.child_frame->GetFrameTreeNodeId());
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(speculative_child_frame.get()),
      base::Process::Priority::kUserBlocking,
      FrameVisibilityVoter::kSpeculativeFrameReason));
}

}  // namespace execution_context_priority
}  // namespace performance_manager

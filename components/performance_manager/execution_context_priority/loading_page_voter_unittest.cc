// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/loading_page_voter.h"

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
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

class LoadingPageVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  LoadingPageVoterTest() = default;
  ~LoadingPageVoterTest() override = default;

  LoadingPageVoterTest(const LoadingPageVoterTest&) = delete;
  LoadingPageVoterTest& operator=(const LoadingPageVoterTest&) = delete;

  void SetUp() override {
    Super::SetUp();
    graph()->PassToGraph(std::make_unique<PageLiveStateDecorator>());
    loading_page_voter_.InitializeOnGraph(graph(),
                                          observer_.BuildVotingChannel());
  }

  void TearDown() override {
    loading_page_voter_.TearDownOnGraph(graph());
    Super::TearDown();
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return observer_; }

  VoterId voter_id() const { return loading_page_voter_.voter_id(); }

 private:
  DummyVoteObserver observer_;
  LoadingPageVoter loading_page_voter_;
};

}  // namespace

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
                                 base::Process::Priority::kUserVisible,
                                 LoadingPageVoter::kPageIsLoadingReason));
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(child_frame_node.get()),
                                 base::Process::Priority::kUserVisible,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Still voting when the page is in the state kLoadedBusy.
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoadedBusy);

  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::Process::Priority::kUserVisible,
                                 LoadingPageVoter::kPageIsLoadingReason));
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(child_frame_node.get()),
                                 base::Process::Priority::kUserVisible,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Add a frame while the page is loading.
  auto other_child_frame_node = graph()->CreateFrameNodeAutoId(
      mock_graph.process.get(), mock_graph.page.get(), frame_node.get());

  EXPECT_EQ(observer().GetVoteCount(), 3u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::Process::Priority::kUserVisible,
                                 LoadingPageVoter::kPageIsLoadingReason));
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(child_frame_node.get()),
                                 base::Process::Priority::kUserVisible,
                                 LoadingPageVoter::kPageIsLoadingReason));
  EXPECT_TRUE(observer().HasVote(
      voter_id(), GetExecutionContext(other_child_frame_node.get()),
      base::Process::Priority::kUserVisible,
      LoadingPageVoter::kPageIsLoadingReason));

  // Remove a frame while the page is loading.
  other_child_frame_node.reset();

  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::Process::Priority::kUserVisible,
                                 LoadingPageVoter::kPageIsLoadingReason));
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(child_frame_node.get()),
                                 base::Process::Priority::kUserVisible,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Finish loading.
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);

  EXPECT_EQ(observer().GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(frame_node.get())));
  EXPECT_FALSE(observer().HasVote(voter_id(),
                                  GetExecutionContext(child_frame_node.get())));
}

TEST_F(LoadingPageVoterTest, IdenticalPriorityNoCrash) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto& embedder_frame_node = mock_graph.frame;
  auto& embedder_child_frame = mock_graph.child_frame;

  PageLiveStateDecorator::Data::GetOrCreateForPageNode(mock_graph.page.get())
      ->SetIsActiveTabForTesting(true);

  auto embedded_page_node = CreateNode<PageNodeImpl>();
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(
      embedded_page_node.get());
  embedded_page_node->SetEmbedderFrameNode(embedder_frame_node.get());
  auto embedded_frame_node =
      CreateFrameNodeAutoId(mock_graph.process.get(), embedded_page_node.get());

  embedded_page_node->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(embedded_frame_node.get()),
                                 base::Process::Priority::kUserBlocking,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Moving the embedded page to another frame in the same active tab page
  // should not crash due to identical ChangeVote calls.
  embedded_page_node->SetEmbedderFrameNode(embedder_child_frame.get());
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(embedded_frame_node.get()),
                                 base::Process::Priority::kUserBlocking,
                                 LoadingPageVoter::kPageIsLoadingReason));
}

TEST_F(LoadingPageVoterTest, VoteWhenActiveTabAndLoading) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto& frame_node = mock_graph.frame;
  auto& child_frame_node = mock_graph.child_frame;

  EXPECT_EQ(observer().GetVoteCount(), 0u);

  // Start loading while not the active tab -> votes kUserVisible.
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::Process::Priority::kUserVisible,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Become active tab -> priority upgrades to kUserBlocking.
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(mock_graph.page.get())
      ->SetIsActiveTabForTesting(true);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::Process::Priority::kUserBlocking,
                                 LoadingPageVoter::kPageIsLoadingReason));
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(child_frame_node.get()),
                                 base::Process::Priority::kUserBlocking,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Become inactive tab -> priority downgrades to kUserVisible.
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(mock_graph.page.get())
      ->SetIsActiveTabForTesting(false);
  EXPECT_EQ(observer().GetVoteCount(), 2u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(frame_node.get()),
                                 base::Process::Priority::kUserVisible,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Finish loading -> vote invalidated.
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

TEST_F(LoadingPageVoterTest, EmbeddedPageBoosted) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto& embedder_frame_node = mock_graph.frame;

  PageLiveStateDecorator::Data::GetOrCreateForPageNode(mock_graph.page.get())
      ->SetIsActiveTabForTesting(true);

  // Outer page is NOT loading.
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(observer().GetVoteCount(), 0u);

  // Create an embedded page.
  auto embedded_page_node = CreateNode<PageNodeImpl>();
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(
      embedded_page_node.get());
  embedded_page_node->SetEmbedderFrameNode(embedder_frame_node.get());

  auto embedded_frame_node =
      CreateFrameNodeAutoId(mock_graph.process.get(), embedded_page_node.get());

  // Embedded page starts loading -> inherits active-tab status from
  // embedder root page, so it receives kUserBlocking vote.
  embedded_page_node->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(embedded_frame_node.get()),
                                 base::Process::Priority::kUserBlocking,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Outer page becomes inactive -> embedded page priority downgrades to
  // kUserVisible.
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(mock_graph.page.get())
      ->SetIsActiveTabForTesting(false);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(embedded_frame_node.get()),
                                 base::Process::Priority::kUserVisible,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Outer page becomes active again -> embedded page priority upgrades to
  // kUserBlocking.
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(mock_graph.page.get())
      ->SetIsActiveTabForTesting(true);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(embedded_frame_node.get()),
                                 base::Process::Priority::kUserBlocking,
                                 LoadingPageVoter::kPageIsLoadingReason));

  embedded_page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

TEST_F(LoadingPageVoterTest, EmbeddedPageInSubframeBoosted) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto& embedder_child_frame = mock_graph.child_frame;

  PageLiveStateDecorator::Data::GetOrCreateForPageNode(mock_graph.page.get())
      ->SetIsActiveTabForTesting(true);

  // Outer page is NOT loading.
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(observer().GetVoteCount(), 0u);

  // Create an embedded page attached to a child frame (subframe).
  auto embedded_page_node = CreateNode<PageNodeImpl>();
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(
      embedded_page_node.get());
  embedded_page_node->SetEmbedderFrameNode(embedder_child_frame.get());

  auto embedded_frame_node =
      CreateFrameNodeAutoId(mock_graph.process.get(), embedded_page_node.get());

  // Embedded page starts loading -> inherits active-tab status from
  // embedder root page, so it receives kUserBlocking vote.
  embedded_page_node->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(embedded_frame_node.get()),
                                 base::Process::Priority::kUserBlocking,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Outer page becomes inactive -> embedded page priority downgrades to
  // kUserVisible.
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(mock_graph.page.get())
      ->SetIsActiveTabForTesting(false);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(embedded_frame_node.get()),
                                 base::Process::Priority::kUserVisible,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Outer page becomes active again -> embedded page priority upgrades to
  // kUserBlocking.
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(mock_graph.page.get())
      ->SetIsActiveTabForTesting(true);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(embedded_frame_node.get()),
                                 base::Process::Priority::kUserBlocking,
                                 LoadingPageVoter::kPageIsLoadingReason));

  embedded_page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

TEST_F(LoadingPageVoterTest, EmbeddedPageAttachedWhileLoading) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto& embedder_frame_node = mock_graph.frame;

  PageLiveStateDecorator::Data::GetOrCreateForPageNode(mock_graph.page.get())
      ->SetIsActiveTabForTesting(true);

  // Create an embedded page, initially unattached to the embedder frame.
  auto embedded_page_node = CreateNode<PageNodeImpl>();
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(
      embedded_page_node.get());

  auto embedded_frame_node =
      CreateFrameNodeAutoId(mock_graph.process.get(), embedded_page_node.get());

  // Embedded page starts loading before being attached to an embedder. Since
  // it is not yet an active tab, it receives kUserVisible.
  embedded_page_node->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(embedded_frame_node.get()),
                                 base::Process::Priority::kUserVisible,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Attach to embedder -> OnEmbedderFrameNodeChanged is called, re-evaluating
  // votes and assigning kUserBlocking from the active tab embedder root page.
  embedded_page_node->SetEmbedderFrameNode(embedder_frame_node.get());
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(embedded_frame_node.get()),
                                 base::Process::Priority::kUserBlocking,
                                 LoadingPageVoter::kPageIsLoadingReason));

  embedded_page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

TEST_F(LoadingPageVoterTest, EmbedderFrameChangedWhenOuterPageNotLoading) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto& embedder_frame_node = mock_graph.frame;

  PageLiveStateDecorator::Data::GetOrCreateForPageNode(mock_graph.page.get())
      ->SetIsActiveTabForTesting(true);
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);

  // Create an embedded page attached to the active tab embedder frame.
  auto embedded_page_node = CreateNode<PageNodeImpl>();
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(
      embedded_page_node.get());
  embedded_page_node->SetEmbedderFrameNode(embedder_frame_node.get());

  auto embedded_frame_node =
      CreateFrameNodeAutoId(mock_graph.process.get(), embedded_page_node.get());

  embedded_page_node->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(embedded_frame_node.get()),
                                 base::Process::Priority::kUserBlocking,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Create another page node that is inactive and NOT loading.
  auto inactive_page_node = CreateNode<PageNodeImpl>();
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(
      inactive_page_node.get());
  inactive_page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  auto inactive_frame_node =
      CreateFrameNodeAutoId(mock_graph.process.get(), inactive_page_node.get());

  // Attach embedded_page_node to inactive_frame_node. Even though
  // inactive_page_node is NOT loading, OnEmbedderFrameNodeChanged should update
  // embedded_page_node's vote (downgrading it to kUserVisible because the root
  // page is no longer the active tab).
  embedded_page_node->SetEmbedderFrameNode(inactive_frame_node.get());
  EXPECT_EQ(observer().GetVoteCount(), 1u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(embedded_frame_node.get()),
                                 base::Process::Priority::kUserVisible,
                                 LoadingPageVoter::kPageIsLoadingReason));

  embedded_page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
}

TEST_F(LoadingPageVoterTest, SimultaneousOuterAndEmbeddedPageLoading) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto& outer_frame_node = mock_graph.frame;

  PageLiveStateDecorator::Data::GetOrCreateForPageNode(mock_graph.page.get())
      ->SetIsActiveTabForTesting(true);

  auto embedded_page_node = CreateNode<PageNodeImpl>();
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(
      embedded_page_node.get());
  embedded_page_node->SetEmbedderFrameNode(outer_frame_node.get());
  auto embedded_frame_node =
      CreateFrameNodeAutoId(mock_graph.process.get(), embedded_page_node.get());

  // Both outer page and embedded page start loading simultaneously.
  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoading);
  embedded_page_node->SetLoadingState(PageNode::LoadingState::kLoading);

  // Both outer page main frames (frame, child_frame) and embedded frame get
  // kUserBlocking.
  EXPECT_EQ(observer().GetVoteCount(), 3u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(outer_frame_node.get()),
                                 base::Process::Priority::kUserBlocking,
                                 LoadingPageVoter::kPageIsLoadingReason));
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(embedded_frame_node.get()),
                                 base::Process::Priority::kUserBlocking,
                                 LoadingPageVoter::kPageIsLoadingReason));

  // Become inactive tab -> both outer frames and embedded frame downgrade to
  // kUserVisible.
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(mock_graph.page.get())
      ->SetIsActiveTabForTesting(false);
  EXPECT_EQ(observer().GetVoteCount(), 3u);
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(outer_frame_node.get()),
                                 base::Process::Priority::kUserVisible,
                                 LoadingPageVoter::kPageIsLoadingReason));
  EXPECT_TRUE(observer().HasVote(voter_id(),
                                 GetExecutionContext(embedded_frame_node.get()),
                                 base::Process::Priority::kUserVisible,
                                 LoadingPageVoter::kPageIsLoadingReason));

  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  embedded_page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

TEST_F(LoadingPageVoterTest, OnIsActiveTabChangedOnEmbeddedPageNoCrash) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto& outer_frame_node = mock_graph.frame;

  auto embedded_page_node = CreateNode<PageNodeImpl>();
  auto* data = PageLiveStateDecorator::Data::GetOrCreateForPageNode(
      embedded_page_node.get());
  embedded_page_node->SetEmbedderFrameNode(outer_frame_node.get());

  // Toggling IsActiveTab on an embedded page should safely return without
  // CHECK-crashing.
  data->SetIsActiveTabForTesting(true);
  data->SetIsActiveTabForTesting(false);
}

TEST_F(LoadingPageVoterTest, PageNodeRemovedWhileLoading) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());

  mock_graph.page->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(observer().GetVoteCount(), 2u);

  // In PerformanceManager, frames must leave the graph before their page node.
  mock_graph.child_frame.reset();
  mock_graph.frame.reset();
  mock_graph.page.reset();
  EXPECT_EQ(observer().GetVoteCount(), 0u);
}

}  // namespace performance_manager::execution_context_priority

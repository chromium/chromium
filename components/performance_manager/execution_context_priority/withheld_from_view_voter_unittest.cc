// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/withheld_from_view_voter.h"

#include <memory>
#include <utility>

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/voting.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::execution_context_priority {

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;
using ScopedWithheldFromView = PageLiveStateDecorator::ScopedWithheldFromView;

namespace {

std::unique_ptr<ScopedWithheldFromView> MarkWithheldFromView(
    const PageNode* page_node) {
  return PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->MarkWithheldFromViewForTesting();
}

// Base harness, parameterized on whether main frame visibility is ignored so
// that both configurations exercise the same code paths.
class WithheldFromViewVoterTestBase : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  explicit WithheldFromViewVoterTestBase(bool ignore_main_frame_visibility)
      : voter_(ignore_main_frame_visibility) {}
  ~WithheldFromViewVoterTestBase() override = default;

  WithheldFromViewVoterTestBase(const WithheldFromViewVoterTestBase&) = delete;
  WithheldFromViewVoterTestBase& operator=(
      const WithheldFromViewVoterTestBase&) = delete;

  void SetUp() override {
    Super::SetUp();
    graph()->PassToGraph(std::make_unique<PageLiveStateDecorator>());
    voter_.InitializeOnGraph(graph(), observer_.BuildVotingChannel());
  }

  void TearDown() override {
    voter_.TearDownOnGraph(graph());
    Super::TearDown();
  }

  VoterId voter_id() const { return voter_.voter_id(); }

  DummyVoteObserver observer_;
  WithheldFromViewVoter voter_;
};

class WithheldFromViewVoterTest : public WithheldFromViewVoterTestBase {
 public:
  WithheldFromViewVoterTest()
      : WithheldFromViewVoterTestBase(/*ignore_main_frame_visibility=*/false) {}
};

class WithheldFromViewVoterIgnoreMainFrameTest
    : public WithheldFromViewVoterTestBase {
 public:
  WithheldFromViewVoterIgnoreMainFrameTest()
      : WithheldFromViewVoterTestBase(/*ignore_main_frame_visibility=*/true) {}
};

}  // namespace

// Unlike GlicActuationPriorityVoter, this voter boosts every frame of the page:
// a cross-process subframe must also load before the embedder can show it.
TEST_F(WithheldFromViewVoterTest, VotesForAllFramesWhileWithheld) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();
  auto* main_frame_node = mock_graph.frame.get();
  auto* child_frame_node = mock_graph.child_frame.get();

  // A page nobody is withholding gets no votes.
  EXPECT_EQ(observer_.GetVoteCount(), 0u);

  auto token = MarkWithheldFromView(page_node);
  EXPECT_EQ(observer_.GetVoteCount(), 2u);
  EXPECT_TRUE(observer_.HasVote(
      voter_id(), main_frame_node, base::Process::Priority::kUserBlocking,
      WithheldFromViewVoter::kWithheldFromViewReason));
  EXPECT_TRUE(observer_.HasVote(
      voter_id(), child_frame_node, base::Process::Priority::kUserBlocking,
      WithheldFromViewVoter::kWithheldFromViewReason));

  // Releasing the token withdraws every vote.
  token.reset();
  EXPECT_EQ(observer_.GetVoteCount(), 0u);
  EXPECT_FALSE(observer_.HasVote(voter_id(), main_frame_node));
  EXPECT_FALSE(observer_.HasVote(voter_id(), child_frame_node));
}

// The boost is independent of visibility: a withheld page that becomes visible
// keeps its vote until the embedder releases the token, at which point the
// regular visibility voters are the only ones voting.
TEST_F(WithheldFromViewVoterTest, VoteSurvivesVisibilityChange) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();

  auto token = MarkWithheldFromView(page_node);
  EXPECT_EQ(observer_.GetVoteCount(), 1u);

  mock_graph.page->SetIsVisible(true);
  EXPECT_TRUE(observer_.HasVote(voter_id(), mock_graph.frame.get()));

  token.reset();
  EXPECT_EQ(observer_.GetVoteCount(), 0u);
}

// A frame created while the page is already being withheld must be voted for
// too, otherwise a subframe that appears mid-load is left at minimum priority.
TEST_F(WithheldFromViewVoterTest, VotesForFrameAddedWhileWithheld) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();

  auto token = MarkWithheldFromView(page_node);
  EXPECT_EQ(observer_.GetVoteCount(), 1u);

  auto child_frame_node = CreateFrameNodeAutoId(
      mock_graph.process.get(), page_node, mock_graph.frame.get());
  EXPECT_EQ(observer_.GetVoteCount(), 2u);
  EXPECT_TRUE(
      observer_.HasVote(voter_id(), child_frame_node.get(),
                        base::Process::Priority::kUserBlocking,
                        WithheldFromViewVoter::kWithheldFromViewReason));

  // Removing it withdraws its vote and leaves the main frame's intact.
  child_frame_node.reset();
  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_TRUE(observer_.HasVote(voter_id(), mock_graph.frame.get()));
}

// A token that outlives its page must not resurrect or crash.
TEST_F(WithheldFromViewVoterTest, TokenOutlivingPageIsInert) {
  std::unique_ptr<ScopedWithheldFromView> token;
  {
    MockSinglePageInSingleProcessGraph mock_graph(graph());
    token = MarkWithheldFromView(mock_graph.page.get());
    EXPECT_EQ(observer_.GetVoteCount(), 1u);
  }
  EXPECT_EQ(observer_.GetVoteCount(), 0u);
  token.reset();
  EXPECT_EQ(observer_.GetVoteCount(), 0u);
}

// The voter honors ignore_main_frame_visibility identically to
// FrameVisibilityVoter: an embedder that has excluded main frame visibility
// from priority has excluded withheld main frames too.
TEST_F(WithheldFromViewVoterIgnoreMainFrameTest, DoesNotVoteForMainFrame) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();
  auto* main_frame_node = mock_graph.frame.get();
  auto* child_frame_node = mock_graph.child_frame.get();

  auto token = MarkWithheldFromView(page_node);

  // Only the child frame is voted for.
  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_FALSE(observer_.HasVote(voter_id(), main_frame_node));
  EXPECT_TRUE(observer_.HasVote(
      voter_id(), child_frame_node, base::Process::Priority::kUserBlocking,
      WithheldFromViewVoter::kWithheldFromViewReason));

  token.reset();
  EXPECT_EQ(observer_.GetVoteCount(), 0u);
}

}  // namespace performance_manager::execution_context_priority

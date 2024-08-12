// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/root_vote_observer.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/voting.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace execution_context_priority {

namespace {

using testing::_;

static const char kReason[] = "test reason";

class LenientMockFrameNodeObserver : public FrameNode::ObserverDefaultImpl {
 public:
  LenientMockFrameNodeObserver() = default;
  LenientMockFrameNodeObserver(const LenientMockFrameNodeObserver&) = delete;
  LenientMockFrameNodeObserver& operator=(const LenientMockFrameNodeObserver&) =
      delete;
  ~LenientMockFrameNodeObserver() override = default;

  MOCK_METHOD(void,
              OnPriorityAndReasonChanged,
              (const FrameNode*, const PriorityAndReason&),
              (override));
};

using MockFrameNodeObserver =
    ::testing::StrictMock<LenientMockFrameNodeObserver>;

using RootVoteObserverTest = GraphTestHarness;

}  // namespace

TEST_F(RootVoteObserverTest, VotesForwardedToGraph) {
  RootVoteObserver root_vote_observer;

  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto& frame = mock_graph.frame;

  auto* execution_context =
      execution_context::ExecutionContext::From(frame.get());

  MockFrameNodeObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  VotingChannel voter = root_vote_observer.GetVotingChannel();

  // The priority and reason starts with a default value.
  static const PriorityAndReason kDefaultPriorityAndReason(
      base::TaskPriority::LOWEST, FrameNodeImpl::kDefaultPriorityReason);
  EXPECT_EQ(frame->GetPriorityAndReason(), kDefaultPriorityAndReason);

  // Do not expect a notification when an identical vote is submitted.
  voter.SubmitVote(execution_context, Vote(kDefaultPriorityAndReason.priority(),
                                           kDefaultPriorityAndReason.reason()));
  testing::Mock::VerifyAndClear(&obs);

  // Update the vote with a new priority and expect that to propagate.
  EXPECT_CALL(obs, OnPriorityAndReasonChanged(frame.get(), _));
  voter.ChangeVote(execution_context,
                   Vote(base::TaskPriority::HIGHEST, kReason));

  testing::Mock::VerifyAndClear(&obs);
  EXPECT_EQ(frame->GetPriorityAndReason().priority(),
            base::TaskPriority::HIGHEST);
  EXPECT_EQ(frame->GetPriorityAndReason().reason(), kReason);

  // Cancel the existing vote and expect it to go back to the default.
  EXPECT_CALL(obs, OnPriorityAndReasonChanged(frame.get(), _));
  voter.InvalidateVote(execution_context);
  testing::Mock::VerifyAndClear(&obs);
  EXPECT_EQ(frame->GetPriorityAndReason(), kDefaultPriorityAndReason);

  graph()->RemoveFrameNodeObserver(&obs);
}

}  // namespace execution_context_priority
}  // namespace performance_manager

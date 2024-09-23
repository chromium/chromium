// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/page_load_tracker_decorator.h"

#include <memory>
#include <type_traits>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

// Aliasing these here makes this unittest much more legible.
using Data = PageLoadTrackerDecorator::Data;
using LIS = Data::LoadIdleState;
using LS = PageNode::LoadingState;

class PageLoadTrackerDecoratorTest : public GraphTestHarness {
 public:
  PageLoadTrackerDecoratorTest(const PageLoadTrackerDecoratorTest&) = delete;
  PageLoadTrackerDecoratorTest& operator=(const PageLoadTrackerDecoratorTest&) =
      delete;

 protected:
  using Super = GraphTestHarness;

  PageLoadTrackerDecoratorTest() = default;
  ~PageLoadTrackerDecoratorTest() override = default;

  void SetUp() override {
    Super::SetUp();
    pltd_ = new PageLoadTrackerDecorator();
    graph()->PassToGraph(base::WrapUnique(pltd_.get()));
  }

  void TestPageAlmostIdleTransitions(bool timeout_waiting_for_response,
                                     bool timeout_waiting_for_idle);

  bool IsIdling(const PageNodeImpl* page_node) const {
    return PageLoadTrackerDecorator::IsIdling(page_node);
  }

  static constexpr base::TimeDelta GetWaitingForIdleTimeout() {
    return PageLoadTrackerDecorator::kWaitingForIdleTimeout;
  }

  raw_ptr<PageLoadTrackerDecorator> pltd_ = nullptr;
};

void PageLoadTrackerDecoratorTest::TestPageAlmostIdleTransitions(
    bool timeout_waiting_for_response,
    bool timeout_waiting_for_idle) {
  static const base::TimeDelta kLoadedAndIdlingTimeout =
      PageLoadTrackerDecorator::kLoadedAndIdlingTimeout;
  static const base::TimeDelta kWaitingForIdleTimeout =
      PageLoadTrackerDecorator::kWaitingForIdleTimeout;

  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* frame_node = mock_graph.frame.get();
  auto* page_node = mock_graph.page.get();
  auto* proc_node = mock_graph.process.get();

  // Initially the page should be in a loading not started state.
  EXPECT_FALSE(Data::Exists(page_node));
  EXPECT_EQ(LS::kLoadingNotStarted, page_node->GetLoadingState());

  // The state should transition to loading when DidStartLoading() is called to
  // indicate that loading starts.
  PageLoadTrackerDecorator::DidStartLoading(page_node);
  ASSERT_TRUE(Data::Exists(page_node));
  Data& page_data = Data::Get(page_node);
  EXPECT_EQ(LIS::kWaitingForNavigation, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoading, page_node->GetLoadingState());
  EXPECT_TRUE(page_data.timer_.IsRunning());
  EXPECT_EQ(page_data.timer_.GetCurrentDelay(),
            PageLoadTrackerDecorator::kWaitingForNavigationTimeout);

  if (timeout_waiting_for_response) {
    // Let the timeout run down. The page should transition to loading timed
    // out.
    task_env().FastForwardBy(
        PageLoadTrackerDecorator::kWaitingForNavigationTimeout);
    EXPECT_EQ(LIS::kWaitingForNavigationTimedOut, page_data.load_idle_state());
    EXPECT_EQ(LS::kLoadingTimedOut, page_node->GetLoadingState());
    EXPECT_FALSE(page_data.timer_.IsRunning());
  }

  // Indicate that a page change was committed. The state should transition to
  // kLoading (no matter whether the kWaitingForNavigationTimeout expired since
  // load started).
  PageLoadTrackerDecorator::PrimaryPageChanged(page_node);
  EXPECT_EQ(LIS::kLoading, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoading, page_node->GetLoadingState());
  EXPECT_FALSE(page_data.timer_.IsRunning());

  // Mark the page as idling. It should transition from
  // kLoading directly to kLoadedAndIdling after this.
  frame_node->SetNetworkAlmostIdle();
  proc_node->SetMainThreadTaskLoadIsLow(true);
  PageLoadTrackerDecorator::DidStopLoading(page_node);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoadedBusy, page_node->GetLoadingState());
  EXPECT_TRUE(page_data.timer_.IsRunning());

  // Go back to not idling. We should transition back to kLoadedNotIdling, and
  // a timer should still be running.
  frame_node->OnNavigationCommitted(
      GURL(), url::Origin(), /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  EXPECT_FALSE(frame_node->GetNetworkAlmostIdle());
  EXPECT_EQ(LIS::kLoadedNotIdling, page_data.load_idle_state());
  EXPECT_TRUE(page_data.timer_.IsRunning());

  if (timeout_waiting_for_idle) {
    // Let the timeout run down. The final state transition should occur.
    task_env().FastForwardBy(kWaitingForIdleTimeout);
    EXPECT_FALSE(Data::Exists(page_node));
    EXPECT_EQ(LS::kLoadedIdle, page_node->GetLoadingState());
  } else {
    // Go back to idling.
    frame_node->SetNetworkAlmostIdle();
    EXPECT_TRUE(frame_node->GetNetworkAlmostIdle());
    EXPECT_EQ(LIS::kLoadedAndIdling, page_data.load_idle_state());
    EXPECT_EQ(LS::kLoadedBusy, page_node->GetLoadingState());
    EXPECT_TRUE(page_data.timer_.IsRunning());

    // Let the idle timer evaluate. The final state transition should occur.
    task_env().FastForwardBy(kLoadedAndIdlingTimeout);
    EXPECT_FALSE(Data::Exists(page_node));
    EXPECT_EQ(LS::kLoadedIdle, page_node->GetLoadingState());
  }

  // Firing other signals should not change the state at all.
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(Data::Exists(page_node));
  EXPECT_EQ(LS::kLoadedIdle, page_node->GetLoadingState());
  frame_node->OnNavigationCommitted(
      GURL(), url::Origin(), /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  EXPECT_FALSE(frame_node->GetNetworkAlmostIdle());
  EXPECT_FALSE(Data::Exists(page_node));
  EXPECT_EQ(LS::kLoadedIdle, page_node->GetLoadingState());
}

TEST_F(PageLoadTrackerDecoratorTest, TestTransitionsNoTimeout) {
  TestPageAlmostIdleTransitions(false, false);
}

TEST_F(PageLoadTrackerDecoratorTest, TestTransitionsTimeoutWaitingForResponse) {
  TestPageAlmostIdleTransitions(true, false);
}

TEST_F(PageLoadTrackerDecoratorTest, TestTransitionsTimeoutWaitingForIdle) {
  TestPageAlmostIdleTransitions(false, true);
}

TEST_F(PageLoadTrackerDecoratorTest,
       TestTransitionsTimeoutWaitingForResponseAndWaitingForIdle) {
  TestPageAlmostIdleTransitions(true, true);
}

TEST_F(PageLoadTrackerDecoratorTest, TestTransitionsNotIdlingOnDidStopLoading) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* frame_node = mock_graph.frame.get();
  auto* page_node = mock_graph.page.get();
  auto* proc_node = mock_graph.process.get();

  // Initially the page should be in a loading not started state.
  EXPECT_FALSE(Data::Exists(page_node));
  EXPECT_EQ(LS::kLoadingNotStarted, page_node->GetLoadingState());

  // The state should transition to loading when PrimaryPageChanged() is called
  // to indicate that loading starts.
  PageLoadTrackerDecorator::DidStartLoading(page_node);
  PageLoadTrackerDecorator::PrimaryPageChanged(page_node);
  ASSERT_TRUE(Data::Exists(page_node));
  Data& page_data = Data::Get(page_node);
  EXPECT_EQ(LIS::kLoading, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoading, page_node->GetLoadingState());
  EXPECT_FALSE(page_data.timer_.IsRunning());

  // Mark the page as not idling.
  frame_node->OnNavigationCommitted(
      GURL(), url::Origin(), /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(IsIdling(page_node));

  // DidStopLoading() should cause a transition to kLoadedNotIdling.
  PageLoadTrackerDecorator::DidStopLoading(page_node);
  EXPECT_EQ(LIS::kLoadedNotIdling, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoadedBusy, page_node->GetLoadingState());
  EXPECT_TRUE(page_data.timer_.IsRunning());
}

TEST_F(PageLoadTrackerDecoratorTest, TestStartLoadingAgainBeforeIdle) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* frame_node = mock_graph.frame.get();
  auto* page_node = mock_graph.page.get();
  auto* proc_node = mock_graph.process.get();

  // Initially the page should be in a loading not started state.
  EXPECT_FALSE(Data::Exists(page_node));
  EXPECT_EQ(LS::kLoadingNotStarted, page_node->GetLoadingState());

  // The state should transition to loading when PrimaryPageChanged() is called
  // to indicate that loading starts.
  PageLoadTrackerDecorator::DidStartLoading(page_node);
  PageLoadTrackerDecorator::PrimaryPageChanged(page_node);
  ASSERT_TRUE(Data::Exists(page_node));
  Data& page_data = Data::Get(page_node);
  EXPECT_EQ(LIS::kLoading, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoading, page_node->GetLoadingState());

  // Mark the page as not idling.
  frame_node->OnNavigationCommitted(
      GURL(), url::Origin(), /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(IsIdling(page_node));

  // DidStopLoading() should cause a transition to kLoadedNotIdling.
  PageLoadTrackerDecorator::DidStopLoading(page_node);
  EXPECT_EQ(LIS::kLoadedNotIdling, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoadedBusy, page_node->GetLoadingState());

  // The state should transition to loading if DidStartLoading() is invoked
  // again, before the page reaches an idle state.
  PageLoadTrackerDecorator::DidStartLoading(page_node);
  EXPECT_EQ(LIS::kWaitingForNavigation, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoading, page_node->GetLoadingState());

  // Test transitions until the page is loaded and idle.
  PageLoadTrackerDecorator::PrimaryPageChanged(page_node);
  EXPECT_EQ(LIS::kLoading, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoading, page_node->GetLoadingState());

  PageLoadTrackerDecorator::DidStopLoading(page_node);
  EXPECT_EQ(LIS::kLoadedNotIdling, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoadedBusy, page_node->GetLoadingState());

  task_env().FastForwardBy(GetWaitingForIdleTimeout());

  EXPECT_FALSE(Data::Exists(page_node));
  EXPECT_EQ(LS::kLoadedIdle, page_node->GetLoadingState());
}

TEST_F(PageLoadTrackerDecoratorTest, IsIdling) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* frame_node = mock_graph.frame.get();
  auto* page_node = mock_graph.page.get();
  auto* proc_node = mock_graph.process.get();

  // Neither of the idling properties are set, so IsIdling should return false.
  EXPECT_FALSE(IsIdling(page_node));

  // Should still return false after main thread task is low.
  proc_node->SetMainThreadTaskLoadIsLow(true);
  EXPECT_FALSE(IsIdling(page_node));

  // Should return true when network is idle.
  frame_node->SetNetworkAlmostIdle();
  EXPECT_TRUE(IsIdling(page_node));

  // Should toggle with main thread task low.
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(IsIdling(page_node));
  proc_node->SetMainThreadTaskLoadIsLow(true);
  EXPECT_TRUE(IsIdling(page_node));

  // Should return false when network is no longer idle.
  frame_node->OnNavigationCommitted(
      GURL(), url::Origin(), /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  EXPECT_FALSE(IsIdling(page_node));

  // And should stay false if main thread task also goes low again.
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(IsIdling(page_node));
}

}  // namespace performance_manager

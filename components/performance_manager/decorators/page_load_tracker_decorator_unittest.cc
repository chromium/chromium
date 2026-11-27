// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/page_load_tracker_decorator.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager {

// Aliasing these here makes this unittest much more legible.
using Data = PageLoadTrackerDecorator::Data;
using LIS = Data::LoadIdleState;
using LS = PageNode::LoadingState;

class PageLoadTrackerDecoratorTest : public PerformanceManagerTestHarness {
 public:
  PageLoadTrackerDecoratorTest(const PageLoadTrackerDecoratorTest&) = delete;
  PageLoadTrackerDecoratorTest& operator=(const PageLoadTrackerDecoratorTest&) =
      delete;

 protected:
  PageLoadTrackerDecoratorTest()
      : PerformanceManagerTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PageLoadTrackerDecoratorTest() override = default;

  void SetUp() override {
    GetGraphFeatures().EnablePageLoadTrackerDecorator();
    PerformanceManagerTestHarness::SetUp();
    SetContents(CreateTestWebContents());
  }

  void TearDown() override {
    DeleteContents();
    PerformanceManagerTestHarness::TearDown();
  }

  PageNodeImpl* GetPageNode() {
    base::WeakPtr<PageNode> node =
        PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
    CHECK(node);
    return PageNodeImpl::FromNode(node.get());
  }

  void TestPageAlmostIdleTransitions(bool timeout_waiting_for_response,
                                     bool timeout_waiting_for_idle);

  bool IsIdling(const PageNodeImpl* page_node) const {
    return PageLoadTrackerDecorator::IsIdling(page_node);
  }

  static constexpr base::TimeDelta GetWaitingForIdleTimeout() {
    return PageLoadTrackerDecorator::kWaitingForIdleTimeout;
  }
};

void PageLoadTrackerDecoratorTest::TestPageAlmostIdleTransitions(
    bool timeout_waiting_for_response,
    bool timeout_waiting_for_idle) {
  static const base::TimeDelta kLoadedAndIdlingTimeout =
      PageLoadTrackerDecorator::kLoadedAndIdlingTimeout;
  static const base::TimeDelta kWaitingForIdleTimeout =
      PageLoadTrackerDecorator::kWaitingForIdleTimeout;

  auto* page_node = GetPageNode();

  // Initially the page should be in a loading not started state.
  EXPECT_FALSE(Data::Exists(page_node));
  EXPECT_EQ(LS::kLoadingNotStarted, page_node->GetLoadingState());

  // The state should transition to loading when a navigation starts.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/"), web_contents());
  navigation->SetKeepLoading(true);
  navigation->Start();

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
    task_environment()->FastForwardBy(
        PageLoadTrackerDecorator::kWaitingForNavigationTimeout);
    EXPECT_EQ(LIS::kWaitingForNavigationTimedOut, page_data.load_idle_state());
    EXPECT_EQ(LS::kLoadingTimedOut, page_node->GetLoadingState());
    EXPECT_FALSE(page_data.timer_.IsRunning());
  }

  // Commit the navigation while keeping loading. The state should transition
  // to kLoading (no matter whether kWaitingForNavigationTimeout expired since
  // load started).
  navigation->Commit();
  EXPECT_EQ(LIS::kLoading, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoading, page_node->GetLoadingState());
  EXPECT_FALSE(page_data.timer_.IsRunning());

  FrameNodeImpl* frame_node = page_node->primary_main_frame_node();
  ASSERT_TRUE(frame_node);
  ProcessNodeImpl* proc_node = frame_node->process_node();
  ASSERT_TRUE(proc_node);

  // Mark the page as idling. It should transition from
  // kLoading directly to kLoadedAndIdling after StopLoading().
  frame_node->SetNetworkAlmostIdle();
  proc_node->SetMainThreadTaskLoadIsLow(true);
  navigation->StopLoading();
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
    task_environment()->FastForwardBy(kWaitingForIdleTimeout);
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
    task_environment()->FastForwardBy(kLoadedAndIdlingTimeout);
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
  auto* page_node = GetPageNode();

  // Initially the page should be in a loading not started state.
  EXPECT_FALSE(Data::Exists(page_node));
  EXPECT_EQ(LS::kLoadingNotStarted, page_node->GetLoadingState());

  // The state should transition to loading when a navigation commits while
  // still loading.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/"), web_contents());
  navigation->SetKeepLoading(true);
  navigation->Start();
  navigation->Commit();

  ASSERT_TRUE(Data::Exists(page_node));
  Data& page_data = Data::Get(page_node);
  EXPECT_EQ(LIS::kLoading, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoading, page_node->GetLoadingState());
  EXPECT_FALSE(page_data.timer_.IsRunning());

  FrameNodeImpl* frame_node = page_node->primary_main_frame_node();
  ASSERT_TRUE(frame_node);
  ProcessNodeImpl* proc_node = frame_node->process_node();
  ASSERT_TRUE(proc_node);

  // Mark the page as not idling.
  frame_node->OnNavigationCommitted(
      GURL(), url::Origin(), /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(IsIdling(page_node));

  // StopLoading() should cause a transition to kLoadedNotIdling.
  navigation->StopLoading();
  EXPECT_EQ(LIS::kLoadedNotIdling, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoadedBusy, page_node->GetLoadingState());
  EXPECT_TRUE(page_data.timer_.IsRunning());
}

TEST_F(PageLoadTrackerDecoratorTest, TestStartLoadingAgainBeforeIdle) {
  auto* page_node = GetPageNode();

  // Initially the page should be in a loading not started state.
  EXPECT_FALSE(Data::Exists(page_node));
  EXPECT_EQ(LS::kLoadingNotStarted, page_node->GetLoadingState());

  // Start and commit the first navigation.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/"), web_contents());
  navigation->SetKeepLoading(true);
  navigation->Start();
  navigation->Commit();

  ASSERT_TRUE(Data::Exists(page_node));
  Data& page_data = Data::Get(page_node);
  EXPECT_EQ(LIS::kLoading, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoading, page_node->GetLoadingState());

  FrameNodeImpl* frame_node = page_node->primary_main_frame_node();
  ASSERT_TRUE(frame_node);
  ProcessNodeImpl* proc_node = frame_node->process_node();
  ASSERT_TRUE(proc_node);

  // Mark the page as not idling.
  frame_node->OnNavigationCommitted(
      GURL(), url::Origin(), /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(IsIdling(page_node));

  // StopLoading() should cause a transition to kLoadedNotIdling.
  navigation->StopLoading();
  EXPECT_EQ(LIS::kLoadedNotIdling, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoadedBusy, page_node->GetLoadingState());

  // The state should transition to loading if a new navigation starts before
  // the page reaches an idle state.
  auto navigation2 = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example2.com/"), web_contents());
  navigation2->SetKeepLoading(true);
  navigation2->Start();
  EXPECT_EQ(LIS::kWaitingForNavigation, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoading, page_node->GetLoadingState());

  // Test transitions until the page is loaded and idle.
  navigation2->Commit();
  EXPECT_EQ(LIS::kLoading, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoading, page_node->GetLoadingState());

  navigation2->StopLoading();
  EXPECT_EQ(LIS::kLoadedNotIdling, page_data.load_idle_state());
  EXPECT_EQ(LS::kLoadedBusy, page_node->GetLoadingState());

  task_environment()->FastForwardBy(GetWaitingForIdleTimeout());

  EXPECT_FALSE(Data::Exists(page_node));
  EXPECT_EQ(LS::kLoadedIdle, page_node->GetLoadingState());
}

TEST_F(PageLoadTrackerDecoratorTest, IsIdling) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/"));

  auto* page_node = GetPageNode();
  FrameNodeImpl* frame_node = page_node->primary_main_frame_node();
  ASSERT_TRUE(frame_node);
  ProcessNodeImpl* proc_node = frame_node->process_node();
  ASSERT_TRUE(proc_node);

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

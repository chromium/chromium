// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/policies/tab_loading_frame_navigation_policy.h"

#include "base/bind.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace policies {

namespace {

class TabLoadingFrameNavigationPolicyTest
    : public PerformanceManagerTestHarness,
      public TabLoadingFrameNavigationPolicy::MechanismDelegate {
  using Super = PerformanceManagerTestHarness;

 public:
  TabLoadingFrameNavigationPolicyTest()
      : PerformanceManagerTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  TabLoadingFrameNavigationPolicyTest(
      const TabLoadingFrameNavigationPolicyTest&) = delete;
  TabLoadingFrameNavigationPolicyTest& operator=(
      const TabLoadingFrameNavigationPolicyTest&) = delete;
  ~TabLoadingFrameNavigationPolicyTest() override = default;

  void SetUp() override {
    Super::SetUp();

    // When the policy is created we expect it to enable the mechanism.
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    EXPECT_CALL(*this, OnSetThrottlingEnabled(true));
    PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindLambdaForTesting([policy = this->policy_](Graph* graph) {
          // Destroy the policy object early, so that it invokes
          // StopThrottlingEverything in a controlled way.
          graph->TakeFromGraph(policy);
        }));

    // Create the policy object and inject it into the graph.
    std::unique_ptr<TabLoadingFrameNavigationPolicy> policy =
        std::make_unique<TabLoadingFrameNavigationPolicy>();
    policy->SetMechanismDelegateForTesting(this);
    policy_ = policy.get();
    PerformanceManager::PassToGraph(FROM_HERE, std::move(policy));
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(this);

    // Ensure that the policy default initializes with no throttles in place.
    ExpectThrottledPageCount(0);
    start_ = task_environment()->GetMockTickClock()->NowTicks();
  }

  void TearDown() override {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    EXPECT_CALL(*this, OnSetThrottlingEnabled(false));
    PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindLambdaForTesting([policy = this->policy_](Graph* graph) {
          // Destroy the policy object early, so that it invokes
          // StopThrottlingEverything in a controlled way.
          graph->TakeFromGraph(policy);
        }));
    policy_ = nullptr;
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(this);
    Super::TearDown();
  }

  void RunUntilStopThrottling() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    quit_closure_.Reset();
  }

  // Returns the scheduled page timeout after simulating FCP.
  base::TimeTicks SimulateFirstContentfulPaint(content::WebContents* contents,
                                               base::TimeDelta fcp) {
    base::RunLoop run_loop;
    base::TimeTicks timeout;

    auto frame = PerformanceManager::GetFrameNodeForRenderFrameHost(
        contents->GetMainFrame());
    PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindLambdaForTesting(
            [policy = policy(), fcp, frame, &run_loop, &timeout](Graph* graph) {
              EXPECT_TRUE(frame.get());
              policy->OnFirstContentfulPaintForTesting(frame.get(), fcp);
              timeout = policy->GetPageTimeoutForTesting(frame->GetPageNode());
              run_loop.Quit();
            }));

    run_loop.Run();
    return timeout;
  }

  void ExpectThrottledPageCount(size_t expected_throttled_page_count) {
    base::RunLoop run_loop;

    // This will be called back on the UI thread once the throttled page count
    // has been retrieved from the PM and validated.
    auto on_ui_thread = base::BindLambdaForTesting(
        [expected_throttled_page_count, &run_loop](size_t throttled_page_count,
                                                   bool timer_running) {
          EXPECT_EQ(expected_throttled_page_count, throttled_page_count);
          EXPECT_EQ(expected_throttled_page_count > 0, timer_running);
          run_loop.Quit();
        });

    // Call into the graph to get the throttled page count, bounce back to
    // the UI thread, then continue.
    auto* policy = policy_;
    PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindLambdaForTesting([policy, on_ui_thread](Graph* graph_unused) {
          size_t throttled_page_count =
              policy->GetThrottledPageCountForTesting();
          bool timer_running = policy->IsTimerRunningForTesting();

          // Post back to the UI thread with the answer, where it will be
          // validated and the run loop exited.
          content::GetUIThreadTaskRunner({})->PostTask(
              FROM_HERE, base::BindOnce(on_ui_thread, throttled_page_count,
                                        timer_running));
        }));

    // Wait for the response from the graph.
    run_loop.Run();
  }

  // The MechanismDelegate calls redirect here.
  MOCK_METHOD1(OnSetThrottlingEnabled, void(bool));
  MOCK_METHOD2(OnStopThrottling,
               void(content::WebContents*, int64_t last_navigation_id));

  // Accessors.
  TabLoadingFrameNavigationPolicy* policy() const { return policy_; }
  base::TimeTicks start() const { return start_; }

  // The current time, expressed as a multiple of the timeout period.
  double GetRelativeTime() {
    const base::TimeTicks now =
        task_environment()->GetMockTickClock()->NowTicks();
    return (now - start_) / policy_->GetMaxTimeoutForTesting();
  }

 private:
  // MechanismDelegate implementation:
  void SetThrottlingEnabled(bool enabled) override {
    OnSetThrottlingEnabled(enabled);
    // Always expect the quit closure to be set for calls to this.
    quit_closure_.Run();
  }
  void StopThrottling(content::WebContents* contents,
                      int64_t last_navigation_id) override {
    OnStopThrottling(contents, last_navigation_id);

    // Time can be manually advanced as well, so we're not always in a RunLoop.
    // Only try to invoke the quit closure if it has been set.
    if (!quit_closure_.is_null())
      quit_closure_.Run();
  }

  // This is graph owned.
  TabLoadingFrameNavigationPolicy* policy_ = nullptr;
  base::RepeatingClosure quit_closure_;
  base::TimeTicks start_;
};

// Navigates and commits from the browser, returning the navigation id.
int64_t NavigateAndCommitFromBrowser(content::WebContents* contents,
                                     GURL gurl) {
  std::unique_ptr<content::NavigationSimulator> simulator(
      content::NavigationSimulator::CreateBrowserInitiated(gurl, contents));
  simulator->Start();
  int64_t navigation_id = simulator->GetNavigationHandle()->GetNavigationId();
  simulator->Commit();
  auto* pmth = PerformanceManagerTabHelper::FromWebContents(contents);
  CHECK(pmth);
  EXPECT_EQ(pmth->LastNavigationId(), navigation_id);
  EXPECT_EQ(pmth->LastNewDocNavigationId(), navigation_id);
  return navigation_id;
}

// Navigates and commits a same document navigation from the renderer.
// Returns the navigation id.
int64_t NavigateAndCommitSameDocFromRenderer(content::WebContents* contents,
                                             GURL gurl) {
  std::unique_ptr<content::NavigationSimulator> simulator(
      content::NavigationSimulator::CreateRendererInitiated(
          gurl, contents->GetMainFrame()));
  simulator->CommitSameDocument();
  auto* pmth = PerformanceManagerTabHelper::FromWebContents(contents);
  CHECK(pmth);
  EXPECT_NE(pmth->LastNavigationId(), pmth->LastNewDocNavigationId());
  return pmth->LastNavigationId();
}

}  // namespace

TEST_F(TabLoadingFrameNavigationPolicyTest, OnlyHttpContentsThrottled) {
  // Only contents with http(s) URLs should be throttled. Anything like
  // data://, file://, chrome:// or about:// should be ignored.

  // Set the active contents in the RenderViewHostTestHarness.
  SetContents(CreateTestWebContents());

  {
    // Expect https:// contents to be throttled.
    SCOPED_TRACE("https");
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("https://www.foo.com/"));
    EXPECT_TRUE(policy()->ShouldThrottleWebContents(web_contents()));
    ExpectThrottledPageCount(1);
  }

  {
    // Expect http:// contents to be throttled.
    SCOPED_TRACE("http");
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("http://www.bar.com/"));
    EXPECT_TRUE(policy()->ShouldThrottleWebContents(web_contents()));
    ExpectThrottledPageCount(1);
  }

  {
    // Expect chrome:// contents not to be throttled.
    SCOPED_TRACE("chrome");
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("chrome://settings/"));
    EXPECT_FALSE(policy()->ShouldThrottleWebContents(web_contents()));
    ExpectThrottledPageCount(0);
  }

  {
    // Expect file:// contents not to be throttled.
    SCOPED_TRACE("file");
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("file:///home/chrome/foo.txt"));
    EXPECT_FALSE(policy()->ShouldThrottleWebContents(web_contents()));
    ExpectThrottledPageCount(0);
  }

  {
    // Expect about:blank contents not to be throttled.
    SCOPED_TRACE("about");
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("about:blank"));
    EXPECT_FALSE(policy()->ShouldThrottleWebContents(web_contents()));
    ExpectThrottledPageCount(0);
  }

  {
    // Expect data:// contents not to be throttled.
    SCOPED_TRACE("data");
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("data:,Hello%2C%20World!"));
    EXPECT_FALSE(policy()->ShouldThrottleWebContents(web_contents()));
    ExpectThrottledPageCount(0);
  }
}

TEST_F(TabLoadingFrameNavigationPolicyTest,
       OnlyAppropriateChildFramesThrottled) {
  SetContents(CreateTestWebContents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));

  auto* main_frame = web_contents()->GetMainFrame();

  // Create a child to the exact same domain. It should not be throttled.
  auto* child1 =
      content::RenderFrameHostTester::For(main_frame)->AppendChild("child1");
  content::MockNavigationHandle handle1(GURL("https://www.foo.com/foo.html"),
                                        child1);
  EXPECT_FALSE(policy()->ShouldThrottleNavigation(&handle1));

  // Create a child to the the same eTLD+1.
  auto* child2 =
      content::RenderFrameHostTester::For(main_frame)->AppendChild("child2");
  content::MockNavigationHandle handle2(GURL("https://static.foo.com/foo.html"),
                                        child2);
  EXPECT_FALSE(policy()->ShouldThrottleNavigation(&handle2));

  // Create a child to a completely different domain. It should be throttled.
  auto* child3 =
      content::RenderFrameHostTester::For(main_frame)->AppendChild("child3");
  content::MockNavigationHandle handle3(GURL("https://bar.com"), child3);
  EXPECT_TRUE(policy()->ShouldThrottleNavigation(&handle3));

  // Create a child to a non-HTTP(s) protocol. It should not be throttled.
  auto* child4 =
      content::RenderFrameHostTester::For(main_frame)->AppendChild("child4");
  content::MockNavigationHandle handle4(GURL("data:,Hello%2C%20World!"),
                                        child4);
  EXPECT_FALSE(policy()->ShouldThrottleNavigation(&handle4));
}

TEST_F(TabLoadingFrameNavigationPolicyTest, TimeoutWorks) {
  // We express times in this test as fractions of a "timeout" period, which
  // we'll call T. At the beginning of the test, we are at start() == 0.
  ASSERT_EQ(0.0, GetRelativeTime());

  // Navigate and throttle a first contents. It will expire at time T.
  std::unique_ptr<content::WebContents> contents1 = CreateTestWebContents();
  int64_t navigation_id = NavigateAndCommitFromBrowser(
      contents1.get(), GURL("http://www.foo1.com/"));
  EXPECT_TRUE(policy()->ShouldThrottleWebContents(contents1.get()));
  ExpectThrottledPageCount(1);

  // We expect to still be at time 0.
  ASSERT_EQ(0.0, GetRelativeTime());

  // Advance time by half of a timeout. No callbacks should fire.
  task_environment()->FastForwardBy(policy()->GetMaxTimeoutForTesting() * 0.5);
  testing::Mock::VerifyAndClearExpectations(this);

  // We are now at time T / 2.
  ASSERT_EQ(0.5, GetRelativeTime());

  // Navigate and throttle a second contents. It will expire at 1.5 T.
  std::unique_ptr<content::WebContents> contents2 = CreateTestWebContents();
  NavigateAndCommitFromBrowser(contents2.get(), GURL("https://www.foo2.com/"));
  EXPECT_TRUE(policy()->ShouldThrottleWebContents(contents2.get()));
  ExpectThrottledPageCount(2);

  // Run until the first contents times out.
  EXPECT_CALL(*this, OnStopThrottling(contents1.get(), navigation_id));
  RunUntilStopThrottling();
  ExpectThrottledPageCount(1);
  base::TimeTicks stop1 = task_environment()->GetMockTickClock()->NowTicks();
  EXPECT_EQ(policy()->GetMaxTimeoutForTesting(), stop1 - start());

  // We are now at time T.
  ASSERT_EQ(1.0, GetRelativeTime());

  // Navigate and throttle a third contents. It will expire at time 2 T.
  std::unique_ptr<content::WebContents> contents3 = CreateTestWebContents();
  navigation_id = NavigateAndCommitFromBrowser(contents3.get(),
                                               GURL("https://www.foo3.com/"));
  EXPECT_TRUE(policy()->ShouldThrottleWebContents(contents3.get()));
  ExpectThrottledPageCount(2);

  // Advance time by a quarter of the timeout period, bringing us to time
  // 1.25 T. This means there will be 1/4 of the timeout left on the second as
  // it expires at 1.5 T.
  task_environment()->FastForwardBy(policy()->GetMaxTimeoutForTesting() * 0.25);
  testing::Mock::VerifyAndClearExpectations(this);
  ExpectThrottledPageCount(2);

  // We are now at time 1.25 T.
  ASSERT_EQ(1.25, GetRelativeTime());

  // Do a same document navigation. This will cause another navigation commit,
  // but the policy should remain bound to the previous navigation id.
  int64_t same_doc_navigation_id = NavigateAndCommitSameDocFromRenderer(
      contents3.get(), GURL("https://www.foo3.com/#somehash"));
  ASSERT_NE(navigation_id, same_doc_navigation_id);

  // Close the 2nd contents before the timeout expires, and expect the throttled
  // count to drop. Now the timer should be running for the 3rd contents.
  contents2.reset();
  ExpectThrottledPageCount(1);

  // We are still at time 1.25 T.
  ASSERT_EQ(1.25, GetRelativeTime());

  // Advance time to a time past when the second notification *would* have
  // expired, and expect no notifications. We'll go to 1.6 T, so 0.35 T further.
  task_environment()->FastForwardBy(policy()->GetMaxTimeoutForTesting() * 0.35);
  testing::Mock::VerifyAndClearExpectations(this);

  // We are now at time 1.6 T.
  ASSERT_EQ(1.6, GetRelativeTime());

  // Finally, run until the third contents times out.
  EXPECT_CALL(*this, OnStopThrottling(contents3.get(), navigation_id));
  RunUntilStopThrottling();
  ExpectThrottledPageCount(0);
  base::TimeTicks stop3 = task_environment()->GetMockTickClock()->NowTicks();
  EXPECT_EQ(policy()->GetMaxTimeoutForTesting(), stop3 - stop1);

  // We are now at time 2 T.
  ASSERT_EQ(2.0, GetRelativeTime());
}

TEST_F(TabLoadingFrameNavigationPolicyTest, MinTimeoutUpdateWorks) {
  // Navigate and throttle a contents.
  std::unique_ptr<content::WebContents> contents1 = CreateTestWebContents();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      contents1.get(), GURL("http://www.foo1.com/"));
  EXPECT_TRUE(policy()->ShouldThrottleWebContents(contents1.get()));
  ExpectThrottledPageCount(1);

  // Simulate an FCP that would cause a minimum timeout to be applied.
  base::TimeDelta fcp = policy()->GetMinTimeoutForTesting() /
                            policy()->GetFCPMultipleForTesting() -
                        base::TimeDelta::FromMilliseconds(100);
  ASSERT_LT(base::TimeDelta(), fcp);

  // Advance time by that amount and simulate FCP. No callbacks should fire.
  task_environment()->FastForwardBy(fcp);
  base::TimeTicks now = task_environment()->GetMockTickClock()->NowTicks();
  base::TimeTicks timeout = SimulateFirstContentfulPaint(contents1.get(), fcp);
  testing::Mock::VerifyAndClearExpectations(this);

  // The timeout should be set at the minimum timeout.
  EXPECT_EQ(now + policy()->GetMinTimeoutForTesting(), timeout);
}

TEST_F(TabLoadingFrameNavigationPolicyTest, FCPTimeoutUpdateWorks) {
  // Navigate and throttle a contents.
  std::unique_ptr<content::WebContents> contents1 = CreateTestWebContents();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      contents1.get(), GURL("http://www.foo1.com/"));
  EXPECT_TRUE(policy()->ShouldThrottleWebContents(contents1.get()));
  ExpectThrottledPageCount(1);

  // Simulate an FCP that will cause a timeout update.
  base::TimeDelta fcp = policy()->GetMinTimeoutForTesting() +
                        base::TimeDelta::FromMilliseconds(100);

  // Advance time by that amount and simulate FCP. No callbacks should fire.
  task_environment()->FastForwardBy(fcp);
  base::TimeTicks timeout = SimulateFirstContentfulPaint(contents1.get(), fcp);
  testing::Mock::VerifyAndClearExpectations(this);

  // The timeout should be set at the expected calculated timeout (to ms
  // precision).
  EXPECT_EQ((fcp * policy()->GetFCPMultipleForTesting()).InMilliseconds(),
            (timeout - start()).InMilliseconds());
}

TEST_F(TabLoadingFrameNavigationPolicyTest, MaxTimeoutWorks) {
  // Navigate and throttle a contents.
  std::unique_ptr<content::WebContents> contents1 = CreateTestWebContents();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      contents1.get(), GURL("http://www.foo1.com/"));
  EXPECT_TRUE(policy()->ShouldThrottleWebContents(contents1.get()));
  ExpectThrottledPageCount(1);

  // Calculate an FCP that would cause a calculated timeout that exceeds the
  // maximum.
  base::TimeDelta fcp = policy()->GetMaxTimeoutForTesting() /
                            policy()->GetFCPMultipleForTesting() +
                        base::TimeDelta::FromMilliseconds(100);

  // Advance time by that amount and simulate FCP. No callbacks should fire.
  task_environment()->FastForwardBy(fcp);
  base::TimeTicks timeout = SimulateFirstContentfulPaint(contents1.get(), fcp);
  testing::Mock::VerifyAndClearExpectations(this);

  // The timeout should remain at the max timeout it was initially set to.
  EXPECT_EQ(start() + policy()->GetMaxTimeoutForTesting(), timeout);
}

TEST(TabLoadingFrameNavigationThrottlesParams, FeatureParamsWork) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kTabLoadingFrameNavigationThrottles,
      {{"MinimumThrottleTimeoutMilliseconds", "2500"},
       {"MaximumThrottleTimeoutMilliseconds", "25000"},
       {"FCPMultiple", "3.14"}});

  // Make sure the parsing works.
  auto params = features::TabLoadingFrameNavigationThrottlesParams::GetParams();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2500),
            params.minimum_throttle_timeout);
  EXPECT_EQ(base::TimeDelta::FromSeconds(25), params.maximum_throttle_timeout);
  EXPECT_EQ(3.14, params.fcp_multiple);

  // And make sure the plumbing works.
  std::unique_ptr<TabLoadingFrameNavigationPolicy> policy =
      std::make_unique<TabLoadingFrameNavigationPolicy>();
  EXPECT_EQ(params.minimum_throttle_timeout, policy->GetMinTimeoutForTesting());
  EXPECT_EQ(params.maximum_throttle_timeout, policy->GetMaxTimeoutForTesting());
  EXPECT_EQ(params.fcp_multiple, policy->GetFCPMultipleForTesting());

  // Finally make sure that the calculation is as expected.

  // An FCP of 300ms would yield 942ms, or 642ms of additional timeout. This is
  // less than timeout_min_, so we should get that.
  EXPECT_EQ(params.minimum_throttle_timeout,
            policy->CalculateTimeoutFromFCPForTesting(
                base::TimeDelta::FromMilliseconds(300)));

  // An FCP of 1000ms would yield 3140ms, or 2140ms of additional timeout. This
  // is also less than timeout_min_, so we should get that.
  EXPECT_EQ(params.minimum_throttle_timeout,
            policy->CalculateTimeoutFromFCPForTesting(
                base::TimeDelta::FromMilliseconds(1000)));

  // An FCP of 2000ms would yield 6280ms, or 4280ms of additional timeout. We
  // should get that.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(4280),
            policy->CalculateTimeoutFromFCPForTesting(
                base::TimeDelta::FromMilliseconds(2000)));
}

}  // namespace policies
}  // namespace performance_manager

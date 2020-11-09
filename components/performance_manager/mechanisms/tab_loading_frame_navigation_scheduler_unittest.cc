// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/mechanisms/tab_loading_frame_navigation_scheduler.h"

#include <memory>
#include <utility>

#include "base/test/bind_test_util.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager {
namespace mechanisms {

namespace {

// A simply policy delegate that always says yes to throttling everything.
class DummyPolicyDelegate
    : public TabLoadingFrameNavigationScheduler::PolicyDelegate {
 public:
  DummyPolicyDelegate() = default;
  DummyPolicyDelegate(const DummyPolicyDelegate&) = delete;
  DummyPolicyDelegate& operator=(const DummyPolicyDelegate&) = delete;
  ~DummyPolicyDelegate() override = default;

  // PolicyDelegate implementation:
  bool ShouldThrottleWebContents(content::WebContents* contents) override {
    return true;
  }
  bool ShouldThrottleNavigation(content::NavigationHandle* handle) override {
    return true;
  }
};

class TabLoadingFrameNavigationSchedulerTest
    : public PerformanceManagerTestHarness {
 public:
  using Super = PerformanceManagerTestHarness;

  TabLoadingFrameNavigationSchedulerTest() = default;
  TabLoadingFrameNavigationSchedulerTest(
      const TabLoadingFrameNavigationSchedulerTest&) = delete;
  TabLoadingFrameNavigationSchedulerTest& operator=(
      const TabLoadingFrameNavigationSchedulerTest&) = delete;
  ~TabLoadingFrameNavigationSchedulerTest() override = default;

  void SetUp() override {
    Super::SetUp();
    TabLoadingFrameNavigationScheduler::SetPolicyDelegateForTesting(
        &policy_delegate_);
  }

  void TearDown() override {
    // Clear the delegate (a static singleton) so we don't affect other tests.
    TabLoadingFrameNavigationScheduler::SetPolicyDelegateForTesting(nullptr);
    Super::TearDown();
  }

 private:
  DummyPolicyDelegate policy_delegate_;
};

// A simple WebContentsObserver that attaches a throttle to a navigation
// when the navigation starts.
class AttachThrottleHelper : public content::WebContentsObserver {
 public:
  explicit AttachThrottleHelper(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}

  AttachThrottleHelper(const AttachThrottleHelper&) = delete;
  AttachThrottleHelper& operator=(const AttachThrottleHelper&) = delete;

  ~AttachThrottleHelper() override = default;

 private:
  // WebContentsObserver implementation:
  void DidStartNavigation(content::NavigationHandle* handle) override {
    std::unique_ptr<content::NavigationThrottle> throttle =
        TabLoadingFrameNavigationScheduler::MaybeCreateThrottleForNavigation(
            handle);
    if (throttle)
      handle->RegisterThrottleForTesting(std::move(throttle));
  }
};

}  // namespace

TEST_F(TabLoadingFrameNavigationSchedulerTest, MultipleThrottlesClosed) {
  TabLoadingFrameNavigationScheduler::SetThrottlingEnabled(true);

  // A helper class for attaching NavigationThrottles to the simulated
  // navigations.
  SetContents(CreateTestWebContents());
  AttachThrottleHelper attach_throttle_helper(web_contents());

  // Start a main frame navigation.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));
  content::RenderFrameHost* main_frame = web_contents()->GetMainFrame();

  // A scheduler should now exist, but there should be no throttles.
  auto* scheduler =
      TabLoadingFrameNavigationScheduler::FromWebContents(web_contents());
  ASSERT_TRUE(scheduler);
  EXPECT_EQ(0u, scheduler->GetThrottleCountForTesting());

  // Create a child frame that we will navigate.
  auto* child1 =
      content::RenderFrameHostTester::For(main_frame)->AppendChild("child1");

  // Pretend to navigate the child frame with a MockNavigation, and issue a
  // throttle for it. We manually delete the throttle before transferring
  // ownership to a NavigationHandle, testing the throttle deletion notification
  // codepath.
  const GURL bar_url("https://www.bar.com/");
  {
    content::MockNavigationHandle handle(bar_url, child1);
    std::unique_ptr<content::NavigationThrottle> throttle =
        scheduler->MaybeCreateThrottleForNavigation(&handle);
    EXPECT_TRUE(throttle.get());
    EXPECT_EQ(1u, scheduler->GetThrottleCountForTesting());
    throttle.reset();
    EXPECT_EQ(0u, scheduler->GetThrottleCountForTesting());
  }

  // Navigate the child frame again. This time we'll use a navigation simulator
  // and actually attach the throttle to the handle.
  std::unique_ptr<content::NavigationSimulator> simulator1(
      content::NavigationSimulator::CreateRendererInitiated(bar_url, child1));
  simulator1->SetAutoAdvance(false);
  simulator1->Start();
  auto* handle1 = simulator1->GetNavigationHandle();
  EXPECT_EQ(1u, scheduler->GetThrottleCountForTesting());
  EXPECT_TRUE(simulator1->IsDeferred());

  // Create another frame from a distinct domain. It should be throttled.
  auto* child2 =
      content::RenderFrameHostTester::For(main_frame)->AppendChild("child2");
  std::unique_ptr<content::NavigationSimulator> simulator2(
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://www.baz.com/"), child2));
  simulator2->SetAutoAdvance(false);
  simulator2->Start();
  auto* handle2 = simulator2->GetNavigationHandle();
  EXPECT_EQ(2u, scheduler->GetThrottleCountForTesting());
  EXPECT_TRUE(simulator2->IsDeferred());

  // Set a "Resume" callback. When resume is called for one of the throttles,
  // reset the other. Don't expect another resume callback for the second
  // throttle, as it should be synchronously cleaned up in the scope of the
  // first resume.
  size_t resume_callback_count = 0;
  scheduler->SetResumeCallbackForTesting(base::BindLambdaForTesting(
      [&simulator1, &simulator2, handle1, handle2,
       &resume_callback_count](content::NavigationThrottle* throttle) {
        ++resume_callback_count;
        // Calling AbortFromRenderer will cause the throttle to be destroyed via
        // a DidFinishNavigation notification.
        auto* handle = throttle->navigation_handle();
        if (handle == handle1) {
          simulator2->AbortFromRenderer();
        } else if (handle == handle2) {
          simulator1->AbortFromRenderer();
        }
      }));

  // Disable throttling, and expect this to cause the scheduler to tear down.
  // The resume callback should only be invoked for one of the throttles, and
  // the scheduler should be torn down.
  TabLoadingFrameNavigationScheduler::SetThrottlingEnabled(false);
  DCHECK_EQ(1u, resume_callback_count);
  EXPECT_FALSE(
      TabLoadingFrameNavigationScheduler::FromWebContents(web_contents()));
}

}  // namespace mechanisms
}  // namespace performance_manager

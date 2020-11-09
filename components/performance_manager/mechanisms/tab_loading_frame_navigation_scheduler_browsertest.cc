// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/mechanisms/tab_loading_frame_navigation_scheduler.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "components/performance_manager/performance_manager_registry_impl.h"
#include "components/performance_manager/test_support/performance_manager_browsertest_harness.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace mechanisms {

namespace {

class LenientMockPolicyDelegate
    : public TabLoadingFrameNavigationScheduler::PolicyDelegate {
 public:
  LenientMockPolicyDelegate() = default;
  ~LenientMockPolicyDelegate() override = default;

  // PolicyDelegate implementation:
  MOCK_METHOD1(ShouldThrottleWebContents, bool(content::WebContents*));
  MOCK_METHOD1(ShouldThrottleNavigation, bool(content::NavigationHandle*));
};

using MockPolicyDelegate = ::testing::StrictMock<LenientMockPolicyDelegate>;

class TabLoadingFrameNavigationSchedulerTest
    : public PerformanceManagerBrowserTestHarness {
  using Super = PerformanceManagerBrowserTestHarness;

 public:
  TabLoadingFrameNavigationSchedulerTest() = default;
  ~TabLoadingFrameNavigationSchedulerTest() override = default;

  // Used as an embedder hook. Allows us to add navigation throttles to new
  // navigations. This is hooked up to the ShellContentBrowserClient in
  // "CreatedBrowserMainParts".
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(content::NavigationHandle* handle) {
    std::vector<std::unique_ptr<content::NavigationThrottle>> ret;
    // Invoke the class under test here. We control the outcome of this via
    // the MockPolicyDelegate.
    std::unique_ptr<content::NavigationThrottle> throttle =
        TabLoadingFrameNavigationScheduler::MaybeCreateThrottleForNavigation(
            handle);
    if (throttle)
      ret.push_back(std::move(throttle));
    return ret;
  }

  // content::BrowserTestBase overrides:
  void PreRunTestOnMainThread() override {
    Super::PreRunTestOnMainThread();

    TabLoadingFrameNavigationScheduler::SetPolicyDelegateForTesting(
        &mock_policy_delegate_);

    // Enable the mechanism at the beginning of all tests.
    EXPECT_FALSE(
        TabLoadingFrameNavigationScheduler::IsThrottlingEnabledForTesting());
    EXPECT_FALSE(
        TabLoadingFrameNavigationScheduler::IsMechanismRegisteredForTesting());
    TabLoadingFrameNavigationScheduler::SetThrottlingEnabled(true);
    EXPECT_TRUE(
        TabLoadingFrameNavigationScheduler::IsThrottlingEnabledForTesting());
    EXPECT_TRUE(
        TabLoadingFrameNavigationScheduler::IsMechanismRegisteredForTesting());

    // Register a callback so we can set navigation throttles. Passing
    // |this| is fine because we clear the callback before we are torn down.
    content::ShellContentBrowserClient::Get()
        ->set_create_throttles_for_navigation_callback(
            base::BindRepeating(&TabLoadingFrameNavigationSchedulerTest::
                                    CreateThrottlesForNavigation,
                                base::Unretained(this)));
  }
  void PostRunTestOnMainThread() override {
    // Set an empty callback so we stop getting called.
    base::RepeatingCallback<
        std::vector<std::unique_ptr<content::NavigationThrottle>>(
            content::NavigationHandle*)>
        callback;
    content::ShellContentBrowserClient::Get()
        ->set_create_throttles_for_navigation_callback(callback);

    // Disable at the end of all tests. Some tests may already have disabled
    // throttling, so don't first check it is enabled.
    TabLoadingFrameNavigationScheduler::SetThrottlingEnabled(false);
    EXPECT_FALSE(
        TabLoadingFrameNavigationScheduler::IsThrottlingEnabledForTesting());
    EXPECT_FALSE(
        TabLoadingFrameNavigationScheduler::IsMechanismRegisteredForTesting());

    TabLoadingFrameNavigationScheduler::SetPolicyDelegateForTesting(nullptr);

    Super::PostRunTestOnMainThread();
  }

  // Helper function for getting the scheduler instance associated with the
  // given |contents|.
  TabLoadingFrameNavigationScheduler* GetScheduler(
      content::WebContents* contents) {
    return TabLoadingFrameNavigationScheduler::FromWebContents(contents);
  }

 protected:
  MockPolicyDelegate mock_policy_delegate_;
};

}  // namespace

// TODO(crbug.com/1121748): Test is flaky.
IN_PROC_BROWSER_TEST_F(TabLoadingFrameNavigationSchedulerTest,
                       DISABLED_ThrottlingDisabled) {
  GURL url(embedded_test_server()->GetURL("a.com", "/a.html"));
  auto* contents = shell()->web_contents();

  TabLoadingFrameNavigationScheduler::SetThrottlingEnabled(false);
  EXPECT_FALSE(
      TabLoadingFrameNavigationScheduler::IsThrottlingEnabledForTesting());

  // No scheduler should be created if the mechanism is disabled. There will be
  // no calls to the mock policy engine.
  StartNavigation(contents, url);
  WaitForLoad(contents);
  testing::Mock::VerifyAndClearExpectations(this);
}

IN_PROC_BROWSER_TEST_F(TabLoadingFrameNavigationSchedulerTest,
                       SchedulerNotCreated) {
  GURL url(embedded_test_server()->GetURL("a.com", "/a.html"));
  auto* contents = shell()->web_contents();

  // No scheduler should be created if "ShouldThrottleWebContents" returns
  // false.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_policy_delegate_, ShouldThrottleWebContents(contents))
      .WillOnce([&run_loop](content::WebContents*) -> bool {
        run_loop.Quit();
        return false;
      });
  StartNavigation(contents, url);
  run_loop.Run();
  auto* scheduler = GetScheduler(contents);
  EXPECT_EQ(nullptr, scheduler);

  // Wait for the load to finish so that it's not ongoing while the test
  // fixture tears down.
  WaitForLoad(contents);
}

IN_PROC_BROWSER_TEST_F(TabLoadingFrameNavigationSchedulerTest,
                       SchedulerCreatedAndDestroyed) {
  GURL url1(embedded_test_server()->GetURL("a.com", "/a.html"));
  GURL url2(embedded_test_server()->GetURL("b.com", "/b.html"));
  auto* contents1 = shell()->web_contents();
  auto* shell2 = CreateShell();
  auto* contents2 = shell2->web_contents();

  // A scheduler *should* be created if "ShouldThrottleWebContents" returns
  // true.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_policy_delegate_, ShouldThrottleWebContents(contents1))
        .WillOnce([&run_loop](content::WebContents*) -> bool {
          run_loop.Quit();
          return true;
        });
    StartNavigation(contents1, url1);
    run_loop.Run();
    auto* scheduler = GetScheduler(contents1);
    EXPECT_NE(nullptr, scheduler);
    EXPECT_EQ(0u, scheduler->GetThrottleCountForTesting());
  }

  // Start another navigation and expect another scheduler to be created.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_policy_delegate_, ShouldThrottleWebContents(contents2))
        .WillOnce([&run_loop](content::WebContents*) -> bool {
          run_loop.Quit();
          return true;
        });
    StartNavigation(contents2, url2);
    run_loop.Run();
    auto* scheduler = GetScheduler(contents2);
    EXPECT_NE(nullptr, scheduler);
    EXPECT_EQ(0u, scheduler->GetThrottleCountForTesting());
  }

  // Disable throttling and expect all schedulers to be torn down.
  TabLoadingFrameNavigationScheduler::SetThrottlingEnabled(false);
  EXPECT_FALSE(
      TabLoadingFrameNavigationScheduler::IsThrottlingEnabledForTesting());
  EXPECT_EQ(nullptr, GetScheduler(contents1));
  EXPECT_EQ(nullptr, GetScheduler(contents2));

  // Wait for the load to finish so that it's not ongoing while the test
  // fixture tears down.
  WaitForLoad(contents1);
  WaitForLoad(contents2);
}

// TODO(crbug.com/1121748): Test is flaky.
IN_PROC_BROWSER_TEST_F(TabLoadingFrameNavigationSchedulerTest,
                       DISABLED_ChildFrameThrottled) {
  GURL url(embedded_test_server()->GetURL("a.com", "/a_embeds_b.html"));
  auto* contents = shell()->web_contents();

  // Throttle the navigation, and throttle the child frame.
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  EXPECT_CALL(mock_policy_delegate_, ShouldThrottleWebContents(contents))
      .WillOnce([&run_loop1](content::WebContents*) -> bool {
        run_loop1.Quit();
        return true;
      });
  EXPECT_CALL(mock_policy_delegate_, ShouldThrottleNavigation(testing::_))
      .WillOnce([&run_loop2](content::NavigationHandle*) -> bool {
        run_loop2.Quit();
        return true;
      });

  // Start the navigation, and expect scheduler to have been created.
  StartNavigation(contents, url);
  run_loop1.Run();
  auto* scheduler = GetScheduler(contents);
  EXPECT_NE(nullptr, scheduler);
  EXPECT_EQ(0u, scheduler->GetThrottleCountForTesting());

  // Wait for the child navigation to have started. We'll know once
  // the policy function has been invoked, which will quit the runloop.
  run_loop2.Run();

  // At this point the child frame navigation should be throttled, waiting for
  // the policy object to notify that the throttles should be removed.
  EXPECT_EQ(1u, scheduler->GetThrottleCountForTesting());

  // Release the throttles. This also causes the scheduler to be deleted, which
  // we confirm.
  scheduler->StopThrottlingForTesting();
  scheduler = GetScheduler(contents);
  EXPECT_EQ(nullptr, scheduler);

  // Wait for the load to finish so that it's not ongoing while the test
  // fixture tears down.
  WaitForLoad(contents);
}

IN_PROC_BROWSER_TEST_F(TabLoadingFrameNavigationSchedulerTest,
                       NavigationCancelsThrottling) {
  GURL url(embedded_test_server()->GetURL("a.com", "/a_embeds_b.html"));
  auto* contents = shell()->web_contents();

  // Throttle the navigation, and throttle the child frame.
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  EXPECT_CALL(mock_policy_delegate_, ShouldThrottleWebContents(contents))
      .WillOnce([&run_loop1](content::WebContents*) -> bool {
        run_loop1.Quit();
        return true;
      });
  EXPECT_CALL(mock_policy_delegate_, ShouldThrottleNavigation(testing::_))
      .WillOnce([&run_loop2](content::NavigationHandle*) -> bool {
        run_loop2.Quit();
        return true;
      });

  // Start the navigation, and expect scheduler to have been created.
  StartNavigation(contents, url);
  run_loop1.Run();
  auto* scheduler = GetScheduler(contents);
  EXPECT_NE(nullptr, scheduler);
  EXPECT_EQ(0u, scheduler->GetThrottleCountForTesting());
  int64_t original_navigation_id = scheduler->GetNavigationIdForTesting();

  // Wait for the child navigation to have started. We'll know once
  // the policy function has been invoked, which will quit the runloop.
  run_loop2.Run();

  // At this point the child frame navigation should be throttled, waiting for
  // the policy object to notify that the throttles should be removed.
  EXPECT_EQ(1u, scheduler->GetThrottleCountForTesting());

  // Reuse the contents for another navigation. This should result in another
  // call to ShouldThrottleWebContents (which returns false), and the
  // scheduler should be deleted.
  url = embedded_test_server()->GetURL("b.com", "/b.html");
  base::RunLoop run_loop3;
  EXPECT_CALL(mock_policy_delegate_, ShouldThrottleWebContents(contents))
      .WillOnce([&run_loop3](content::WebContents* contents) -> bool {
        run_loop3.Quit();
        return false;
      });
  StartNavigation(contents, url);
  run_loop3.Run();
  scheduler = GetScheduler(contents);
  EXPECT_EQ(nullptr, scheduler);

  // Simulate a delayed arrival of a policy message for the previous navigation
  // and expect it to do nothing.
  TabLoadingFrameNavigationScheduler::StopThrottling(contents,
                                                     original_navigation_id);
  scheduler = GetScheduler(contents);
  EXPECT_EQ(nullptr, scheduler);

  // Wait for the load to finish so that it's not ongoing while the test
  // fixture tears down.
  WaitForLoad(contents);
}

// TODO(crbug.com/1121748): Test is flaky.
IN_PROC_BROWSER_TEST_F(TabLoadingFrameNavigationSchedulerTest,
                       DISABLED_NavigationInterruptsThrottling) {
  GURL url(embedded_test_server()->GetURL("a.com", "/a_embeds_b.html"));
  auto* contents = shell()->web_contents();

  // Throttle the navigation, and throttle the child frame.
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  EXPECT_CALL(mock_policy_delegate_, ShouldThrottleWebContents(contents))
      .WillOnce([&run_loop1](content::WebContents*) -> bool {
        run_loop1.Quit();
        return true;
      });
  EXPECT_CALL(mock_policy_delegate_, ShouldThrottleNavigation(testing::_))
      .WillOnce([&run_loop2](content::NavigationHandle*) -> bool {
        run_loop2.Quit();
        return true;
      });

  // Start the navigation, and expect scheduler to have been created.
  StartNavigation(contents, url);
  run_loop1.Run();
  auto* scheduler = GetScheduler(contents);
  EXPECT_NE(nullptr, scheduler);
  EXPECT_EQ(0u, scheduler->GetThrottleCountForTesting());
  int64_t original_navigation_id = scheduler->GetNavigationIdForTesting();

  // Wait for the child navigation to have started. We'll know once
  // the policy function has been invoked, which will quit the runloop.
  run_loop2.Run();

  // At this point the child frame navigation should be throttled, waiting for
  // the policy object to notify that the throttles should be removed.
  EXPECT_EQ(1u, scheduler->GetThrottleCountForTesting());

  // Reuse the contents for another navigation. This should result in another
  // call to ShouldThrottleWebContents, and the scheduler should be recreated
  // with no throttles.
  url = embedded_test_server()->GetURL("b.com", "/b.html");
  base::RunLoop run_loop3;
  EXPECT_CALL(mock_policy_delegate_, ShouldThrottleWebContents(contents))
      .WillOnce([&run_loop3](content::WebContents* contents) -> bool {
        run_loop3.Quit();
        return true;
      });
  StartNavigation(contents, url);
  run_loop3.Run();
  scheduler = GetScheduler(contents);
  EXPECT_NE(nullptr, scheduler);
  EXPECT_EQ(0u, scheduler->GetThrottleCountForTesting());

  // Simulate a delayed arrival of a policy message for the previous navigation
  // and expect it to do nothing.
  TabLoadingFrameNavigationScheduler::StopThrottling(contents,
                                                     original_navigation_id);
  auto* scheduler2 = GetScheduler(contents);
  EXPECT_EQ(scheduler, scheduler2);
  EXPECT_EQ(0u, scheduler2->GetThrottleCountForTesting());

  // Wait for the load to finish so that it's not ongoing while the test
  // fixture tears down.
  WaitForLoad(contents);
}

}  // namespace mechanisms
}  // namespace performance_manager

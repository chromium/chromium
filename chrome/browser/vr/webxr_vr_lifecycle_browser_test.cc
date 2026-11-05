// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test_utils.h"

namespace vr {

// Tests that closing a tab (which deletes the RenderFrameHost) while an active
// immersive session is running correctly cleans up the XR session state on the
// RenderProcessHost, preventing state leaks.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTestBase,
                       TestFrameDeletionCleansUpSession) {
  MockXRDeviceHookBase mock;

  // Load the page in Tab 1 (keeps the process alive).
  LoadFileAndAwaitInitialization("generic_webxr_page");
  content::WebContents* tab_1_contents = GetCurrentWebContents();
  content::RenderProcessHost* render_process_host =
      tab_1_contents->GetPrimaryMainFrame()->GetProcess();

  // Verify initial state: no session.
  EXPECT_FALSE(render_process_host->HasImmersiveXrSessionForTesting());

  // Open Tab 2 to a blank page from Tab 1 to ensure they share the same
  // process. We use window.open to guarantee SiteInstance (and process)
  // sharing.
  RunJavaScriptOrFail("window.open('about:blank', '_blank');");
  // Wait for the new tab to become active. The browser may create other web
  // contents first, e.g. for its UI.
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return GetCurrentWebContents() != tab_1_contents; }));
  content::WebContents* tab_2_contents = GetCurrentWebContents();

  // Verify they indeed share the process.
  ASSERT_EQ(render_process_host,
            tab_2_contents->GetPrimaryMainFrame()->GetProcess());

  // Load the test page in Tab 2 and wait for it to be fully initialized, then
  // start an immersive session.
  LoadFileAndAwaitInitialization("generic_webxr_page");
  EnterSessionWithUserGestureOrFail(tab_2_contents);

  // Verify the process is marked as having an active XR session.
  EXPECT_TRUE(render_process_host->HasImmersiveXrSessionForTesting());

  // Close Tab 2 to trigger RenderFrameDeleted.
  content::WebContentsDestroyedWatcher destroyed_watcher(tab_2_contents);
  CloseTab(tab_2_contents);
  destroyed_watcher.Wait();

  // Verify the process is no longer marked as having an active XR session.
  EXPECT_FALSE(render_process_host->HasImmersiveXrSessionForTesting());
}

// Tests that the XR session state on the RenderProcessHost is correctly
// updated during a normal session start and stop lifecycle.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTestBase,
                       TestSessionLifecycleUpdatesProcessState) {
  MockXRDeviceHookBase mock;

  // Load the test page.
  LoadFileAndAwaitInitialization("generic_webxr_page");
  content::WebContents* web_contents = GetCurrentWebContents();
  content::RenderProcessHost* render_process_host =
      web_contents->GetPrimaryMainFrame()->GetProcess();

  // Verify initial state: no session.
  EXPECT_FALSE(render_process_host->HasImmersiveXrSessionForTesting());

  // Start an immersive session, and verify that the process now tracks having
  // one.
  EnterSessionWithUserGestureOrFail();
  EXPECT_TRUE(render_process_host->HasImmersiveXrSessionForTesting());

  // End the session normally and verify that the process no lnger has an active
  // XR session.
  EndSessionOrFail();

  // We use RunUntil because the browser-side state update might happen
  // asynchronously after the renderer-side session has ended.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !render_process_host->HasImmersiveXrSessionForTesting();
  }));
}

// Tests that a page can request an immersive session while the runtime is
// still shutting down another page's session. The request has to wait until
// the shutdown finishes: OpenXrDevice::RequestSession() CHECKs that it has no
// session. Only Android shuts a session down asynchronously, while the XR
// activity finishes, so only there can a request reach the runtime meanwhile.
#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTestBase,
                       TestRequestWhilePreviousSessionShutsDown) {
  MockXRDeviceHookBase mock;

  // Tab 1 requests a session later. Its XR service keeps the browser's XR
  // state alive when tab 2 closes, so its request goes to the runtime that is
  // shutting down tab 2's session.
  LoadFileAndAwaitInitialization("generic_webxr_page");
  content::WebContents* tab_1_contents = GetCurrentWebContents();

  content::WebContentsAddedObserver new_contents_observer;
  RunJavaScriptOrFail("window.open('about:blank', '_blank');");
  content::WebContents* tab_2_contents = new_contents_observer.GetWebContents();
  ASSERT_EQ(tab_2_contents, GetCurrentWebContents());
  LoadFileAndAwaitInitialization("generic_webxr_page");
  EnterSessionWithUserGestureOrFail(tab_2_contents);

  // Closing tab 2 ends its session, and the runtime starts shutting it down.
  mock.HoldSessionShutdown();
  content::WebContentsDestroyedWatcher destroyed_watcher(tab_2_contents);
  CloseTab(tab_2_contents);
  destroyed_watcher.Wait();
  ASSERT_EQ(tab_1_contents, GetCurrentWebContents());
  PollJavaScriptBooleanOrFail("document.visibilityState == 'visible'",
                              kPollTimeoutLong, tab_1_contents);

  // Request a session from tab 1 while the shutdown is held, and give the
  // request time to get through the renderer and the browser. If the browser
  // passed it on now, the runtime would get it before it finishes shutting
  // down.
  EnterSessionWithUserGesture(tab_1_contents);
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(1));
  run_loop.Run();

  mock.ReleaseSessionShutdown();
  PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].currentSession != null",
      kPollTimeoutLong, tab_1_contents);
  EndSessionOrFail(tab_1_contents);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace vr

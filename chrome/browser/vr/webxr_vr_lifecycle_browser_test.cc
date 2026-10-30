// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
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

}  // namespace vr

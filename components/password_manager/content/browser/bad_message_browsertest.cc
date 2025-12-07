// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/bad_message.h"

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace password_manager::bad_message {

class BadMessageBrowserTest : public content::ContentBrowserTest {
 public:
  BadMessageBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&BadMessageBrowserTest::GetWebContents,
                                base::Unretained(this))) {}

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  content::WebContents* GetWebContents() { return shell()->web_contents(); }

  content::RenderFrameHost* SetupTestWithNavigation() {
    GURL initial_url = embedded_test_server()->GetURL("/title1.html");
    EXPECT_TRUE(NavigateToURL(shell()->web_contents(), initial_url));

    content::RenderFrameHost* active_frame =
        shell()->web_contents()->GetPrimaryMainFrame();
    EXPECT_TRUE(active_frame);

    return active_frame;
  }

  void ExpectNoHistogramEntries(const base::HistogramTester& histogram_tester) {
    histogram_tester.ExpectTotalCount(
        "Stability.BadMessageTerminated.PasswordManager", 0);
  }

  void ExpectHistogramPrerendering(
      const base::HistogramTester& histogram_tester) {
    histogram_tester.ExpectUniqueSample(
        "Stability.BadMessageTerminated.PasswordManager",
        static_cast<int>(BadMessageReason::CPMD_BAD_ORIGIN_PRERENDERING), 1);
  }

  content::test::PrerenderTestHelper prerender_helper_;
};

// Tests that CheckFrameNotPrerendering() correctly handles active frames.
IN_PROC_BROWSER_TEST_F(BadMessageBrowserTest,
                       CheckFrameNotPrerendering_Integration) {
  base::HistogramTester histogram_tester;
  content::RenderFrameHost* active_frame = SetupTestWithNavigation();

  ASSERT_EQ(active_frame->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);

  bool result = CheckFrameNotPrerendering(active_frame);

  EXPECT_TRUE(result);
  ExpectNoHistogramEntries(histogram_tester);
}

// Tests that CheckFrameNotPrerendering() correctly handles prerendering frames.
IN_PROC_BROWSER_TEST_F(BadMessageBrowserTest,
                       CheckFrameNotPrerendering_Prerendering) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  base::HistogramTester histogram_tester;

  GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), initial_url));

  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  auto host_id = prerender_helper_.AddPrerender(prerender_url);
  content::RenderFrameHost* prerender_frame =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);
  ASSERT_TRUE(prerender_frame);
  ASSERT_EQ(prerender_frame->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kPrerendering);

  content::RenderProcessHostWatcher crash_observer(
      prerender_frame->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  bool result = CheckFrameNotPrerendering(prerender_frame);
  EXPECT_FALSE(result);

  crash_observer.Wait();

  ExpectHistogramPrerendering(histogram_tester);
}

// Tests that CheckChildProcessSecurityPolicyForURL() works with real
// WebContents in a browser test environment to verify integration.
IN_PROC_BROWSER_TEST_F(BadMessageBrowserTest,
                       CheckChildProcessSecurityPolicyForURL_Integration) {
  base::HistogramTester histogram_tester;
  content::RenderFrameHost* active_frame = SetupTestWithNavigation();

  GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  bool result = CheckChildProcessSecurityPolicyForURL(
      active_frame, initial_url,
      BadMessageReason::CPMD_BAD_ORIGIN_FORM_SUBMITTED);

  EXPECT_TRUE(result);
  ExpectNoHistogramEntries(histogram_tester);
}

}  // namespace password_manager::bad_message

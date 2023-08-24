// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_readiness_tracker.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::InSequence;

class ThumbnailReadinessTrackerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    readiness_tracker_ = std::make_unique<ThumbnailReadinessTracker>(
        web_contents(), readiness_callback_.Get());
  }

  void TearDownOnMainThread() override { readiness_tracker_.reset(); }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  base::MockCallback<ThumbnailReadinessTracker::ReadinessChangeCallback>
      readiness_callback_;
  std::unique_ptr<ThumbnailReadinessTracker> readiness_tracker_;
};

IN_PROC_BROWSER_TEST_F(ThumbnailReadinessTrackerBrowserTest,
                       NavigateAfterOnload) {
  const GURL url = embedded_test_server()->GetURL(
      "/thumbnail_capture/navigate_after_onload.html");
  const GURL redirect_url =
      embedded_test_server()->GetURL("/thumbnail_capture/redirect_target.html");

  {
    InSequence _s;

    // The first page fully loads, completing its onload handler.
    EXPECT_CALL(
        readiness_callback_,
        Run(ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture));
    EXPECT_CALL(
        readiness_callback_,
        Run(ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture));

    // After onload completes, it redirects.
    EXPECT_CALL(readiness_callback_,
                Run(ThumbnailReadinessTracker::Readiness::kNotReady));
    EXPECT_CALL(
        readiness_callback_,
        Run(ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture));
    EXPECT_CALL(
        readiness_callback_,
        Run(ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture));
  }

  content::NavigateToURLBlockUntilNavigationsComplete(web_contents(), url, 2);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), redirect_url);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
}

IN_PROC_BROWSER_TEST_F(ThumbnailReadinessTrackerBrowserTest,
                       NavigateIframeAfterOnload) {
  const GURL url = embedded_test_server()->GetURL(
      "/thumbnail_capture/navigate_iframe_after_onload.html");
  const GURL iframe_url =
      embedded_test_server()->GetURL("/thumbnail_capture/iframe_2.html");

  {
    InSequence _s;

    EXPECT_CALL(
        readiness_callback_,
        Run(ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture));
    EXPECT_CALL(
        readiness_callback_,
        Run(ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture));
  }

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  content::NavigateIframeToURL(web_contents(), "if", iframe_url);
}

// Regression test for crbug.com/1120940.
//
// Artifical navigations like history.pushState() shouldn't invalidate
// our thumbnail.
IN_PROC_BROWSER_TEST_F(ThumbnailReadinessTrackerBrowserTest,
                       PushStateAfterOnload) {
  const GURL url = embedded_test_server()->GetURL(
      "/thumbnail_capture/push_state_after_onload.html");

  {
    InSequence _s;

    // The page should become ready for capture. The pushState() call
    // shouldn't change anything.
    EXPECT_CALL(
        readiness_callback_,
        Run(ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture));
    EXPECT_CALL(
        readiness_callback_,
        Run(ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture));
  }

  content::NavigateToURLBlockUntilNavigationsComplete(web_contents(), url, 2);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
}

class ThumbnailReadinessTrackerPrerenderTest
    : public ThumbnailReadinessTrackerBrowserTest {
 public:
  ThumbnailReadinessTrackerPrerenderTest()
      : prerender_helper_(base::BindRepeating(
            &ThumbnailReadinessTrackerBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~ThumbnailReadinessTrackerPrerenderTest() override = default;

  void SetUpOnMainThread() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    ThumbnailReadinessTrackerBrowserTest::SetUpOnMainThread();
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(ThumbnailReadinessTrackerPrerenderTest,
                       PrerenderAfterOnload) {
  const GURL url = embedded_test_server()->GetURL("/simple.html");
  const GURL prerender_url = embedded_test_server()->GetURL("/title1.html");

  {
    InSequence _s;

    // The first page fully loads, completing its onload handler.
    EXPECT_CALL(
        readiness_callback_,
        Run(ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture));
    EXPECT_CALL(
        readiness_callback_,
        Run(ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture));

    // The prerendered page should become ready for capture after activation.
    EXPECT_CALL(readiness_callback_,
                Run(ThumbnailReadinessTracker::Readiness::kNotReady));
    EXPECT_CALL(
        readiness_callback_,
        Run(ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture));
    EXPECT_CALL(
        readiness_callback_,
        Run(ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture));
  }

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  prerender_helper_.AddPrerender(prerender_url);
  prerender_helper_.NavigatePrimaryPage(url);
}

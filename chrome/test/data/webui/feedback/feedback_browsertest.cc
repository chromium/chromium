// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class WebContentsObserver : public content::WebContentsObserver {
 public:
  explicit WebContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override {
    ASSERT_TRUE(ExecJs(web_contents(),
                       "Object.assign(window, {whenTestSetupDoneResolver: "
                       "Promise.withResolvers()})"));
  }
};

class FeedbackBrowserTest : public WebUIMochaBrowserTest {
 protected:
  FeedbackBrowserTest() { set_test_loader_host(chrome::kChromeUIFeedbackHost); }

  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();
    // Register a WebContentsObserver to inject some code that allows the tests
    // to
    // perform setup steps before the prod code runs.
    // TODO(dpapad): Remove this if/when this page is migrated to use Web
    // Components.
    content::WebContents* web_contents =
        chrome_test_utils::GetActiveWebContents(this);
    injection_observer_ = std::make_unique<WebContentsObserver>(web_contents);
  }

 private:
  std::unique_ptr<WebContentsObserver> injection_observer_;
};

IN_PROC_BROWSER_TEST_F(FeedbackBrowserTest, Feedback) {
  RunTestWithoutTestLoader("feedback/feedback_test.js",
                           "runMochaSuite('FeedbackTest')");
}

IN_PROC_BROWSER_TEST_F(FeedbackBrowserTest, AIFeedback) {
  RunTestWithoutTestLoader("feedback/feedback_test.js",
                           "runMochaSuite('AIFeedbackTest')");
}

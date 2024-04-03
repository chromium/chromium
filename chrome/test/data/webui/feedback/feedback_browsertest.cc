// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class FeedbackBrowserTest : public WebUIMochaBrowserTest {
 protected:
  FeedbackBrowserTest() { set_test_loader_host(chrome::kChromeUIFeedbackHost); }
};

IN_PROC_BROWSER_TEST_F(FeedbackBrowserTest, Feedback) {
  RunTest("feedback/feedback_test.js", "runMochaSuite('FeedbackTest')");
}

IN_PROC_BROWSER_TEST_F(FeedbackBrowserTest, AIFeedback) {
  RunTest("feedback/feedback_test.js", "runMochaSuite('AIFeedbackTest')");
}

IN_PROC_BROWSER_TEST_F(FeedbackBrowserTest, AutofillMetadata) {
  RunTest("feedback/autofill_metadata_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(FeedbackBrowserTest, SeaPenFeedback) {
  RunTest("feedback/feedback_test.js", "runMochaSuite('SeaPenFeedbackTest')");
}

IN_PROC_BROWSER_TEST_F(FeedbackBrowserTest, SystemInfo) {
  RunTest("feedback/system_info_test.js", "mocha.run()");
}

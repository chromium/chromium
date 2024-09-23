// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/url_constants.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

/**
 * @fileoverview Test suite for chrome://os-feedback.
 */
namespace ash {

namespace {

class OSFeedbackBrowserTest : public WebUIMochaBrowserTest {
 public:
  OSFeedbackBrowserTest() {
    set_test_loader_host(::ash::kChromeUIOSFeedbackHost);
  }

 protected:
  void RunTestAtPath(const std::string& testFilePath) {
    auto testPath =
        base::StringPrintf("chromeos/os_feedback_ui/%s", testFilePath.c_str());
    WebUIMochaBrowserTest::RunTest(testPath, "mocha.run()");
  }
};

// TODO(b/40884343): Flaky.
IN_PROC_BROWSER_TEST_F(OSFeedbackBrowserTest, DISABLED_ConfirmationPage) {
  RunTestAtPath("confirmation_page_test.js");
}

// TODO(b/40884343): Flaky.
IN_PROC_BROWSER_TEST_F(OSFeedbackBrowserTest, DISABLED_FeedbackFlow) {
  RunTestAtPath("feedback_flow_test.js");
}

// TODO(b/40884343): Flaky.
IN_PROC_BROWSER_TEST_F(OSFeedbackBrowserTest, DISABLED_FileAttachment) {
  RunTestAtPath("file_attachment_test.js");
}

IN_PROC_BROWSER_TEST_F(OSFeedbackBrowserTest, HelpContent) {
  RunTestAtPath("help_content_test.js");
}

// TODO(b/40884343): Flaky.
IN_PROC_BROWSER_TEST_F(OSFeedbackBrowserTest, DISABLED_SearchPage) {
  RunTestAtPath("search_page_test.js");
}

// TODO(b/40884343): Flaky.
IN_PROC_BROWSER_TEST_F(OSFeedbackBrowserTest, DISABLED_ShareDataPage) {
  RunTestAtPath("share_data_page_test.js");
}

}  // namespace

}  // namespace ash

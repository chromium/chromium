// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class CastFeedbackUITest : public WebUIMochaBrowserTest {
 protected:
  CastFeedbackUITest() {
    set_test_loader_host(chrome::kChromeUICastFeedbackHost);
  }
};

IN_PROC_BROWSER_TEST_F(CastFeedbackUITest, Success) {
  RunTest("media_router/cast_feedback_ui_test.js",
          "runMochaTest('Suite', 'Success')");
}

IN_PROC_BROWSER_TEST_F(CastFeedbackUITest, Failure) {
  RunTest("media_router/cast_feedback_ui_test.js",
          "runMochaTest('Suite', 'Failure')");
}

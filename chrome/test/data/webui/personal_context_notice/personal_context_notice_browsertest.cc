// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class PersonalContextNoticeTest : public WebUIMochaBrowserTest {
 protected:
  PersonalContextNoticeTest() {
    set_test_loader_host("personal-context-notice");
  }
};

IN_PROC_BROWSER_TEST_F(PersonalContextNoticeTest, App) {
  RunTest("personal_context_notice/personal_context_notice_test.js",
          "mocha.run()");
}

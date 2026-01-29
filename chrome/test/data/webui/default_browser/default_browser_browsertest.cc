// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/default_browser_modal_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class DefaultBrowserTest : public WebUIMochaBrowserTest {
 protected:
  DefaultBrowserTest() {
    set_test_loader_host(chrome::kChromeUIDefaultBrowserModalHost);
  }
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserTest, AppWithIllustration) {
  RunTest("default_browser/app_with_illustration_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserTest, AppWithoutIllustration) {
  RunTest("default_browser/app_without_illustration_test.js", "mocha.run()");
}

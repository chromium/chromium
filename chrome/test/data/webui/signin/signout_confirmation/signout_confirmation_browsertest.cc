// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class SignoutConfirmationBrowserTest : public WebUIMochaBrowserTest {
 public:
  SignoutConfirmationBrowserTest() {
    set_test_loader_host(chrome::kChromeUISignoutConfirmationHost);
  }
};

IN_PROC_BROWSER_TEST_F(SignoutConfirmationBrowserTest, MainView) {
  RunTest("signin/signout_confirmation/signout_confirmation_view_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SignoutConfirmationBrowserTest, ExtensionsSection) {
  RunTest("signin/signout_confirmation/extensions_section_test.js",
          "mocha.run()");
}

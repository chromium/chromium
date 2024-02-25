// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/test/browser_test.h"

class InlineLoginBrowserTest : public WebUIMochaBrowserTest {
 protected:
  InlineLoginBrowserTest() {
    set_test_loader_host(chrome::kChromeUIChromeSigninHost);
  }

  void RunTestCase(const std::string& testCase) {
    WebUIMochaBrowserTest::RunTest(
        base::StringPrintf(
            "inline_login/inline_login_test.js&reason=%d",
            static_cast<int>(
                signin_metrics::Reason::kForcedSigninPrimaryAccount)),
        base::StringPrintf("runMochaTest('InlineLoginTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(InlineLoginBrowserTest, Initialize) {
  RunTestCase("Initialize");
}

IN_PROC_BROWSER_TEST_F(InlineLoginBrowserTest, WebUICallbacks) {
  RunTestCase("WebUICallbacks");
}

IN_PROC_BROWSER_TEST_F(InlineLoginBrowserTest, AuthenticatorCallbacks) {
  RunTestCase("AuthenticatorCallbacks");
}

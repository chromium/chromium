// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
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
            "chromeos/inline_login/inline_login_test.js&reason=%d",
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

IN_PROC_BROWSER_TEST_F(InlineLoginBrowserTest, BackButton) {
  RunTestCase("BackButton");
}

IN_PROC_BROWSER_TEST_F(InlineLoginBrowserTest, OkButton) {
  RunTestCase("OkButton");
}

class InlineLoginWelcomePageBrowserTest : public WebUIMochaBrowserTest {
 protected:
  InlineLoginWelcomePageBrowserTest() {
    set_test_loader_host(chrome::kChromeUIChromeSigninHost);
  }

  void RunTestCase(const std::string& testCase) {
    WebUIMochaBrowserTest::RunTest(
        base::StringPrintf(
            "chromeos/inline_login/inline_login_welcome_page_test.js&reason=%d",
            static_cast<int>(
                signin_metrics::Reason::kForcedSigninPrimaryAccount)),
        base::StringPrintf("runMochaTest('InlineLoginWelcomePageTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(InlineLoginWelcomePageBrowserTest, Reauthentication) {
  RunTestCase("Reauthentication");
}

IN_PROC_BROWSER_TEST_F(InlineLoginWelcomePageBrowserTest, OkButton) {
  RunTestCase("OkButton");
}

IN_PROC_BROWSER_TEST_F(InlineLoginWelcomePageBrowserTest, Checkbox) {
  RunTestCase("Checkbox");
}

IN_PROC_BROWSER_TEST_F(InlineLoginWelcomePageBrowserTest, GoBack) {
  RunTestCase("GoBack");
}

class InlineLoginSigninBlockedByPolicyPageBrowserTest
    : public WebUIMochaBrowserTest {
 protected:
  InlineLoginSigninBlockedByPolicyPageBrowserTest() {
    set_test_loader_host(chrome::kChromeUIChromeSigninHost);
  }

  void RunTestCase(const std::string& testCase) {
    WebUIMochaBrowserTest::RunTest(
        base::StringPrintf(
            "chromeos/inline_login/"
            "inline_login_signin_blocked_by_policy_page_test.js&reason=%d",
            static_cast<int>(signin_metrics::Reason::kAddSecondaryAccount)),
        base::StringPrintf(
            "runMochaTest('InlineLoginSigninBlockedByPolicyPageTest', '%s');",
            testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(InlineLoginSigninBlockedByPolicyPageBrowserTest,
                       BlockedSigninPage) {
  RunTestCase("BlockedSigninPage");
}

IN_PROC_BROWSER_TEST_F(InlineLoginSigninBlockedByPolicyPageBrowserTest,
                       OkButton) {
  RunTestCase("OkButton");
}

IN_PROC_BROWSER_TEST_F(InlineLoginSigninBlockedByPolicyPageBrowserTest,
                       FireWebUIListenerCallback) {
  RunTestCase("FireWebUIListenerCallback");
}

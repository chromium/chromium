// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/test/browser_test.h"

class GaiaAuthHostTest : public WebUIMochaBrowserTest {
 protected:
  GaiaAuthHostTest() {
    set_test_loader_host(chrome::kChromeUIChromeSigninHost);
  }

  void RunTestCase(const std::string& file_name) {
    WebUIMochaBrowserTest::RunTest(
        base::StringPrintf(
            "%s&reason=%d", file_name.c_str(),
            static_cast<int>(signin_metrics::Reason::kFetchLstOnly)),
        "mocha.run()");
  }
};

IN_PROC_BROWSER_TEST_F(GaiaAuthHostTest, PasswordChangeAuthenticator) {
  RunTestCase("gaia_auth_host/password_change_authenticator_test.js");
}

IN_PROC_BROWSER_TEST_F(GaiaAuthHostTest, SamlPasswordAttributes) {
  RunTestCase("gaia_auth_host/saml_password_attributes_test.js");
}

IN_PROC_BROWSER_TEST_F(GaiaAuthHostTest, SamlTimestamps) {
  RunTestCase("gaia_auth_host/saml_timestamps_test.js");
}

IN_PROC_BROWSER_TEST_F(GaiaAuthHostTest, SamlUsernameAutofill) {
  RunTestCase("gaia_auth_host/saml_username_autofill_test.js");
}

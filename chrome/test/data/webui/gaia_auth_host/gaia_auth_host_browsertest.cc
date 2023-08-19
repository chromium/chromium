// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class GaiaAuthHostTest : public WebUIMochaBrowserTest {
 protected:
  GaiaAuthHostTest() {
    set_test_loader_host(chrome::kChromeUIChromeSigninHost);
  }
};

IN_PROC_BROWSER_TEST_F(GaiaAuthHostTest, PasswordChangeAuthenticator) {
  RunTest("gaia_auth_host/password_change_authenticator_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(GaiaAuthHostTest, SamlPasswordAttributes) {
  RunTest("gaia_auth_host/saml_password_attributes_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(GaiaAuthHostTest, SamlTimestamps) {
  RunTest("gaia_auth_host/saml_timestamps_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(GaiaAuthHostTest, SamlUsernameAutofill) {
  RunTest("gaia_auth_host/saml_username_autofill_test.js", "mocha.run()");
}

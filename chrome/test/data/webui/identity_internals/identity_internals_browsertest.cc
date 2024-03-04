// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/webui/identity_internals_ui_browsertest.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class BaseIdentityInternalsWebUITest : public IdentityInternalsUIBrowserTest {
 protected:
  BaseIdentityInternalsWebUITest() {
    set_test_loader_host(chrome::kChromeUIIdentityInternalsHost);
  }

  void RunTestCase(const std::string& testSuite, const std::string& testCase) {
    RunTestWithoutTestLoader(
        "identity_internals/identity_internals_test.js",
        base::StringPrintf("runMochaTest('%s', '%s');", testSuite.c_str(),
                           testCase.c_str()));
  }
};

// Test verifying chrome://identity-internals Web UI when the token cache is
// empty.
IN_PROC_BROWSER_TEST_F(BaseIdentityInternalsWebUITest, emptyTokenCache) {
  RunTestCase("NoToken", "emptyTokenCache");
}

using IdentityInternalsSingleTokenWebUITest = BaseIdentityInternalsWebUITest;

// Test for listing a token cache with a single token. It uses a known extension
// - the Chrome Web Store, in order to check the extension name.
IN_PROC_BROWSER_TEST_F(IdentityInternalsSingleTokenWebUITest, getAllTokens) {
  SetupTokenCacheWithStoreApp();
  RunTestCase("SingleToken", "getAllTokens");
}

// Test ensuring the getters on the BaseIdentityInternalsWebUITest work
// correctly. They are implemented on the child class, because the parent does
// not have any tokens to display.
IN_PROC_BROWSER_TEST_F(IdentityInternalsSingleTokenWebUITest, verifyGetters) {
  SetupTokenCacheWithStoreApp();
  RunTestCase("SingleToken", "verifyGetters");
}

using IdentityInternalsMultipleTokensWebUITest = BaseIdentityInternalsWebUITest;

// Test for listing a token cache with multiple tokens. Names of the extensions
// are empty, because extensions are faked, and not present in the extension
// service.
IN_PROC_BROWSER_TEST_F(IdentityInternalsMultipleTokensWebUITest, getAllTokens) {
  SetupTokenCache(2);
  RunTestCase("MultipleTokens", "getAllTokens");
}

IN_PROC_BROWSER_TEST_F(IdentityInternalsMultipleTokensWebUITest, revokeToken) {
  SetupTokenCache(2);
  RunTestCase("MultipleTokens", "revokeToken");
}

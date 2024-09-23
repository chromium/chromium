// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

class PrivateStateTokensTest : public WebUIMochaBrowserTest {
 protected:
  PrivateStateTokensTest() {
    scoped_feature_list_.InitWithFeatures(
        {privacy_sandbox::kPrivateStateTokensDevUI,
         privacy_sandbox::kPrivacySandboxInternalsDevUI},
        {});
    set_test_loader_host(chrome::kChromeUIPrivacySandboxInternalsHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivateStateTokensTest, App) {
  RunTest("privacy_sandbox/internals/private_state_tokens/app_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrivateStateTokensTest, Toolbar) {
  RunTest("privacy_sandbox/internals/private_state_tokens/toolbar_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrivateStateTokensTest, ListItem) {
  RunTest(
      "privacy_sandbox/internals/private_state_tokens/"
      "list_item_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrivateStateTokensTest, Sidebar) {
  RunTest(
      "privacy_sandbox/internals/private_state_tokens/"
      "sidebar_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrivateStateTokensTest, ListContainer) {
  RunTest(
      "privacy_sandbox/internals/private_state_tokens/"
      "list_container_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrivateStateTokensTest, Metadata) {
  RunTest(
      "privacy_sandbox/internals/private_state_tokens/"
      "metadata_test.js",
      "mocha.run()");
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

class PrivacySandboxInternalsMochaTest : public WebUIMochaBrowserTest {
 public:
  PrivacySandboxInternalsMochaTest() {
    set_test_loader_host(chrome::kChromeUIPrivacySandboxInternalsHost);
  }

 private:
  base::test::ScopedFeatureList features_{
      privacy_sandbox::kPrivacySandboxInternalsDevUI};
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMochaTest, CustomElements) {
  RunTest("privacy_sandbox/internals/privacy_sandbox_internals_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMochaTest,
                       ContentSettingsCustomElement) {
  RunTest("privacy_sandbox/internals/content_settings_test.js", "mocha.run();");
}

}  // namespace

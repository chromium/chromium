// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class FlagsUiBrowserTest : public WebUIMochaBrowserTest {
 protected:
  FlagsUiBrowserTest() { set_test_loader_host(chrome::kChromeUIFlagsHost); }
};

IN_PROC_BROWSER_TEST_F(FlagsUiBrowserTest, App) {
  RunTest("flags/app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(FlagsUiBrowserTest, Experiment) {
  RunTest("flags/experiment_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(FlagsUiBrowserTest, Url) {
  // Invoke the test from a URL with an experiment reference tag, i.e.,
  // chrome://flags/#test-feature.
  set_test_loader_host(std::string(chrome::kChromeUIFlagsHost) +
                       "/#test-feature");
  RunTestWithoutTestLoader("flags/url_test.js", "mocha.run()");
}

class FlagsDeprecatedUiBrowserTest : public WebUIMochaBrowserTest {
 protected:
  FlagsDeprecatedUiBrowserTest() {
    set_test_loader_host(chrome::kChromeUIFlagsHost +
                         std::string("/deprecated"));
  }
};

IN_PROC_BROWSER_TEST_F(FlagsDeprecatedUiBrowserTest, App) {
  RunTest("flags/deprecated_test.js", "mocha.run()");
}

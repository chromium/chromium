// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class InvalidationsWebUITest : public WebUIMochaBrowserTest {
 protected:
  InvalidationsWebUITest() {
    set_test_loader_host(std::string(chrome::kChromeUIInvalidationsHost) +
                         "?isTest");
  }

  void RunTestCase(const std::string& testCase) {
    InvalidationsWebUITest::RunTestWithoutTestLoader(
        "invalidations/invalidations_test.js",
        base::StringPrintf("runMochaTest('InvalidationsTest', '%s');",
                           testCase.c_str()));
  }
};

// Test that registering an invalidations appears properly on the textarea.
IN_PROC_BROWSER_TEST_F(InvalidationsWebUITest, RegisterNewInvalidation) {
  RunTestCase("RegisterNewInvalidation");
}

// Test that changing the Invalidations Service state appears both in the
// span and in the textarea.
IN_PROC_BROWSER_TEST_F(InvalidationsWebUITest, ChangeInvalidationsState) {
  RunTestCase("ChangeInvalidationsState");
}

// Test that objects ids appear on the table.
IN_PROC_BROWSER_TEST_F(InvalidationsWebUITest, RegisterNewIds) {
  RunTestCase("RegisterNewIds");
}

// Test that registering new handlers appear on the website.
IN_PROC_BROWSER_TEST_F(InvalidationsWebUITest, UpdateRegisteredHandlers) {
  RunTestCase("UpdateRegisteredHandlers");
}

// Test that an object showing internal state is correctly displayed.
IN_PROC_BROWSER_TEST_F(InvalidationsWebUITest, UpdateInternalDisplay) {
  RunTestCase("UpdateInternalDisplay");
}

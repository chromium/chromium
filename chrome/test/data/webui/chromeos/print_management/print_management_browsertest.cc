// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/webui/print_management/url_constants.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

/**
 * @fileoverview Test suite for chrome://print-management.
 */

namespace ash {

namespace {

class PrintManagementBrowserTest : public WebUIMochaBrowserTest {
 public:
  PrintManagementBrowserTest() {
    set_test_loader_host(::ash::kChromeUIPrintManagementHost);
  }

 protected:
  void RunTestAtPath(const std::string& testFilePath) {
    auto testPath = base::StringPrintf("chromeos/print_management/%s",
                                       testFilePath.c_str());
    WebUIMochaBrowserTest::RunTest(testPath, "mocha.run()");
  }
};

IN_PROC_BROWSER_TEST_F(PrintManagementBrowserTest, PrintManagementTest) {
  RunTestAtPath("print_management_test.js");
}

}  // namespace

}  // namespace ash

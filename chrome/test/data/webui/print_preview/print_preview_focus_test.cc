// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class PrintPreviewFocusTest : public WebUIMochaFocusTest {
 protected:
  PrintPreviewFocusTest() { set_test_loader_host(chrome::kChromeUIPrintHost); }
};

class PrintPreviewPagesSettingsFocusTest : public PrintPreviewFocusTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewFocusTest::RunTest(
        "print_preview/pages_settings_test.js",
        base::StringPrintf("runMochaTest('PagesSettingsTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewPagesSettingsFocusTest, ClearInput) {
  RunTestCase("ClearInput");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPagesSettingsFocusTest,
                       InputNotDisabledOnValidityChange) {
  RunTestCase("InputNotDisabledOnValidityChange");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPagesSettingsFocusTest,
                       EnterOnInputTriggersPrint) {
  RunTestCase("EnterOnInputTriggersPrint");
}

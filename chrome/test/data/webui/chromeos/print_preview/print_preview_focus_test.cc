// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class PrintPreviewFocusTest : public WebUIMochaFocusTest {
 protected:
  PrintPreviewFocusTest() { set_test_loader_host(chrome::kChromeUIPrintHost); }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewFocusTest, FocusPrintButtonOnReady) {
  RunTest("chromeos/print_preview/button_strip_interactive_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewFocusTest, BlurResetsEmptyNumberInput) {
  RunTest("chromeos/print_preview/number_settings_section_interactive_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewFocusTest, AutoFocusScalingInput) {
  RunTest("chromeos/print_preview/scaling_settings_interactive_test.js",
          "mocha.run()");
}

class PrintPreviewPagesSettingsFocusTest : public PrintPreviewFocusTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewFocusTest::RunTest(
        "chromeos/print_preview/pages_settings_test.js",
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

class PrintPreviewDestinationDropdownCrosFocusTest
    : public PrintPreviewFocusTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewFocusTest::RunTest(
        "chromeos/print_preview/destination_dropdown_cros_test.js",
        base::StringPrintf("runMochaTest('DestinationDropdownCrosTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDropdownCrosFocusTest,
                       ClickCloses) {
  RunTestCase("ClickCloses");
}

class PrintPreviewDestinationDialogFocusTest : public PrintPreviewFocusTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewFocusTest::RunTest(
        "chromeos/print_preview/destination_dialog_cros_interactive_test.js",
        base::StringPrintf(
            "runMochaTest('DestinationDialogInteractiveTest', '%s');",
            testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDialogFocusTest, FocusSearchBox) {
  RunTestCase("FocusSearchBox");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDialogFocusTest,
                       EscapeSearchBox) {
  RunTestCase("EscapeSearchBox");
}

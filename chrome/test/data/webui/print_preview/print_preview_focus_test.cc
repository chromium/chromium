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

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
#if BUILDFLAG(IS_WIN)
#define MAYBE_FocusPrintButtonOnReady DISABLED_FocusPrintButtonOnReady
#else
#define MAYBE_FocusPrintButtonOnReady FocusPrintButtonOnReady
#endif
IN_PROC_BROWSER_TEST_F(PrintPreviewFocusTest, MAYBE_FocusPrintButtonOnReady) {
  RunTest("print_preview/button_strip_interactive_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewFocusTest, BlurResetsEmptyNumberInput) {
  RunTest("print_preview/number_settings_section_interactive_test.js",
          "mocha.run()");
}

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
#if BUILDFLAG(IS_WIN)
#define MAYBE_AutoFocusScalingInput DISABLED_AutoFocusScalingInput
#else
#define MAYBE_AutoFocusScalingInput AutoFocusScalingInput
#endif
IN_PROC_BROWSER_TEST_F(PrintPreviewFocusTest, MAYBE_AutoFocusScalingInput) {
  RunTest("print_preview/scaling_settings_interactive_test.js", "mocha.run()");
}

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

#if BUILDFLAG(IS_CHROMEOS)
class PrintPreviewDestinationDropdownCrosFocusTest
    : public PrintPreviewFocusTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewFocusTest::RunTest(
        "print_preview/destination_dropdown_cros_test.js",
        base::StringPrintf("runMochaTest('DestinationDropdownCrosTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDropdownCrosFocusTest,
                       ClickCloses) {
  RunTestCase("ClickCloses");
}
#endif

class PrintPreviewDestinationDialogFocusTest : public PrintPreviewFocusTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewFocusTest::RunTest(
#if BUILDFLAG(IS_CHROMEOS)
        "print_preview/destination_dialog_cros_interactive_test.js",
#else
        "print_preview/destination_dialog_interactive_test.js",
#endif
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

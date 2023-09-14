// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class PrintPreviewBrowserTest : public WebUIMochaBrowserTest {
 protected:
  PrintPreviewBrowserTest() {
    set_test_loader_host(chrome::kChromeUIPrintHost);
  }
};

using PrintPreviewTest = PrintPreviewBrowserTest;

// Note: Keep tests below in alphabetical ordering.
IN_PROC_BROWSER_TEST_F(PrintPreviewTest, ColorSettingsTest) {
  RunTest("print_preview/color_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, CopiesSettingsTest) {
  RunTest("print_preview/copies_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, DpiSettingsTest) {
  RunTest("print_preview/dpi_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, DuplexSettingsTest) {
  RunTest("print_preview/duplex_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, LayoutSettingsTest) {
  RunTest("print_preview/layout_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, MarginsSettingsTest) {
  RunTest("print_preview/margins_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, MediaSizeSettingsTest) {
  RunTest("print_preview/media_size_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, MediaTypeSettingsTest) {
  RunTest("print_preview/media_type_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, ModelSettingsAvailabilityTest) {
  RunTest("print_preview/model_settings_availability_test.js", "mocha.run()");
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewTest, ModelSettingsPolicyTest) {
  RunTest("print_preview/model_settings_policy_test.js", "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, OtherOptionsSettingsTest) {
  RunTest("print_preview/other_options_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, PagesPerSheetSettingsTest) {
  RunTest("print_preview/pages_per_sheet_settings_test.js", "mocha.run()");
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewTest, PinSettingsTest) {
  RunTest("print_preview/pin_settings_test.js", "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, SettingsSelectTest) {
  RunTest("print_preview/settings_select_test.js", "mocha.run()");
}

class PrintPreviewAppTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/print_preview_app_test.js",
        base::StringPrintf("runMochaTest('PrintPreviewAppTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewAppTest, PrintPresets) {
  RunTestCase("PrintPresets");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewAppTest, DestinationsManaged) {
  RunTestCase("DestinationsManaged");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewAppTest, HeaderFooterManaged) {
  RunTestCase("HeaderFooterManaged");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewAppTest, CssBackgroundManaged) {
  RunTestCase("CssBackgroundManaged");
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewAppTest, SheetsManaged) {
  RunTestCase("SheetsManaged");
}
#endif

class PrintPreviewSidebarTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/print_preview_sidebar_test.js",
        base::StringPrintf("runMochaTest('PrintPreviewSidebarTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewSidebarTest,
                       SettingsSectionsVisibilityChange) {
  RunTestCase("SettingsSectionsVisibilityChange");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewSidebarTest, SheetCountWithDuplex) {
  RunTestCase("SheetCountWithDuplex");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewSidebarTest, SheetCountWithCopies) {
  RunTestCase("SheetCountWithCopies");
}

class PrintPreviewPagesSettingsTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/pages_settings_test.js",
        base::StringPrintf("runMochaTest('PagesSettingsTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewPagesSettingsTest, PagesDropdown) {
  RunTestCase("PagesDropdown");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPagesSettingsTest, NoParityOptions) {
  RunTestCase("NoParityOptions");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPagesSettingsTest,
                       ParitySelectionMemorized) {
  RunTestCase("ParitySelectionMemorized");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPagesSettingsTest, ValidPageRanges) {
  RunTestCase("ValidPageRanges");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPagesSettingsTest, InvalidPageRanges) {
  RunTestCase("InvalidPageRanges");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPagesSettingsTest, NupChangesPages) {
  RunTestCase("NupChangesPages");
}

class PrintPreviewPdfToolbarManagerTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/pdf_toolbar_manager_test.js",
        base::StringPrintf("runMochaTest('PdfToolbarManagerTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewPdfToolbarManagerTest, KeyboardNavigation) {
  RunTestCase("KeyboardNavigation");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPdfToolbarManagerTest,
                       ResetKeyboardNavigation) {
  RunTestCase("ResetKeyboardNavigation");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPdfToolbarManagerTest, TouchInteraction) {
  RunTestCase("TouchInteraction");
}

class PrintPreviewPdfViewerTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/pdf_viewer_test.js",
        base::StringPrintf("runMochaTest('PdfViewerTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewPdfViewerTest, Basic) {
  RunTestCase("Basic");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPdfViewerTest, PageIndicator) {
  RunTestCase("PageIndicator");
}

class PrintPreviewPdfZoomToolbarTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/pdf_zoom_toolbar_test.js",
        base::StringPrintf("runMochaTest('PdfZoomToolbarTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewPdfZoomToolbarTest, Toggle) {
  RunTestCase("Toggle");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPdfZoomToolbarTest, ForceFitToPage) {
  RunTestCase("ForceFitToPage");
}

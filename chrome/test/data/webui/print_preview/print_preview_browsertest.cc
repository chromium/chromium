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
IN_PROC_BROWSER_TEST_F(PrintPreviewTest, ColorSettings) {
  RunTest("print_preview/color_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, CopiesSettings) {
  RunTest("print_preview/copies_settings_test.js", "mocha.run()");
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewTest, DestinationSelect) {
  RunTest("print_preview/destination_select_test.js", "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, DpiSettings) {
  RunTest("print_preview/dpi_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, DuplexSettings) {
  RunTest("print_preview/duplex_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, InvalidSettings) {
  RunTest("print_preview/invalid_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, LayoutSettings) {
  RunTest("print_preview/layout_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, MarginsSettings) {
  RunTest("print_preview/margins_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, MediaSizeSettings) {
  RunTest("print_preview/media_size_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, MediaTypeSettings) {
  RunTest("print_preview/media_type_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, ModelSettingsAvailability) {
  RunTest("print_preview/model_settings_availability_test.js", "mocha.run()");
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewTest, ModelSettingsPolicy) {
  RunTest("print_preview/model_settings_policy_test.js", "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, OtherOptionsSettings) {
  RunTest("print_preview/other_options_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, PagesPerSheetSettings) {
  RunTest("print_preview/pages_per_sheet_settings_test.js", "mocha.run()");
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewTest, PinSettings) {
  RunTest("print_preview/pin_settings_test.js", "mocha.run()");
}
#endif

// Test is flaky on LaCros, see crbug.com/328690296
#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(PrintPreviewTest, SearchableDropDownCros) {
  RunTest("print_preview/searchable_drop_down_cros_test.js", "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, SelectMixin) {
  RunTest("print_preview/select_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, SettingsSelect) {
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

class PrintPreviewPolicyTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/policy_test.js",
        base::StringPrintf("runMochaTest('PolicyTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewPolicyTest, HeaderFooterPolicy) {
  RunTestCase("HeaderFooterPolicy");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPolicyTest, CssBackgroundPolicy) {
  RunTestCase("CssBackgroundPolicy");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPolicyTest, MediaSizePolicy) {
  RunTestCase("MediaSizePolicy");
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewPolicyTest, SheetsPolicy) {
  RunTestCase("SheetsPolicy");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPolicyTest, ColorPolicy) {
  RunTestCase("ColorPolicy");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPolicyTest, DuplexPolicy) {
  RunTestCase("DuplexPolicy");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPolicyTest, PinPolicy) {
  RunTestCase("PinPolicy");
}
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(PrintPreviewPolicyTest, PrintPdfAsImageAvailability) {
  RunTestCase("PrintPdfAsImageAvailability");
}
#endif

IN_PROC_BROWSER_TEST_F(PrintPreviewPolicyTest, PrintPdfAsImageDefault) {
  RunTestCase("PrintPdfAsImageDefault");
}

class PrintPreviewNumberSettingsSectionTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/number_settings_section_test.js",
        base::StringPrintf("runMochaTest('NumberSettingsSectionTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewNumberSettingsSectionTest,
                       BlocksInvalidKeys) {
  RunTestCase("BlocksInvalidKeys");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewNumberSettingsSectionTest,
                       UpdatesErrorMessage) {
  RunTestCase("UpdatesErrorMessage");
}

class PrintPreviewRestoreStateTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/restore_state_test.js",
        base::StringPrintf("runMochaTest('RestoreStateTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewRestoreStateTest, RestoreTrueValues) {
  RunTestCase("RestoreTrueValues");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewRestoreStateTest, RestoreFalseValues) {
  RunTestCase("RestoreFalseValues");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewRestoreStateTest, SaveValues) {
  RunTestCase("SaveValues");
}

class PrintPreviewModelTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/model_test.js",
        base::StringPrintf("runMochaTest('ModelTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewModelTest, SetStickySettings) {
  RunTestCase("SetStickySettings");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewModelTest, SetPolicySettings) {
  RunTestCase("SetPolicySettings");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewModelTest, GetPrintTicket) {
  RunTestCase("GetPrintTicket");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewModelTest, GetCloudPrintTicket) {
  RunTestCase("GetCloudPrintTicket");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewModelTest, ChangeDestination) {
  RunTestCase("ChangeDestination");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewModelTest, RemoveUnsupportedDestinations) {
  RunTestCase("RemoveUnsupportedDestinations");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewModelTest, CddResetToDefault) {
  RunTestCase("CddResetToDefault");
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewModelTest, PrintToGoogleDriveCros) {
  RunTestCase("PrintToGoogleDriveCros");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewModelTest,
                       PolicyDefaultsOverrideDestinationDefaults) {
  RunTestCase("PolicyDefaultsOverrideDestinationDefaults");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewModelTest,
                       UserSelectedOptionsOverridePolicyDefaults) {
  RunTestCase("UserSelectedOptionsOverridePolicyDefaults");
}
#endif

class PrintPreviewPreviewGenerationTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/preview_generation_test.js",
        base::StringPrintf("runMochaTest('PreviewGenerationTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest, Color) {
  RunTestCase("Color");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest, CssBackground) {
  RunTestCase("CssBackground");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest, HeaderFooter) {
  RunTestCase("HeaderFooter");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest, Layout) {
  RunTestCase("Layout");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest, Margins) {
  RunTestCase("Margins");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest, CustomMargins) {
  RunTestCase("CustomMargins");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest, MediaSize) {
  RunTestCase("MediaSize");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest, PageRange) {
  RunTestCase("PageRange");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest, SelectionOnly) {
  RunTestCase("SelectionOnly");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest, PagesPerSheet) {
  RunTestCase("PagesPerSheet");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest, Scaling) {
  RunTestCase("Scaling");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest, ScalingPdf) {
  RunTestCase("ScalingPdf");
}

#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest, Rasterize) {
  RunTestCase("Rasterize");
}
#endif

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest, Destination) {
  RunTestCase("Destination");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest,
                       ChangeMarginsByPagesPerSheet) {
  RunTestCase("ChangeMarginsByPagesPerSheet");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest,
                       ZeroDefaultMarginsClearsHeaderFooter) {
  RunTestCase("ZeroDefaultMarginsClearsHeaderFooter");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewGenerationTest, PageSizeCalculation) {
  RunTestCase("PageSizeCalculation");
}

#if !BUILDFLAG(IS_CHROMEOS)
class PrintPreviewLinkContainerTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/link_container_test.js",
        base::StringPrintf("runMochaTest('LinkContainerTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewLinkContainerTest, HideInAppKioskMode) {
  RunTestCase("HideInAppKioskMode");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewLinkContainerTest, SystemDialogLinkClick) {
  RunTestCase("SystemDialogLinkClick");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewLinkContainerTest,
                       SystemDialogLinkProperties) {
  RunTestCase("SystemDialogLinkProperties");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewLinkContainerTest, InvalidState) {
  RunTestCase("InvalidState");
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(PrintPreviewLinkContainerTest, OpenInPreviewLinkClick) {
  RunTestCase("OpenInPreviewLinkClick");
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
class PrintPreviewSystemDialogTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/system_dialog_test.js",
        base::StringPrintf("runMochaTest('SystemDialogTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewSystemDialogTest, LinkTriggersLocalPrint) {
  RunTestCase("LinkTriggersLocalPrint");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewSystemDialogTest,
                       InvalidSettingsDisableLink) {
  RunTestCase("InvalidSettingsDisableLink");
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

class PrintPreviewDestinationStoreTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/destination_store_test.js",
        base::StringPrintf("runMochaTest('DestinationStoreTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest,
                       SingleRecentDestination) {
  RunTestCase("SingleRecentDestination");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest,
                       RecentDestinationsFallback) {
  RunTestCase("RecentDestinationsFallback");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest,
                       MultipleRecentDestinations) {
  RunTestCase("MultipleRecentDestinations");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest,
                       MultipleRecentDestinationsOneRequest) {
  RunTestCase("MultipleRecentDestinationsOneRequest");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest,
                       DefaultDestinationSelectionRules) {
  RunTestCase("DefaultDestinationSelectionRules");
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest,
                       SystemDefaultPrinterPolicy) {
  RunTestCase("SystemDefaultPrinterPolicy");
}
#endif

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest,
                       KioskModeSelectsFirstPrinter) {
  RunTestCase("KioskModeSelectsFirstPrinter");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest,
                       LoadAndSelectDestination) {
  RunTestCase("LoadAndSelectDestination");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest, NoPrintersShowsError) {
  RunTestCase("NoPrintersShowsError");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest, RecentSaveAsPdf) {
  RunTestCase("RecentSaveAsPdf");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest,
                       DestinationAlreadySelected) {
  RunTestCase("DestinationAlreadySelected");
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest, LoadSaveToDriveCros) {
  RunTestCase("LoadSaveToDriveCros");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest, SaveToDriveDisabled) {
  RunTestCase("SaveToDriveDisabled");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest,
                       ObserveLocalPrintersAfterSuccessfulSearch) {
  RunTestCase("ObserveLocalPrintersAfterSuccessfulSearch");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest,
                       ObserveLocalPrintersAfterNoSearch) {
  RunTestCase("ObserveLocalPrintersAfterNoSearch");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest,
                       LocalPrintersUpdatedEventPrintersAdded) {
  RunTestCase("LocalPrintersUpdatedEventPrintersAdded");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest,
                       LocalPrintersUpdatedEventStatusUpdate) {
  RunTestCase("LocalPrintersUpdatedEventStatusUpdate");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationStoreTest,
                       PrinterStatusOnlineChange) {
  RunTestCase("PrinterStatusOnlineChange");
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
class PrintPreviewPrinterSetupInfoCrosTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/printer_setup_info_cros_test.js",
        base::StringPrintf("runMochaTest('PrinterSetupInfoTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewPrinterSetupInfoCrosTest, ElementDisplays) {
  RunTestCase("ElementDisplays");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPrinterSetupInfoCrosTest, ButtonLocalized) {
  RunTestCase("ButtonLocalized");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPrinterSetupInfoCrosTest,
                       ManagePrintersButton) {
  RunTestCase("ManagePrintersButton");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPrinterSetupInfoCrosTest,
                       MessageMatchesMessageType) {
  RunTestCase("MessageMatchesMessageType");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPrinterSetupInfoCrosTest,
                       ManagePrintersButtonMetrics) {
  RunTestCase("ManagePrintersButtonMetrics");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPrinterSetupInfoCrosTest,
                       DoNotShowManagePrinters) {
  RunTestCase("DoNotShowManagePrinters");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPrinterSetupInfoCrosTest,
                       HideIllustrationForSmallWindow) {
  RunTestCase("HideIllustrationForSmallWindow");
}

class PrintPreviewPrintServerStoreTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/print_server_store_test.js",
        base::StringPrintf("runMochaTest('PrintServerStoreTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewPrintServerStoreTest, ChoosePrintServers) {
  RunTestCase("ChoosePrintServers");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPrintServerStoreTest, PrintServersChanged) {
  RunTestCase("PrintServersChanged");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPrintServerStoreTest,
                       GetPrintServersConfig) {
  RunTestCase("GetPrintServersConfig");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPrintServerStoreTest,
                       ServerPrintersLoading) {
  RunTestCase("ServerPrintersLoading");
}
#endif  // BUILDFLAG(IS_CHROMEOS)

class PrintPreviewDestinationDialogTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/destination_dialog_test.js",
        base::StringPrintf("runMochaTest('DestinationDialogTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDialogTest, PrinterList) {
  RunTestCase("PrinterList");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDialogTest,
                       PrinterListPreloaded) {
  RunTestCase("PrinterListPreloaded");
}

#if BUILDFLAG(IS_CHROMEOS)
class PrintPreviewDestinationDialogCrosTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/destination_dialog_cros_test.js",
        base::StringPrintf("runMochaTest('DestinationDialogCrosTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDialogCrosTest,
                       ShowProvisionalDialog) {
  RunTestCase("ShowProvisionalDialog");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDialogCrosTest,
                       PrintServersChanged) {
  RunTestCase("PrintServersChanged");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDialogCrosTest,
                       PrintServerSelected) {
  RunTestCase("PrintServerSelected");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDialogCrosTest,
                       PrinterSetupAssistanceHasDestinations) {
  RunTestCase("PrinterSetupAssistanceHasDestinations");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDialogCrosTest,
                       PrinterSetupAssistanceHasDestinationsSearching) {
  RunTestCase("PrinterSetupAssistanceHasDestinationsSearching");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDialogCrosTest,
                       PrinterSetupAssistanceHasNoDestinations) {
  RunTestCase("PrinterSetupAssistanceHasNoDestinations");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDialogCrosTest,
                       ManagePrintersMetrics_HasDestinations) {
  RunTestCase("ManagePrintersMetrics_HasDestinations");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDialogCrosTest,
                       ManagePrintersMetrics_HasNoDestinations) {
  RunTestCase("ManagePrintersMetrics_HasNoDestinations");
}

IN_PROC_BROWSER_TEST_F(
    PrintPreviewDestinationDialogCrosTest,
    PrinterSetupAssistanceHasDestinations_ShowManagedPrintersFalse) {
  RunTestCase("PrinterSetupAssistanceHasDestinations_ShowManagedPrintersFalse");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDialogCrosTest,
                       CorrectlyDisplaysAndHidesLoadingUI) {
  RunTestCase("CorrectlyDisplaysAndHidesLoadingUI");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDialogCrosTest,
                       NewDestinationsShowsAndResizesList) {
  RunTestCase("NewDestinationsShowsAndResizesList");
}
#endif  // BUILDFLAG(IS_CHROMEOS)

class PrintPreviewAdvancedDialogTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/advanced_dialog_test.js",
        base::StringPrintf("runMochaTest('AdvancedDialogTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewAdvancedDialogTest,
                       AdvancedSettings1Option) {
  RunTestCase("AdvancedSettings1Option");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewAdvancedDialogTest,
                       AdvancedSettings2Options) {
  RunTestCase("AdvancedSettings2Options");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewAdvancedDialogTest, AdvancedSettingsApply) {
  RunTestCase("AdvancedSettingsApply");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewAdvancedDialogTest,
                       AdvancedSettingsApplyWithEnter) {
  RunTestCase("AdvancedSettingsApplyWithEnter");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewAdvancedDialogTest, AdvancedSettingsClose) {
  RunTestCase("AdvancedSettingsClose");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewAdvancedDialogTest, AdvancedSettingsFilter) {
  RunTestCase("AdvancedSettingsFilter");
}

class PrintPreviewPreviewAreaTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/preview_area_test.js",
        base::StringPrintf("runMochaTest('PreviewAreaTest', '%s');",
                           testCase.c_str()));
  }
};

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewAreaTest, StateChanges) {
  RunTestCase("StateChanges");
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewAreaTest,
                       StateChangesPrinterSetupCros) {
  RunTestCase("StateChangesPrinterSetupCros");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewAreaTest, ManagePrinterMetricsCros) {
  RunTestCase("ManagePrinterMetricsCros");
}
#endif

IN_PROC_BROWSER_TEST_F(PrintPreviewPreviewAreaTest, ViewportSizeChanges) {
  RunTestCase("ViewportSizeChanges");
}

class PrintPreviewCustomMarginsTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/custom_margins_test.js",
        base::StringPrintf("runMochaTest('CustomMarginsTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewCustomMarginsTest, ControlsCheck) {
  RunTestCase("ControlsCheck");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCustomMarginsTest, SetFromStickySettings) {
  RunTestCase("SetFromStickySettings");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCustomMarginsTest, DragControls) {
  RunTestCase("DragControls");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCustomMarginsTest, SetControlsWithTextbox) {
  RunTestCase("SetControlsWithTextbox");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCustomMarginsTest,
                       SetControlsWithTextboxMetric) {
  RunTestCase("SetControlsWithTextboxMetric");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCustomMarginsTest,
                       RestoreStickyMarginsAfterDefault) {
  RunTestCase("RestoreStickyMarginsAfterDefault");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCustomMarginsTest,
                       MediaSizeClearsCustomMargins) {
  RunTestCase("MediaSizeClearsCustomMargins");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCustomMarginsTest,
                       LayoutClearsCustomMargins) {
  RunTestCase("LayoutClearsCustomMargins");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCustomMarginsTest,
                       IgnoreDocumentMarginsFromPDF) {
  RunTestCase("IgnoreDocumentMarginsFromPDF");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCustomMarginsTest,
                       MediaSizeClearsCustomMarginsPDF) {
  RunTestCase("MediaSizeClearsCustomMarginsPDF");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCustomMarginsTest,
                       RequestScrollToOutOfBoundsTextbox) {
  RunTestCase("RequestScrollToOutOfBoundsTextbox");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewCustomMarginsTest, ControlsDisabledOnError) {
  RunTestCase("ControlsDisabledOnError");
}

class PrintPreviewDestinationSearchTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
#if BUILDFLAG(IS_CHROMEOS)
        "print_preview/destination_search_test_chromeos.js",
#else
        "print_preview/destination_search_test.js",
#endif
        base::StringPrintf("runMochaTest('DestinationSearchTest', '%s');",
                           testCase.c_str()));
  }
};

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSearchTest,
                       ReceiveSuccessfulSetup) {
  RunTestCase("ReceiveSuccessfulSetup");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSearchTest, ResolutionFails) {
  RunTestCase("ResolutionFails");
}

#else
IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSearchTest,
                       GetCapabilitiesSucceeds) {
  RunTestCase("GetCapabilitiesSucceeds");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSearchTest,
                       GetCapabilitiesFails) {
  RunTestCase("GetCapabilitiesFails");
}
#endif

class PrintPreviewHeaderTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/header_test.js",
        base::StringPrintf("runMochaTest('HeaderTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewHeaderTest, HeaderPrinterTypes) {
  RunTestCase("HeaderPrinterTypes");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewHeaderTest, HeaderChangesForState) {
  RunTestCase("HeaderChangesForState");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewHeaderTest, EnterprisePolicy) {
  RunTestCase("EnterprisePolicy");
}

class PrintPreviewButtonStripTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/button_strip_test.js",
        base::StringPrintf("runMochaTest('ButtonStripTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewButtonStripTest,
                       ButtonStripChangesForState) {
  RunTestCase("ButtonStripChangesForState");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewButtonStripTest, ButtonOrder) {
  RunTestCase("ButtonOrder");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewButtonStripTest, ButtonStripFiresEvents) {
  RunTestCase("ButtonStripFiresEvents");
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewButtonStripTest, InvalidPinDisablesPrint) {
  RunTestCase("InvalidPinDisablesPrint");
}
#endif

class PrintPreviewDestinationItemTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/destination_item_test.js",
        base::StringPrintf("runMochaTest('DestinationItemTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationItemTest, NoQuery) {
  RunTestCase("NoQuery");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationItemTest, QueryName) {
  RunTestCase("QueryName");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationItemTest, QueryDescription) {
  RunTestCase("QueryDescription");
}

#if BUILDFLAG(IS_CHROMEOS)
class PrintPreviewDestinationItemTestCros : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/destination_item_test_cros.js",
        base::StringPrintf("runMochaTest('DestinationItemTestCros', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationItemTestCros,
                       NewStatusUpdatesIcon) {
  RunTestCase("NewStatusUpdatesIcon");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationItemTestCros,
                       ChangingDestinationUpdatesIcon) {
  RunTestCase("ChangingDestinationUpdatesIcon");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationItemTestCros,
                       OnlyUpdateMatchingDestination) {
  RunTestCase("OnlyUpdateMatchingDestination");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationItemTestCros,
                       PrinterIconMapsToPrinterStatus) {
  RunTestCase("PrinterIconMapsToPrinterStatus");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationItemTestCros,
                       PrinterConnectionStatusClass) {
  RunTestCase("PrinterConnectionStatusClass");
}
#endif

class PrintPreviewAdvancedItemTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/advanced_item_test.js",
        base::StringPrintf("runMochaTest('AdvancedItemTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewAdvancedItemTest, DisplaySelect) {
  RunTestCase("DisplaySelect");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewAdvancedItemTest, DisplayInput) {
  RunTestCase("DisplayInput");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewAdvancedItemTest, UpdateSelect) {
  RunTestCase("UpdateSelect");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewAdvancedItemTest, UpdateInput) {
  RunTestCase("UpdateInput");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewAdvancedItemTest, QueryName) {
  RunTestCase("QueryName");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewAdvancedItemTest, QueryOption) {
  RunTestCase("QueryOption");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewAdvancedItemTest, QueryJapaneseCharacters) {
  RunTestCase("QueryJapaneseCharacters");
}

class PrintPreviewDestinationListTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/destination_list_test.js",
        base::StringPrintf("runMochaTest('DestinationListTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationListTest, FilterDestinations) {
  RunTestCase("FilterDestinations");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationListTest,
                       FireDestinationSelected) {
  RunTestCase("FireDestinationSelected");
}

class PrintPreviewPrintButtonTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/print_button_test.js",
        base::StringPrintf("runMochaTest('PrintButtonTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewPrintButtonTest, LocalPrintHidePreview) {
  RunTestCase("LocalPrintHidePreview");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPrintButtonTest, PDFPrintVisiblePreview) {
  RunTestCase("PDFPrintVisiblePreview");
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewPrintButtonTest,
                       SaveToDriveVisiblePreviewCros) {
  RunTestCase("SaveToDriveVisiblePreviewCros");
}
#endif

class PrintPreviewKeyEventTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/key_event_test.js",
        base::StringPrintf("runMochaTest('KeyEventTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewKeyEventTest, EnterTriggersPrint) {
  RunTestCase("EnterTriggersPrint");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewKeyEventTest, NumpadEnterTriggersPrint) {
  RunTestCase("NumpadEnterTriggersPrint");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewKeyEventTest, EnterOnInputTriggersPrint) {
  RunTestCase("EnterOnInputTriggersPrint");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewKeyEventTest, EnterOnDropdownDoesNotPrint) {
  RunTestCase("EnterOnDropdownDoesNotPrint");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewKeyEventTest, EnterOnButtonDoesNotPrint) {
  RunTestCase("EnterOnButtonDoesNotPrint");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewKeyEventTest, EnterOnCheckboxDoesNotPrint) {
  RunTestCase("EnterOnCheckboxDoesNotPrint");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewKeyEventTest, EscapeClosesDialogOnMacOnly) {
  RunTestCase("EscapeClosesDialogOnMacOnly");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewKeyEventTest,
                       CmdPeriodClosesDialogOnMacOnly) {
  RunTestCase("CmdPeriodClosesDialogOnMacOnly");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewKeyEventTest, CtrlShiftPOpensSystemDialog) {
  RunTestCase("CtrlShiftPOpensSystemDialog");
}

#if BUILDFLAG(IS_CHROMEOS)
class PrintPreviewPrinterStatusTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/printer_status_test.js",
        base::StringPrintf("runMochaTest('PrinterStatusTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewPrinterStatusTest,
                       PrinterStatusUpdatesColor) {
  RunTestCase("PrinterStatusUpdatesColor");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPrinterStatusTest, SendStatusRequestOnce) {
  RunTestCase("SendStatusRequestOnce");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPrinterStatusTest, HiddenStatusText) {
  RunTestCase("HiddenStatusText");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPrinterStatusTest, ChangeIcon) {
  RunTestCase("ChangeIcon");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPrinterStatusTest,
                       SuccessfulPrinterStatusAfterRetry) {
  RunTestCase("SuccessfulPrinterStatusAfterRetry");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPrinterStatusTest, StatusTextClass) {
  RunTestCase("StatusTextClass");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewPrinterStatusTest,
                       OverrideStatusForPrinterSetupInfo) {
  RunTestCase("OverrideStatusForPrinterSetupInfo");
}

class PrintPreviewDestinationDropdownCrosTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/destination_dropdown_cros_test.js",
        base::StringPrintf("runMochaTest('DestinationDropdownCrosTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDropdownCrosTest,
                       CorrectListItems) {
  RunTestCase("CorrectListItems");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDropdownCrosTest,
                       HighlightedAfterUpDown) {
  RunTestCase("HighlightedAfterUpDown");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDropdownCrosTest,
                       DestinationChangeAfterUpDown) {
  RunTestCase("DestinationChangeAfterUpDown");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDropdownCrosTest,
                       EnterOpensCloses) {
  RunTestCase("EnterOpensCloses");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDropdownCrosTest,
                       HighlightedFollowsMouse) {
  RunTestCase("HighlightedFollowsMouse");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDropdownCrosTest, Disabled) {
  RunTestCase("Disabled");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationDropdownCrosTest,
                       HighlightedWhenOpened) {
  RunTestCase("HighlightedWhenOpened");
}
#endif

class PrintPreviewDestinationSettingsTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/destination_settings_test.js",
        base::StringPrintf("runMochaTest('DestinationSettingsTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSettingsTest,
                       ChangeDropdownState) {
  RunTestCase("ChangeDropdownState");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSettingsTest,
                       NoRecentDestinations) {
  RunTestCase("NoRecentDestinations");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSettingsTest,
                       RecentDestinations) {
  RunTestCase("RecentDestinations");
}

// Flaky on Mac and Linux, see https://crbug.com/1147205
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_RecentDestinationsMissing DISABLED_RecentDestinationsMissing
#else
#define MAYBE_RecentDestinationsMissing RecentDestinationsMissing
#endif
IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSettingsTest,
                       MAYBE_RecentDestinationsMissing) {
  RunTestCase("RecentDestinationsMissing");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSettingsTest, SaveAsPdfRecent) {
  RunTestCase("SaveAsPdfRecent");
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSettingsTest, GoogleDriveRecent) {
  RunTestCase("GoogleDriveRecent");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSettingsTest,
                       GoogleDriveAutoselect) {
  RunTestCase("GoogleDriveAutoselect");
}
#endif

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSettingsTest, SelectSaveAsPdf) {
  RunTestCase("SelectSaveAsPdf");
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSettingsTest, SelectGoogleDrive) {
  RunTestCase("SelectGoogleDrive");
}
#endif

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSettingsTest,
                       SelectRecentDestination) {
  RunTestCase("SelectRecentDestination");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSettingsTest, OpenDialog) {
  RunTestCase("OpenDialog");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSettingsTest,
                       UpdateRecentDestinations) {
  RunTestCase("UpdateRecentDestinations");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSettingsTest, DisabledSaveAsPdf) {
  RunTestCase("DisabledSaveAsPdf");
}

// Flaky on Mac, see https://crbug.com/1146513.
#if BUILDFLAG(IS_MAC)
#define MAYBE_NoDestinations DISABLED_NoDestinations
#else
#define MAYBE_NoDestinations NoDestinations
#endif
IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSettingsTest,
                       MAYBE_NoDestinations) {
  RunTestCase("NoDestinations");
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSettingsTest, EulaIsRetrieved) {
  RunTestCase("EulaIsRetrieved");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDestinationSettingsTest,
                       SaveToDriveDisabled) {
  RunTestCase("SaveToDriveDisabled");
}
#endif

class PrintPreviewScalingSettingsTest : public PrintPreviewBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    PrintPreviewBrowserTest::RunTest(
        "print_preview/scaling_settings_test.js",
        base::StringPrintf("runMochaTest('ScalingSettingsTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewScalingSettingsTest,
                       ShowCorrectDropdownOptions) {
  RunTestCase("ShowCorrectDropdownOptions");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewScalingSettingsTest, SetScaling) {
  RunTestCase("SetScaling");
}

IN_PROC_BROWSER_TEST_F(PrintPreviewScalingSettingsTest,
                       InputNotDisabledOnValidityChange) {
  RunTestCase("InputNotDisabledOnValidityChange");
}

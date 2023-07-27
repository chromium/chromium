// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Print Preview tests. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN(`#include "content/public/test/browser_test.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
// Do not enable ash features in lacros chrome.
#define InitWithFeatures(enabled, disabled) InitWithFeatures({}, {})
#endif`);

const PrintPreviewTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/';
  }

  // The name of the mocha suite. Should be overridden by subclasses.
  get suiteName() {
    return null;
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(this.suiteName, testName);
  }
};

var PrintPreviewAppTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/print_preview_app_test.js';
  }

  /** @override */
  get suiteName() {
    return print_preview_app_test.suiteName;
  }
};

TEST_F('PrintPreviewAppTest', 'PrintPresets', function() {
  this.runMochaTest(print_preview_app_test.TestNames.PrintPresets);
});

TEST_F('PrintPreviewAppTest', 'DestinationsManaged', function() {
  this.runMochaTest(print_preview_app_test.TestNames.DestinationsManaged);
});

TEST_F('PrintPreviewAppTest', 'HeaderFooterManaged', function() {
  this.runMochaTest(print_preview_app_test.TestNames.HeaderFooterManaged);
});

TEST_F('PrintPreviewAppTest', 'CssBackgroundManaged', function() {
  this.runMochaTest(print_preview_app_test.TestNames.CssBackgroundManaged);
});

GEN('#if BUILDFLAG(IS_CHROMEOS)');
TEST_F('PrintPreviewAppTest', 'SheetsManaged', function() {
  this.runMochaTest(print_preview_app_test.TestNames.SheetsManaged);
});
GEN('#endif');

var PrintPreviewSidebarTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/print_preview_sidebar_test.js';
  }

  /** @override */
  get suiteName() {
    return print_preview_sidebar_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewSidebarTest', 'SettingsSectionsVisibilityChange', function() {
      this.runMochaTest(print_preview_sidebar_test.TestNames
                            .SettingsSectionsVisibilityChange);
    });

TEST_F('PrintPreviewSidebarTest', 'SheetCountWithDuplex', function() {
  this.runMochaTest(print_preview_sidebar_test.TestNames.SheetCountWithDuplex);
});

TEST_F('PrintPreviewSidebarTest', 'SheetCountWithCopies', function() {
  this.runMochaTest(print_preview_sidebar_test.TestNames.SheetCountWithCopies);
});

var PrintPreviewPagesSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/pages_settings_test.js';
  }

  /** @override */
  get suiteName() {
    return pages_settings_test.suiteName;
  }
};

TEST_F('PrintPreviewPagesSettingsTest', 'PagesDropdown', function() {
  this.runMochaTest(pages_settings_test.TestNames.PagesDropdown);
});

TEST_F('PrintPreviewPagesSettingsTest', 'NoParityOptions', function() {
  this.runMochaTest(pages_settings_test.TestNames.NoParityOptions);
});

TEST_F('PrintPreviewPagesSettingsTest', 'ParitySelectionMemorized', function() {
  this.runMochaTest(pages_settings_test.TestNames.ParitySelectionMemorized);
});

TEST_F('PrintPreviewPagesSettingsTest', 'ValidPageRanges', function() {
  this.runMochaTest(pages_settings_test.TestNames.ValidPageRanges);
});

TEST_F('PrintPreviewPagesSettingsTest', 'InvalidPageRanges', function() {
  this.runMochaTest(pages_settings_test.TestNames.InvalidPageRanges);
});

TEST_F('PrintPreviewPagesSettingsTest', 'NupChangesPages', function() {
  this.runMochaTest(pages_settings_test.TestNames.NupChangesPages);
});

var PrintPreviewPdfToolbarManagerTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/pdf_toolbar_manager_test.js';
  }

  /** @override */
  get suiteName() {
    return pdf_toolbar_manager_test.suiteName;
  }
};

TEST_F('PrintPreviewPdfToolbarManagerTest', 'KeyboardNavigation', function() {
  this.runMochaTest(pdf_toolbar_manager_test.TestNames.KeyboardNavigation);
});

TEST_F(
    'PrintPreviewPdfToolbarManagerTest', 'ResetKeyboardNavigation', function() {
      this.runMochaTest(
          pdf_toolbar_manager_test.TestNames.ResetKeyboardNavigation);
    });

TEST_F('PrintPreviewPdfToolbarManagerTest', 'TouchInteraction', function() {
  this.runMochaTest(pdf_toolbar_manager_test.TestNames.TouchInteraction);
});

var PrintPreviewPdfViewerTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/pdf_viewer_test.js';
  }

  /** @override */
  get suiteName() {
    return pdf_viewer_test.suiteName;
  }
};

TEST_F('PrintPreviewPdfViewerTest', 'Basic', function() {
  this.runMochaTest(pdf_viewer_test.TestNames.Basic);
});

TEST_F('PrintPreviewPdfViewerTest', 'PageIndicator', function() {
  this.runMochaTest(pdf_viewer_test.TestNames.PageIndicator);
});

var PrintPreviewPdfZoomToolbarTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/pdf_zoom_toolbar_test.js';
  }

  /** @override */
  get suiteName() {
    return pdf_zoom_toolbar_test.suiteName;
  }
};

TEST_F('PrintPreviewPdfZoomToolbarTest', 'Toggle', function() {
  this.runMochaTest(pdf_zoom_toolbar_test.TestNames.Toggle);
});

TEST_F('PrintPreviewPdfZoomToolbarTest', 'ForceFitToPage', function() {
  this.runMochaTest(pdf_zoom_toolbar_test.TestNames.ForceFitToPage);
});

var PrintPreviewPolicyTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/policy_test.js';
  }

  /** @override */
  get suiteName() {
    return policy_tests.suiteName;
  }
};

TEST_F('PrintPreviewPolicyTest', 'HeaderFooterPolicy', function() {
  this.runMochaTest(policy_tests.TestNames.HeaderFooterPolicy);
});

TEST_F('PrintPreviewPolicyTest', 'CssBackgroundPolicy', function() {
  this.runMochaTest(policy_tests.TestNames.CssBackgroundPolicy);
});

TEST_F('PrintPreviewPolicyTest', 'MediaSizePolicy', function() {
  this.runMochaTest(policy_tests.TestNames.MediaSizePolicy);
});

GEN('#if BUILDFLAG(IS_CHROMEOS)');
TEST_F('PrintPreviewPolicyTest', 'SheetsPolicy', function() {
  this.runMochaTest(policy_tests.TestNames.SheetsPolicy);
});

TEST_F('PrintPreviewPolicyTest', 'ColorPolicy', function() {
  this.runMochaTest(policy_tests.TestNames.ColorPolicy);
});

TEST_F('PrintPreviewPolicyTest', 'DuplexPolicy', function() {
  this.runMochaTest(policy_tests.TestNames.DuplexPolicy);
});

TEST_F('PrintPreviewPolicyTest', 'PinPolicy', function() {
  this.runMochaTest(policy_tests.TestNames.PinPolicy);
});
GEN('#endif');

GEN('#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)');
TEST_F('PrintPreviewPolicyTest', 'PrintPdfAsImageAvailability', function() {
  this.runMochaTest(policy_tests.TestNames.PrintPdfAsImageAvailability);
});
GEN('#endif');

TEST_F('PrintPreviewPolicyTest', 'PrintPdfAsImageDefault', function() {
  this.runMochaTest(policy_tests.TestNames.PrintPdfAsImageDefault);
});

var PrintPreviewSettingsSelectTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/settings_select_test.js';
  }
};

TEST_F('PrintPreviewSettingsSelectTest', 'All', function() {
  mocha.run();
});

var PrintPreviewSelectMixinTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/select_mixin_test.js';
  }

  /** @override */
  get suiteName() {
    return select_mixin_test.suiteName;
  }
};

TEST_F('PrintPreviewSelectMixinTest', 'CallProcessSelectChange', function() {
  this.runMochaTest(select_mixin_test.TestNames.CallProcessSelectChange);
});

var PrintPreviewNumberSettingsSectionTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/number_settings_section_test.js';
  }

  /** @override */
  get suiteName() {
    return number_settings_section_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewNumberSettingsSectionTest', 'BlocksInvalidKeys', function() {
      this.runMochaTest(
          number_settings_section_test.TestNames.BlocksInvalidKeys);
    });

TEST_F(
    'PrintPreviewNumberSettingsSectionTest', 'UpdatesErrorMessage', function() {
      this.runMochaTest(
          number_settings_section_test.TestNames.UpdatesErrorMessage);
    });

var PrintPreviewRestoreStateTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/restore_state_test.js';
  }

  /** @override */
  get suiteName() {
    return restore_state_test.suiteName;
  }
};

TEST_F('PrintPreviewRestoreStateTest', 'RestoreTrueValues', function() {
  this.runMochaTest(restore_state_test.TestNames.RestoreTrueValues);
});

TEST_F('PrintPreviewRestoreStateTest', 'RestoreFalseValues', function() {
  this.runMochaTest(restore_state_test.TestNames.RestoreFalseValues);
});

TEST_F('PrintPreviewRestoreStateTest', 'SaveValues', function() {
  this.runMochaTest(restore_state_test.TestNames.SaveValues);
});

var PrintPreviewModelTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/model_test.js';
  }

  /** @override */
  get suiteName() {
    return model_test.suiteName;
  }
};

TEST_F('PrintPreviewModelTest', 'SetStickySettings', function() {
  this.runMochaTest(model_test.TestNames.SetStickySettings);
});

TEST_F('PrintPreviewModelTest', 'SetPolicySettings', function() {
  this.runMochaTest(model_test.TestNames.SetPolicySettings);
});

TEST_F('PrintPreviewModelTest', 'GetPrintTicket', function() {
  this.runMochaTest(model_test.TestNames.GetPrintTicket);
});

TEST_F('PrintPreviewModelTest', 'GetCloudPrintTicket', function() {
  this.runMochaTest(model_test.TestNames.GetCloudPrintTicket);
});

TEST_F('PrintPreviewModelTest', 'ChangeDestination', function() {
  this.runMochaTest(model_test.TestNames.ChangeDestination);
});

TEST_F('PrintPreviewModelTest', 'RemoveUnsupportedDestinations', function() {
  this.runMochaTest(model_test.TestNames.RemoveUnsupportedDestinations);
});

TEST_F('PrintPreviewModelTest', 'CddResetToDefault', function() {
  this.runMochaTest(model_test.TestNames.CddResetToDefault);
});

GEN('#if BUILDFLAG(IS_CHROMEOS)');
var PrintPreviewModelTestCros = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/model_test.js';
  }

  /** @override */
  get suiteName() {
    return model_test.suiteName;
  }
};

TEST_F('PrintPreviewModelTestCros', 'PrintToGoogleDriveCros', function() {
  this.runMochaTest(model_test.TestNames.PrintToGoogleDriveCros);
});
GEN('#endif');

var PrintPreviewModelSettingsAvailabilityTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/model_settings_availability_test.js';
  }
};

TEST_F('PrintPreviewModelSettingsAvailabilityTest', 'All', function() {
  mocha.run();
});

GEN('#if BUILDFLAG(IS_CHROMEOS)');
var PrintPreviewModelSettingsPolicyTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/model_settings_policy_test.js';
  }
};

TEST_F('PrintPreviewModelSettingsPolicyTest', 'All', function() {
  mocha.run();
});
GEN('#endif');

var PrintPreviewPreviewGenerationTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/preview_generation_test.js';
  }

  /** @override */
  get suiteName() {
    return preview_generation_test.suiteName;
  }
};

TEST_F('PrintPreviewPreviewGenerationTest', 'Color', function() {
  this.runMochaTest(preview_generation_test.TestNames.Color);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'CssBackground', function() {
  this.runMochaTest(preview_generation_test.TestNames.CssBackground);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'HeaderFooter', function() {
  this.runMochaTest(preview_generation_test.TestNames.HeaderFooter);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'Layout', function() {
  this.runMochaTest(preview_generation_test.TestNames.Layout);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'Margins', function() {
  this.runMochaTest(preview_generation_test.TestNames.Margins);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'CustomMargins', function() {
  this.runMochaTest(preview_generation_test.TestNames.CustomMargins);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'MediaSize', function() {
  this.runMochaTest(preview_generation_test.TestNames.MediaSize);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'PageRange', function() {
  this.runMochaTest(preview_generation_test.TestNames.PageRange);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'SelectionOnly', function() {
  this.runMochaTest(preview_generation_test.TestNames.SelectionOnly);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'PagesPerSheet', function() {
  this.runMochaTest(preview_generation_test.TestNames.PagesPerSheet);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'Scaling', function() {
  this.runMochaTest(preview_generation_test.TestNames.Scaling);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'ScalingPdf', function() {
  this.runMochaTest(preview_generation_test.TestNames.ScalingPdf);
});

GEN('#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC)');
TEST_F('PrintPreviewPreviewGenerationTest', 'Rasterize', function() {
  this.runMochaTest(preview_generation_test.TestNames.Rasterize);
});
GEN('#endif');

TEST_F('PrintPreviewPreviewGenerationTest', 'Destination', function() {
  this.runMochaTest(preview_generation_test.TestNames.Destination);
});

TEST_F(
    'PrintPreviewPreviewGenerationTest', 'ChangeMarginsByPagesPerSheet',
    function() {
      this.runMochaTest(
          preview_generation_test.TestNames.ChangeMarginsByPagesPerSheet);
    });

TEST_F(
    'PrintPreviewPreviewGenerationTest', 'ZeroDefaultMarginsClearsHeaderFooter',
    function() {
      this.runMochaTest(preview_generation_test.TestNames
                            .ZeroDefaultMarginsClearsHeaderFooter);
    });

TEST_F('PrintPreviewPreviewGenerationTest', 'PageSizeCalculation', function() {
  this.runMochaTest(preview_generation_test.TestNames.PageSizeCalculation);
});

GEN('#if !BUILDFLAG(IS_CHROMEOS)');
var PrintPreviewLinkContainerTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/link_container_test.js';
  }

  /** @override */
  get suiteName() {
    return link_container_test.suiteName;
  }
};

TEST_F('PrintPreviewLinkContainerTest', 'HideInAppKioskMode', function() {
  this.runMochaTest(link_container_test.TestNames.HideInAppKioskMode);
});

TEST_F('PrintPreviewLinkContainerTest', 'SystemDialogLinkClick', function() {
  this.runMochaTest(link_container_test.TestNames.SystemDialogLinkClick);
});

TEST_F(
    'PrintPreviewLinkContainerTest', 'SystemDialogLinkProperties', function() {
      this.runMochaTest(
          link_container_test.TestNames.SystemDialogLinkProperties);
    });

TEST_F('PrintPreviewLinkContainerTest', 'InvalidState', function() {
  this.runMochaTest(link_container_test.TestNames.InvalidState);
});
GEN('#endif');  // !BUILDFLAG(IS_CHROMEOS)

GEN('#if BUILDFLAG(IS_MAC)');
TEST_F('PrintPreviewLinkContainerTest', 'OpenInPreviewLinkClick', function() {
  this.runMochaTest(link_container_test.TestNames.OpenInPreviewLinkClick);
});
GEN('#endif');  // BUILDFLAG(IS_MAC)

GEN('#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)');
var PrintPreviewSystemDialogBrowserTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/system_dialog_browsertest.js';
  }

  /** @override */
  get suiteName() {
    return system_dialog_browsertest.suiteName;
  }
};

TEST_F(
    'PrintPreviewSystemDialogBrowserTest', 'LinkTriggersLocalPrint',
    function() {
      this.runMochaTest(
          system_dialog_browsertest.TestNames.LinkTriggersLocalPrint);
    });

TEST_F(
    'PrintPreviewSystemDialogBrowserTest', 'InvalidSettingsDisableLink',
    function() {
      this.runMochaTest(
          system_dialog_browsertest.TestNames.InvalidSettingsDisableLink);
    });
GEN('#endif');  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

var PrintPreviewInvalidSettingsBrowserTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/invalid_settings_browsertest.js';
  }

  /** @override */
  get suiteName() {
    return invalid_settings_browsertest.suiteName;
  }
};

TEST_F(
    'PrintPreviewInvalidSettingsBrowserTest', 'InvalidSettingsError',
    function() {
      this.runMochaTest(
          invalid_settings_browsertest.TestNames.InvalidSettingsError);
    });

var PrintPreviewDestinationStoreTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_store_test.js';
  }

  /** @override */
  get suiteName() {
    return destination_store_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewDestinationStoreTest', 'SingleRecentDestination', function() {
      this.runMochaTest(
          destination_store_test.TestNames.SingleRecentDestination);
    });

TEST_F(
    'PrintPreviewDestinationStoreTest', 'RecentDestinationsFallback',
    function() {
      this.runMochaTest(
          destination_store_test.TestNames.RecentDestinationsFallback);
    });

TEST_F(
    'PrintPreviewDestinationStoreTest', 'MultipleRecentDestinations',
    function() {
      this.runMochaTest(
          destination_store_test.TestNames.MultipleRecentDestinations);
    });

TEST_F(
    'PrintPreviewDestinationStoreTest', 'MultipleRecentDestinationsOneRequest',
    function() {
      this.runMochaTest(destination_store_test.TestNames
                            .MultipleRecentDestinationsOneRequest);
    });

TEST_F(
    'PrintPreviewDestinationStoreTest', 'DefaultDestinationSelectionRules',
    function() {
      this.runMochaTest(
          destination_store_test.TestNames.DefaultDestinationSelectionRules);
    });

GEN('#if !BUILDFLAG(IS_CHROMEOS)');
TEST_F(
    'PrintPreviewDestinationStoreTest', 'SystemDefaultPrinterPolicy',
    function() {
      this.runMochaTest(
          destination_store_test.TestNames.SystemDefaultPrinterPolicy);
    });
GEN('#endif');

TEST_F(
    'PrintPreviewDestinationStoreTest', 'KioskModeSelectsFirstPrinter',
    function() {
      this.runMochaTest(
          destination_store_test.TestNames.KioskModeSelectsFirstPrinter);
    });

TEST_F(
    'PrintPreviewDestinationStoreTest', 'LoadAndSelectDestination', function() {
      this.runMochaTest(
          destination_store_test.TestNames.LoadAndSelectDestination);
    });

TEST_F('PrintPreviewDestinationStoreTest', 'NoPrintersShowsError', function() {
  this.runMochaTest(destination_store_test.TestNames.NoPrintersShowsError);
});

TEST_F('PrintPreviewDestinationStoreTest', 'RecentSaveAsPdf', function() {
  this.runMochaTest(destination_store_test.TestNames.RecentSaveAsPdf);
});

GEN('#if BUILDFLAG(IS_CHROMEOS)');
var PrintPreviewDestinationStoreTestCros = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_store_test.js';
  }

  /** @override */
  get suiteName() {
    return destination_store_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewDestinationStoreTestCros', 'LoadSaveToDriveCros', function() {
      this.runMochaTest(destination_store_test.TestNames.LoadSaveToDriveCros);
    });

TEST_F(
    'PrintPreviewDestinationStoreTestCros', 'SaveToDriveDisabled', function() {
      this.runMochaTest(destination_store_test.TestNames.SaveToDriveDisabled);
    });

var PrintPreviewPrinterSetupInfoCrosTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/' +
        'printer_setup_info_cros_test.js';
  }

  /** @override */
  get suiteName() {
    return printer_setup_info_cros_test.suiteName;
  }
};

TEST_F('PrintPreviewPrinterSetupInfoCrosTest', 'ElementDisplays', function() {
  this.runMochaTest(printer_setup_info_cros_test.TestNames.ElementDisplays);
});

TEST_F('PrintPreviewPrinterSetupInfoCrosTest', 'ButtonLocalized', function() {
  this.runMochaTest(printer_setup_info_cros_test.TestNames.ButtonLocalized);
});

TEST_F(
    'PrintPreviewPrinterSetupInfoCrosTest', 'ManagePrintersButton', function() {
      this.runMochaTest(
          printer_setup_info_cros_test.TestNames.ManagePrintersButton);
    });

TEST_F(
    'PrintPreviewPrinterSetupInfoCrosTest', 'MessageMatchesMessageType',
    function() {
      this.runMochaTest(
          printer_setup_info_cros_test.TestNames.MessageMatchesMessageType);
    });

TEST_F(
    'PrintPreviewPrinterSetupInfoCrosTest', 'ManagePrintersButtonMetrics',
    function() {
      this.runMochaTest(
          printer_setup_info_cros_test.TestNames.ManagePrintersButtonMetrics);
    });
GEN('#endif')

GEN('#if BUILDFLAG(IS_CHROMEOS)');
var PrintPreviewPrintServerStoreTestCros = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/print_server_store_test.js';
  }

  /** @override */
  get suiteName() {
    return print_server_store_test.suiteName;
  }
};
TEST_F(
    'PrintPreviewPrintServerStoreTestCros', 'ChoosePrintServers', function() {
      this.runMochaTest(print_server_store_test.TestNames.ChoosePrintServers);
    });

TEST_F(
    'PrintPreviewPrintServerStoreTestCros', 'PrintServersChanged', function() {
      this.runMochaTest(print_server_store_test.TestNames.PrintServersChanged);
    });

TEST_F(
    'PrintPreviewPrintServerStoreTestCros', 'GetPrintServersConfig',
    function() {
      this.runMochaTest(
          print_server_store_test.TestNames.GetPrintServersConfig);
    });

TEST_F(
    'PrintPreviewPrintServerStoreTestCros', 'ServerPrintersLoading',
    function() {
      this.runMochaTest(
          print_server_store_test.TestNames.ServerPrintersLoading);
    });
GEN('#endif');

var PrintPreviewDestinationDialogTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_dialog_test.js';
  }

  /** @override */
  get suiteName() {
    return destination_dialog_test.suiteName;
  }
};

TEST_F('PrintPreviewDestinationDialogTest', 'PrinterList', function() {
  this.runMochaTest(destination_dialog_test.TestNames.PrinterList);
});

TEST_F('PrintPreviewDestinationDialogTest', 'PrinterListPreloaded', function() {
  this.runMochaTest(destination_dialog_test.TestNames.PrinterListPreloaded);
});

GEN('#if BUILDFLAG(IS_CHROMEOS)');
var PrintPreviewDestinationDialogCrosTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_dialog_cros_test.js';
  }

  /** @override */
  get suiteName() {
    return destination_dialog_cros_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewDestinationDialogCrosTest', 'ShowProvisionalDialog',
    function() {
      this.runMochaTest(
          destination_dialog_cros_test.TestNames.ShowProvisionalDialog);
    });

TEST_F(
    'PrintPreviewDestinationDialogCrosTest', 'PrintServersChanged', function() {
      this.runMochaTest(
          destination_dialog_cros_test.TestNames.PrintServersChanged);
    });

TEST_F(
    'PrintPreviewDestinationDialogCrosTest', 'PrintServerSelected', function() {
      this.runMochaTest(
          destination_dialog_cros_test.TestNames.PrintServerSelected);
    });

TEST_F(
    'PrintPreviewDestinationDialogCrosTest',
    'PrinterSetupAssistanceHasDestinations', function() {
      this.runMochaTest(destination_dialog_cros_test.TestNames
                            .PrinterSetupAssistanceHasDestinations);
    });

TEST_F(
    'PrintPreviewDestinationDialogCrosTest',
    'PrinterSetupAssistanceHasNoDestinations', function() {
      this.runMochaTest(destination_dialog_cros_test.TestNames
                            .PrinterSetupAssistanceHasNoDestinations);
    });

TEST_F(
    'PrintPreviewDestinationDialogCrosTest',
    'ManagePrintersMetrics_HasDestinations', function() {
      this.runMochaTest(destination_dialog_cros_test.TestNames
                            .ManagePrintersMetrics_HasDestinations);
    });

TEST_F(
    'PrintPreviewDestinationDialogCrosTest',
    'ManagePrintersMetrics_HasNoDestinations', function() {
      this.runMochaTest(destination_dialog_cros_test.TestNames
                            .ManagePrintersMetrics_HasNoDestinations);
    });

GEN('#endif');

var PrintPreviewAdvancedDialogTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/advanced_dialog_test.js';
  }

  /** @override */
  get suiteName() {
    return advanced_dialog_test.suiteName;
  }
};

TEST_F('PrintPreviewAdvancedDialogTest', 'AdvancedSettings1Option', function() {
  this.runMochaTest(advanced_dialog_test.TestNames.AdvancedSettings1Option);
});

TEST_F(
    'PrintPreviewAdvancedDialogTest', 'AdvancedSettings2Options', function() {
      this.runMochaTest(
          advanced_dialog_test.TestNames.AdvancedSettings2Options);
    });

TEST_F('PrintPreviewAdvancedDialogTest', 'AdvancedSettingsApply', function() {
  this.runMochaTest(advanced_dialog_test.TestNames.AdvancedSettingsApply);
});

TEST_F(
    'PrintPreviewAdvancedDialogTest', 'AdvancedSettingsApplyWithEnter',
    function() {
      this.runMochaTest(
          advanced_dialog_test.TestNames.AdvancedSettingsApplyWithEnter);
    });

TEST_F('PrintPreviewAdvancedDialogTest', 'AdvancedSettingsClose', function() {
  this.runMochaTest(advanced_dialog_test.TestNames.AdvancedSettingsClose);
});

TEST_F('PrintPreviewAdvancedDialogTest', 'AdvancedSettingsFilter', function() {
  this.runMochaTest(advanced_dialog_test.TestNames.AdvancedSettingsFilter);
});

var PrintPreviewPreviewAreaTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/preview_area_test.js';
  }

  /** @override */
  get suiteName() {
    return preview_area_test.suiteName;
  }
};

TEST_F('PrintPreviewPreviewAreaTest', 'StateChanges', function() {
  this.runMochaTest(preview_area_test.TestNames.StateChanges);
});

GEN('#if BUILDFLAG(IS_CHROMEOS)');
TEST_F(
    'PrintPreviewPreviewAreaTest', 'StateChangesPrinterSetupCros', function() {
      this.runMochaTest(
          preview_area_test.TestNames.StateChangesPrinterSetupCros);
    });

TEST_F('PrintPreviewPreviewAreaTest', 'ManagePrinterMetricsCros', function() {
  this.runMochaTest(preview_area_test.TestNames.ManagePrinterMetricsCros);
});
GEN('#endif');

TEST_F('PrintPreviewPreviewAreaTest', 'ViewportSizeChanges', function() {
  this.runMochaTest(preview_area_test.TestNames.ViewportSizeChanges);
});

var PrintPreviewCustomMarginsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/custom_margins_test.js';
  }

  /** @override */
  get suiteName() {
    return custom_margins_test.suiteName;
  }
};

TEST_F('PrintPreviewCustomMarginsTest', 'ControlsCheck', function() {
  this.runMochaTest(custom_margins_test.TestNames.ControlsCheck);
});

TEST_F('PrintPreviewCustomMarginsTest', 'SetFromStickySettings', function() {
  this.runMochaTest(custom_margins_test.TestNames.SetFromStickySettings);
});

TEST_F('PrintPreviewCustomMarginsTest', 'DragControls', function() {
  this.runMochaTest(custom_margins_test.TestNames.DragControls);
});

TEST_F('PrintPreviewCustomMarginsTest', 'SetControlsWithTextbox', function() {
  this.runMochaTest(custom_margins_test.TestNames.SetControlsWithTextbox);
});

TEST_F(
    'PrintPreviewCustomMarginsTest', 'SetControlsWithTextboxMetric',
    function() {
      this.runMochaTest(
          custom_margins_test.TestNames.SetControlsWithTextboxMetric);
    });

TEST_F(
    'PrintPreviewCustomMarginsTest', 'RestoreStickyMarginsAfterDefault',
    function() {
      this.runMochaTest(
          custom_margins_test.TestNames.RestoreStickyMarginsAfterDefault);
    });

TEST_F(
    'PrintPreviewCustomMarginsTest', 'MediaSizeClearsCustomMargins',
    function() {
      this.runMochaTest(
          custom_margins_test.TestNames.MediaSizeClearsCustomMargins);
    });

TEST_F(
    'PrintPreviewCustomMarginsTest', 'LayoutClearsCustomMargins', function() {
      this.runMochaTest(
          custom_margins_test.TestNames.LayoutClearsCustomMargins);
    });

TEST_F(
    'PrintPreviewCustomMarginsTest', 'IgnoreDocumentMarginsFromPDF',
    function() {
      this.runMochaTest(
          custom_margins_test.TestNames.IgnoreDocumentMarginsFromPDF);
    });

TEST_F(
    'PrintPreviewCustomMarginsTest', 'MediaSizeClearsCustomMarginsPDF',
    function() {
      this.runMochaTest(
          custom_margins_test.TestNames.MediaSizeClearsCustomMarginsPDF);
    });

TEST_F(
    'PrintPreviewCustomMarginsTest', 'RequestScrollToOutOfBoundsTextbox',
    function() {
      this.runMochaTest(
          custom_margins_test.TestNames.RequestScrollToOutOfBoundsTextbox);
    });

TEST_F('PrintPreviewCustomMarginsTest', 'ControlsDisabledOnError', function() {
  this.runMochaTest(custom_margins_test.TestNames.ControlsDisabledOnError);
});

GEN('#if BUILDFLAG(IS_CHROMEOS)');
var PrintPreviewDestinationSearchTestChromeOS = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_search_test_chromeos.js';
  }

  /** @override */
  get suiteName() {
    return destination_search_test_chromeos.suiteName;
  }
};

TEST_F(
    'PrintPreviewDestinationSearchTestChromeOS', 'ReceiveSuccessfulSetup',
    function() {
      this.runMochaTest(
          destination_search_test_chromeos.TestNames.ReceiveSuccessfulSetup);
    });

TEST_F(
    'PrintPreviewDestinationSearchTestChromeOS', 'ResolutionFails', function() {
      this.runMochaTest(
          destination_search_test_chromeos.TestNames.ResolutionFails);
    });

GEN('#else');
var PrintPreviewDestinationSearchTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_search_test.js';
  }

  /** @override */
  get suiteName() {
    return destination_search_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewDestinationSearchTest', 'GetCapabilitiesSucceeds', function() {
      this.runMochaTest(
          destination_search_test.TestNames.GetCapabilitiesSucceeds);
    });

TEST_F('PrintPreviewDestinationSearchTest', 'GetCapabilitiesFails', function() {
  this.runMochaTest(destination_search_test.TestNames.GetCapabilitiesFails);
});
GEN('#endif');

var PrintPreviewHeaderTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/header_test.js';
  }

  /** @override */
  get suiteName() {
    return header_test.suiteName;
  }
};

TEST_F('PrintPreviewHeaderTest', 'HeaderPrinterTypes', function() {
  this.runMochaTest(header_test.TestNames.HeaderPrinterTypes);
});

TEST_F('PrintPreviewHeaderTest', 'HeaderChangesForState', function() {
  this.runMochaTest(header_test.TestNames.HeaderChangesForState);
});

TEST_F('PrintPreviewHeaderTest', 'EnterprisePolicy', function() {
  this.runMochaTest(header_test.TestNames.EnterprisePolicy);
});

var PrintPreviewButtonStripTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/button_strip_test.js';
  }

  /** @override */
  get suiteName() {
    return button_strip_test.suiteName;
  }
};

TEST_F('PrintPreviewButtonStripTest', 'ButtonStripChangesForState', function() {
  this.runMochaTest(button_strip_test.TestNames.ButtonStripChangesForState);
});

TEST_F('PrintPreviewButtonStripTest', 'ButtonOrder', function() {
  this.runMochaTest(button_strip_test.TestNames.ButtonOrder);
});

TEST_F('PrintPreviewButtonStripTest', 'ButtonStripFiresEvents', function() {
  this.runMochaTest(button_strip_test.TestNames.ButtonStripFiresEvents);
});

GEN('#if BUILDFLAG(IS_CHROMEOS)');
TEST_F('PrintPreviewButtonStripTest', 'InvalidPinDisablesPrint', function() {
  this.runMochaTest(button_strip_test.TestNames.InvalidPinDisablesPrint);
});
GEN('#endif');

var PrintPreviewDestinationItemTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_item_test.js';
  }

  /** @override */
  get suiteName() {
    return destination_item_test.suiteName;
  }
};

TEST_F('PrintPreviewDestinationItemTest', 'NoQuery', function() {
  this.runMochaTest(destination_item_test.TestNames.NoQuery);
});

TEST_F('PrintPreviewDestinationItemTest', 'QueryName', function() {
  this.runMochaTest(destination_item_test.TestNames.QueryName);
});

TEST_F('PrintPreviewDestinationItemTest', 'QueryDescription', function() {
  this.runMochaTest(destination_item_test.TestNames.QueryDescription);
});

GEN('#if BUILDFLAG(IS_CHROMEOS)');
var PrintPreviewDestinationItemTestCros = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_item_test_cros.js';
  }

  /** @override */
  get suiteName() {
    return destination_item_test_cros.suiteName;
  }
};

TEST_F(
    'PrintPreviewDestinationItemTestCros', 'NewStatusUpdatesIcon', function() {
      this.runMochaTest(
          destination_item_test_cros.TestNames.NewStatusUpdatesIcon);
    });

TEST_F(
    'PrintPreviewDestinationItemTestCros', 'ChangingDestinationUpdatesIcon',
    function() {
      this.runMochaTest(
          destination_item_test_cros.TestNames.ChangingDestinationUpdatesIcon);
    });

TEST_F(
    'PrintPreviewDestinationItemTestCros', 'OnlyUpdateMatchingDestination',
    function() {
      this.runMochaTest(
          destination_item_test_cros.TestNames.OnlyUpdateMatchingDestination);
    });

TEST_F(
    'PrintPreviewDestinationItemTestCros',
    'PrinterIconMapsToPrinterStatus_FlagOff', function() {
      this.runMochaTest(destination_item_test_cros.TestNames
                            .PrinterIconMapsToPrinterStatus_FlagOff);
    });

TEST_F(
    'PrintPreviewDestinationItemTestCros',
    'PrinterIconMapsToPrinterStatus_FlagOn', function() {
      this.runMochaTest(destination_item_test_cros.TestNames
                            .PrinterIconMapsToPrinterStatus_FlagOn);
    });

TEST_F(
    'PrintPreviewDestinationItemTestCros',
    'PrinterConnectionStatusClass_FlagOff', function() {
      this.runMochaTest(destination_item_test_cros.TestNames
                            .PrinterConnectionStatusClass_FlagOff);
    });

TEST_F(
    'PrintPreviewDestinationItemTestCros',
    'PrinterConnectionStatusClass_FlagOn', function() {
      this.runMochaTest(destination_item_test_cros.TestNames
                            .PrinterConnectionStatusClass_FlagOn);
    });
GEN('#endif');

var PrintPreviewAdvancedItemTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/advanced_item_test.js';
  }

  /** @override */
  get suiteName() {
    return advanced_item_test.suiteName;
  }
};

TEST_F('PrintPreviewAdvancedItemTest', 'DisplaySelect', function() {
  this.runMochaTest(advanced_item_test.TestNames.DisplaySelect);
});

TEST_F('PrintPreviewAdvancedItemTest', 'DisplayInput', function() {
  this.runMochaTest(advanced_item_test.TestNames.DisplayInput);
});

TEST_F('PrintPreviewAdvancedItemTest', 'UpdateSelect', function() {
  this.runMochaTest(advanced_item_test.TestNames.UpdateSelect);
});

TEST_F('PrintPreviewAdvancedItemTest', 'UpdateInput', function() {
  this.runMochaTest(advanced_item_test.TestNames.UpdateInput);
});

TEST_F('PrintPreviewAdvancedItemTest', 'QueryName', function() {
  this.runMochaTest(advanced_item_test.TestNames.QueryName);
});

TEST_F('PrintPreviewAdvancedItemTest', 'QueryOption', function() {
  this.runMochaTest(advanced_item_test.TestNames.QueryOption);
});

var PrintPreviewDestinationListTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_list_test.js';
  }

  /** @override */
  get suiteName() {
    return destination_list_test.suiteName;
  }
};

TEST_F('PrintPreviewDestinationListTest', 'FilterDestinations', function() {
  this.runMochaTest(destination_list_test.TestNames.FilterDestinations);
});

TEST_F(
    'PrintPreviewDestinationListTest', 'FireDestinationSelected', function() {
      this.runMochaTest(
          destination_list_test.TestNames.FireDestinationSelected);
    });

var PrintPreviewPrintButtonTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/print_button_test.js';
  }

  /** @override */
  get suiteName() {
    return print_button_test.suiteName;
  }
};

TEST_F('PrintPreviewPrintButtonTest', 'LocalPrintHidePreview', function() {
  this.runMochaTest(print_button_test.TestNames.LocalPrintHidePreview);
});

TEST_F('PrintPreviewPrintButtonTest', 'PDFPrintVisiblePreview', function() {
  this.runMochaTest(print_button_test.TestNames.PDFPrintVisiblePreview);
});

GEN('#if BUILDFLAG(IS_CHROMEOS)');
var PrintPreviewPrintButtonTestCros = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/print_button_test.js';
  }

  /** @override */
  get suiteName() {
    return print_button_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewPrintButtonTestCros', 'SaveToDriveVisiblePreviewCros',
    function() {
      this.runMochaTest(
          print_button_test.TestNames.SaveToDriveVisiblePreviewCros);
    });
GEN('#endif');

var PrintPreviewKeyEventTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/key_event_test.js';
  }

  /** @override */
  get suiteName() {
    return key_event_test.suiteName;
  }
};

TEST_F('PrintPreviewKeyEventTest', 'EnterTriggersPrint', function() {
  this.runMochaTest(key_event_test.TestNames.EnterTriggersPrint);
});

TEST_F('PrintPreviewKeyEventTest', 'NumpadEnterTriggersPrint', function() {
  this.runMochaTest(key_event_test.TestNames.NumpadEnterTriggersPrint);
});

TEST_F('PrintPreviewKeyEventTest', 'EnterOnInputTriggersPrint', function() {
  this.runMochaTest(key_event_test.TestNames.EnterOnInputTriggersPrint);
});

TEST_F('PrintPreviewKeyEventTest', 'EnterOnDropdownDoesNotPrint', function() {
  this.runMochaTest(key_event_test.TestNames.EnterOnDropdownDoesNotPrint);
});

TEST_F('PrintPreviewKeyEventTest', 'EnterOnButtonDoesNotPrint', function() {
  this.runMochaTest(key_event_test.TestNames.EnterOnButtonDoesNotPrint);
});

TEST_F('PrintPreviewKeyEventTest', 'EnterOnCheckboxDoesNotPrint', function() {
  this.runMochaTest(key_event_test.TestNames.EnterOnCheckboxDoesNotPrint);
});

TEST_F('PrintPreviewKeyEventTest', 'EscapeClosesDialogOnMacOnly', function() {
  this.runMochaTest(key_event_test.TestNames.EscapeClosesDialogOnMacOnly);
});

TEST_F(
    'PrintPreviewKeyEventTest', 'CmdPeriodClosesDialogOnMacOnly', function() {
      this.runMochaTest(
          key_event_test.TestNames.CmdPeriodClosesDialogOnMacOnly);
    });

TEST_F('PrintPreviewKeyEventTest', 'CtrlShiftPOpensSystemDialog', function() {
  this.runMochaTest(key_event_test.TestNames.CtrlShiftPOpensSystemDialog);
});

GEN('#if BUILDFLAG(IS_CHROMEOS)');
var PrintPreviewPrinterStatusTestCros = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_select_test_cros.js';
  }

  /** @override */
  get suiteName() {
    return printer_status_test_cros.suiteName;
  }
};

TEST_F(
    'PrintPreviewPrinterStatusTestCros', 'PrinterStatusUpdatesColor',
    function() {
      this.runMochaTest(
          printer_status_test_cros.TestNames.PrinterStatusUpdatesColor);
    });

TEST_F(
    'PrintPreviewPrinterStatusTestCros', 'SendStatusRequestOnce', function() {
      this.runMochaTest(
          printer_status_test_cros.TestNames.SendStatusRequestOnce);
    });

TEST_F('PrintPreviewPrinterStatusTestCros', 'HiddenStatusText', function() {
  this.runMochaTest(printer_status_test_cros.TestNames.HiddenStatusText);
});

TEST_F('PrintPreviewPrinterStatusTestCros', 'ChangeIcon', function() {
  this.runMochaTest(printer_status_test_cros.TestNames.ChangeIcon);
});

TEST_F(
    'PrintPreviewPrinterStatusTestCros', 'SuccessfulPrinterStatusAfterRetry',
    function() {
      this.runMochaTest(
          printer_status_test_cros.TestNames.SuccessfulPrinterStatusAfterRetry);
    });

TEST_F(
    'PrintPreviewPrinterStatusTestCros', 'StatusTextClass_FlagOff', function() {
      this.runMochaTest(
          printer_status_test_cros.TestNames.StatusTextClass_FlagOff);
    });

TEST_F(
    'PrintPreviewPrinterStatusTestCros', 'StatusTextClass_FlagOn', function() {
      this.runMochaTest(
          printer_status_test_cros.TestNames.StatusTextClass_FlagOn);
    });

var PrintPreviewDestinationDropdownCrosTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_dropdown_cros_test.js';
  }

  /** @override */
  get suiteName() {
    return destination_dropdown_cros_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewDestinationDropdownCrosTest', 'CorrectListItems', function() {
      this.runMochaTest(
          destination_dropdown_cros_test.TestNames.CorrectListItems);
    });

TEST_F(
    'PrintPreviewDestinationDropdownCrosTest', 'HighlightedAfterUpDown',
    function() {
      this.runMochaTest(
          destination_dropdown_cros_test.TestNames.HighlightedAfterUpDown);
    });

TEST_F(
    'PrintPreviewDestinationDropdownCrosTest', 'DestinationChangeAfterUpDown',
    function() {
      this.runMochaTest(destination_dropdown_cros_test.TestNames
                            .DestinationChangeAfterUpDown);
    });

TEST_F(
    'PrintPreviewDestinationDropdownCrosTest', 'EnterOpensCloses', function() {
      this.runMochaTest(
          destination_dropdown_cros_test.TestNames.EnterOpensCloses);
    });

TEST_F(
    'PrintPreviewDestinationDropdownCrosTest', 'HighlightedFollowsMouse',
    function() {
      this.runMochaTest(
          destination_dropdown_cros_test.TestNames.HighlightedFollowsMouse);
    });

TEST_F('PrintPreviewDestinationDropdownCrosTest', 'Disabled', function() {
  this.runMochaTest(destination_dropdown_cros_test.TestNames.Disabled);
});

TEST_F(
    'PrintPreviewDestinationDropdownCrosTest', 'HighlightedWhenOpened',
    function() {
      this.runMochaTest(
          destination_dropdown_cros_test.TestNames.HighlightedWhenOpened);
    });

GEN('#else');
var PrintPreviewDestinationSelectTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_select_test.js';
  }

  /** @override */
  get suiteName() {
    return destination_select_test.suiteName;
  }
};

TEST_F('PrintPreviewDestinationSelectTest', 'ChangeIcon', function() {
  this.runMochaTest(destination_select_test.TestNames.ChangeIcon);
});
GEN('#endif');

var PrintPreviewDestinationSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_settings_test.js';
  }

  /** @override */
  get suiteName() {
    return destination_settings_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewDestinationSettingsTest', 'ChangeDropdownState', function() {
      this.runMochaTest(
          destination_settings_test.TestNames.ChangeDropdownState);
    });

TEST_F(
    'PrintPreviewDestinationSettingsTest', 'NoRecentDestinations', function() {
      this.runMochaTest(
          destination_settings_test.TestNames.NoRecentDestinations);
    });

TEST_F('PrintPreviewDestinationSettingsTest', 'RecentDestinations', function() {
  this.runMochaTest(destination_settings_test.TestNames.RecentDestinations);
});

// Flaky on Mac and Linux, see https://crbug.com/1147205
GEN('#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)');
GEN('#define MAYBE_RecentDestinationsMissing DISABLED_RecentDestinationsMissing');
GEN('#else');
GEN('#define MAYBE_RecentDestinationsMissing RecentDestinationsMissing');
GEN('#endif');
TEST_F(
    'PrintPreviewDestinationSettingsTest', 'MAYBE_RecentDestinationsMissing',
    function() {
      this.runMochaTest(
          destination_settings_test.TestNames.RecentDestinationsMissing);
    });

TEST_F('PrintPreviewDestinationSettingsTest', 'SaveAsPdfRecent', function() {
  this.runMochaTest(destination_settings_test.TestNames.SaveAsPdfRecent);
});

GEN('#if BUILDFLAG(IS_CHROMEOS)');
TEST_F('PrintPreviewDestinationSettingsTest', 'GoogleDriveRecent', function() {
  this.runMochaTest(destination_settings_test.TestNames.GoogleDriveRecent);
});

TEST_F(
    'PrintPreviewDestinationSettingsTest', 'GoogleDriveAutoselect', function() {
      this.runMochaTest(
          destination_settings_test.TestNames.GoogleDriveAutoselect);
    });
GEN('#endif');

TEST_F('PrintPreviewDestinationSettingsTest', 'SelectSaveAsPdf', function() {
  this.runMochaTest(destination_settings_test.TestNames.SelectSaveAsPdf);
});

GEN('#if BUILDFLAG(IS_CHROMEOS)');
TEST_F('PrintPreviewDestinationSettingsTest', 'SelectGoogleDrive', function() {
  this.runMochaTest(destination_settings_test.TestNames.SelectGoogleDrive);
});
GEN('#endif');

TEST_F(
    'PrintPreviewDestinationSettingsTest', 'SelectRecentDestination',
    function() {
      this.runMochaTest(
          destination_settings_test.TestNames.SelectRecentDestination);
    });

TEST_F('PrintPreviewDestinationSettingsTest', 'OpenDialog', function() {
  this.runMochaTest(destination_settings_test.TestNames.OpenDialog);
});

TEST_F(
    'PrintPreviewDestinationSettingsTest', 'UpdateRecentDestinations',
    function() {
      this.runMochaTest(
          destination_settings_test.TestNames.UpdateRecentDestinations);
    });

TEST_F('PrintPreviewDestinationSettingsTest', 'DisabledSaveAsPdf', function() {
  this.runMochaTest(destination_settings_test.TestNames.DisabledSaveAsPdf);
});

// Flaky on Mac, see https://crbug.com/1146513.
GEN('#if BUILDFLAG(IS_MAC)');
GEN('#define MAYBE_NoDestinations DISABLED_NoDestinations');
GEN('#else');
GEN('#define MAYBE_NoDestinations NoDestinations');
GEN('#endif');
TEST_F(
    'PrintPreviewDestinationSettingsTest', 'MAYBE_NoDestinations', function() {
      this.runMochaTest(destination_settings_test.TestNames.NoDestinations);
    });

GEN('#if BUILDFLAG(IS_CHROMEOS)');
TEST_F('PrintPreviewDestinationSettingsTest', 'EulaIsRetrieved', function() {
  this.runMochaTest(destination_settings_test.TestNames.EulaIsRetrieved);
});

TEST_F(
    'PrintPreviewDestinationSettingsTest', 'SaveToDriveDisabled', function() {
      this.runMochaTest(
          destination_settings_test.TestNames.SaveToDriveDisabled);
    });
GEN('#endif');

var PrintPreviewScalingSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/scaling_settings_test.js';
  }

  /** @override */
  get suiteName() {
    return scaling_settings_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewScalingSettingsTest', 'ShowCorrectDropdownOptions',
    function() {
      this.runMochaTest(
          scaling_settings_test.TestNames.ShowCorrectDropdownOptions);
    });

TEST_F('PrintPreviewScalingSettingsTest', 'SetScaling', function() {
  this.runMochaTest(scaling_settings_test.TestNames.SetScaling);
});

TEST_F(
    'PrintPreviewScalingSettingsTest', 'InputNotDisabledOnValidityChange',
    function() {
      this.runMochaTest(
          scaling_settings_test.TestNames.InputNotDisabledOnValidityChange);
    });

var PrintPreviewCopiesSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/copies_settings_test.js';
  }
};

TEST_F('PrintPreviewCopiesSettingsTest', 'All', function() {
  mocha.run();
});

var PrintPreviewMediaSizeSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/media_size_settings_test.js';
  }
};

TEST_F('PrintPreviewMediaSizeSettingsTest', 'All', function() {
  mocha.run();
});

var PrintPreviewDpiSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/dpi_settings_test.js';
  }
};

TEST_F('PrintPreviewDpiSettingsTest', 'All', function() {
  mocha.run();
});

var PrintPreviewOtherOptionsSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/other_options_settings_test.js';
  }
};

TEST_F('PrintPreviewOtherOptionsSettingsTest', 'All', function() {
  mocha.run();
});

var PrintPreviewLayoutSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/layout_settings_test.js';
  }
};

TEST_F('PrintPreviewLayoutSettingsTest', 'All', function() {
  mocha.run();
});

var PrintPreviewColorSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/color_settings_test.js';
  }
};

TEST_F('PrintPreviewColorSettingsTest', 'All', function() {
  mocha.run();
});

var PrintPreviewMarginsSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/margins_settings_test.js';
  }
};

TEST_F('PrintPreviewMarginsSettingsTest', 'All', function() {
  mocha.run();
});

var PrintPreviewPagesPerSheetSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/pages_per_sheet_settings_test.js';
  }
};

TEST_F('PrintPreviewPagesPerSheetSettingsTest', 'All', function() {
  mocha.run();
});

var PrintPreviewDuplexSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/duplex_settings_test.js';
  }
};

TEST_F('PrintPreviewDuplexSettingsTest', 'All', function() {
  mocha.run();
});

GEN('#if BUILDFLAG(IS_CHROMEOS)');
var PrintPreviewPinSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/pin_settings_test.js';
  }
};

TEST_F('PrintPreviewPinSettingsTest', 'All', function() {
  mocha.run();
});
GEN('#endif');

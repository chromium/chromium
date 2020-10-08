// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Print Preview tests. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const PrintPreviewTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
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

// eslint-disable-next-line no-var
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

TEST_F('PrintPreviewAppTest', 'PrintToGoogleDrive', function() {
  this.runMochaTest(print_preview_app_test.TestNames.PrintToGoogleDrive);
});

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

GEN('#if defined(OS_CHROMEOS)');
TEST_F('PrintPreviewAppTest', 'SheetsManaged', function() {
  this.runMochaTest(print_preview_app_test.TestNames.SheetsManaged);
});
GEN('#endif');

// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
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

TEST_F('PrintPreviewPagesSettingsTest', 'ValidPageRanges', function() {
  this.runMochaTest(pages_settings_test.TestNames.ValidPageRanges);
});

TEST_F('PrintPreviewPagesSettingsTest', 'InvalidPageRanges', function() {
  this.runMochaTest(pages_settings_test.TestNames.InvalidPageRanges);
});

TEST_F('PrintPreviewPagesSettingsTest', 'NupChangesPages', function() {
  this.runMochaTest(pages_settings_test.TestNames.NupChangesPages);
});

// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
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

GEN('#if defined(OS_CHROMEOS)');
TEST_F('PrintPreviewPolicyTest', 'SheetsPolicy', function() {
  this.runMochaTest(policy_tests.TestNames.SheetsPolicy);
});
GEN('#endif');

// eslint-disable-next-line no-var
var PrintPreviewSettingsSelectTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/settings_select_test.js';
  }
};

TEST_F('PrintPreviewSettingsSelectTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewSelectBehaviorTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/select_behavior_test.js';
  }

  /** @override */
  get suiteName() {
    return select_behavior_test.suiteName;
  }
};

TEST_F('PrintPreviewSelectBehaviorTest', 'CallProcessSelectChange', function() {
  this.runMochaTest(select_behavior_test.TestNames.CallProcessSelectChange);
});

// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
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

GEN('#if defined(OS_CHROMEOS)');
// eslint-disable-next-line no-var
var PrintPreviewModelTestCros = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/model_test.js';
  }

  /** @override */
  get suiteName() {
    return model_test.suiteName;
  }

  /** @override */
  get featureList() {
    const kPrintSaveToDrive = ['chromeos::features::kPrintSaveToDrive'];
    const featureList = super.featureList || [];
    featureList.enabled = featureList.enabled ?
        featureList.enabled.concat(kPrintSaveToDrive) :
        kPrintSaveToDrive;
    return featureList;
  }
};

TEST_F('PrintPreviewModelTestCros', 'PrintToGoogleDriveCros', function() {
  this.runMochaTest(model_test.TestNames.PrintToGoogleDriveCros);
});
GEN('#endif');

// eslint-disable-next-line no-var
var PrintPreviewModelSettingsAvailabilityTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/model_settings_availability_test.js';
  }
};

TEST_F('PrintPreviewModelSettingsAvailabilityTest', 'All', function() {
  mocha.run();
});

GEN('#if defined(OS_CHROMEOS)');
// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
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

GEN('#if !defined(OS_WIN) && !defined(OS_MAC)');
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

GEN('#if !defined(OS_CHROMEOS)');
// eslint-disable-next-line no-var
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

TEST_F('PrintPreviewLinkContainerTest', 'InvalidState', function() {
  this.runMochaTest(link_container_test.TestNames.InvalidState);
});
GEN('#endif');  // !defined(OS_CHROMEOS)

GEN('#if defined(OS_MAC)');
TEST_F('PrintPreviewLinkContainerTest', 'OpenInPreviewLinkClick', function() {
  this.runMochaTest(link_container_test.TestNames.OpenInPreviewLinkClick);
});
GEN('#endif');  // defined(OS_MAC)

GEN('#if defined(OS_WIN) || defined(OS_MAC)');
// eslint-disable-next-line no-var
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
GEN('#endif');  // defined(OS_WIN) || defined(OS_MAC)

// eslint-disable-next-line no-var
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
    'PrintPreviewInvalidSettingsBrowserTest', 'NoPDFPluginError', function() {
      this.runMochaTest(
          invalid_settings_browsertest.TestNames.NoPDFPluginError);
    });

TEST_F(
    'PrintPreviewInvalidSettingsBrowserTest', 'InvalidSettingsError',
    function() {
      this.runMochaTest(
          invalid_settings_browsertest.TestNames.InvalidSettingsError);
    });

TEST_F(
    'PrintPreviewInvalidSettingsBrowserTest', 'InvalidCertificateError',
    function() {
      this.runMochaTest(
          invalid_settings_browsertest.TestNames.InvalidCertificateError);
    });

TEST_F(
    'PrintPreviewInvalidSettingsBrowserTest',
    'InvalidCertificateErrorReselectDestination', function() {
      this.runMochaTest(invalid_settings_browsertest.TestNames
                            .InvalidCertificateErrorReselectDestination);
    });

// eslint-disable-next-line no-var
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

GEN('#if !defined(OS_CHROMEOS)');
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

TEST_F(
    'PrintPreviewDestinationStoreTest', 'UnreachableRecentCloudPrinter',
    function() {
      this.runMochaTest(
          destination_store_test.TestNames.UnreachableRecentCloudPrinter);
    });

TEST_F('PrintPreviewDestinationStoreTest', 'RecentSaveAsPdf', function() {
  this.runMochaTest(destination_store_test.TestNames.RecentSaveAsPdf);
});

TEST_F(
    'PrintPreviewDestinationStoreTest', 'MultipleRecentDestinationsAccounts',
    function() {
      this.runMochaTest(
          destination_store_test.TestNames.MultipleRecentDestinationsAccounts);
    });

GEN('#if defined(OS_CHROMEOS)');
// eslint-disable-next-line no-var
var PrintPreviewDestinationStoreTestCros = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_store_test.js';
  }

  /** @override */
  get suiteName() {
    return destination_store_test.suiteName;
  }

  /** @override */
  get featureList() {
    const kPrintSaveToDrive = ['chromeos::features::kPrintSaveToDrive'];
    const featureList = super.featureList || [];
    featureList.enabled = featureList.enabled ?
        featureList.enabled.concat(kPrintSaveToDrive) :
        kPrintSaveToDrive;
    return featureList;
  }
};

TEST_F(
    'PrintPreviewDestinationStoreTestCros',
    'MultipleRecentDestinationsAccountsCros', function() {
      this.runMochaTest(destination_store_test.TestNames
                            .MultipleRecentDestinationsAccountsCros);
    });

TEST_F(
    'PrintPreviewDestinationStoreTestCros', 'LoadSaveToDriveCros', function() {
      this.runMochaTest(destination_store_test.TestNames.LoadSaveToDriveCros);
    });

TEST_F('PrintPreviewDestinationStoreTestCros', 'DriveNotMounted', function() {
  this.runMochaTest(destination_store_test.TestNames.DriveNotMounted);
});
GEN('#endif');

// eslint-disable-next-line no-var
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

GEN('#if defined(OS_CHROMEOS)');
TEST_F(
    'PrintPreviewDestinationDialogTest', 'ShowProvisionalDialog', function() {
      this.runMochaTest(
          destination_dialog_test.TestNames.ShowProvisionalDialog);
    });
GEN('#endif');

TEST_F('PrintPreviewDestinationDialogTest', 'UserAccounts', function() {
  this.runMochaTest(destination_dialog_test.TestNames.UserAccounts);
});

TEST_F(
    'PrintPreviewDestinationDialogTest', 'CloudPrinterDeprecationWarnings',
    function() {
      this.runMochaTest(
          destination_dialog_test.TestNames.CloudPrinterDeprecationWarnings);
    });

TEST_F(
    'PrintPreviewDestinationDialogTest',
    'CloudPrinterDeprecationWarningsSuppressed', function() {
      this.runMochaTest(destination_dialog_test.TestNames
                            .CloudPrinterDeprecationWarningsSuppressed);
    });

// TODO(crbug.com/1111985): Different tests are needed because |isChromeOS| from
// cr.m.js does not match the behavior of the |OS_CHROMEOS| macro on Lacros.
GEN('#if defined(OS_CHROMEOS)');
TEST_F(
    'PrintPreviewDestinationDialogTest', 'SaveToDriveDeprecationWarningsCros',
    function() {
      this.runMochaTest(
          destination_dialog_test.TestNames.SaveToDriveDeprecationWarningsCros);
    });

TEST_F(
    'PrintPreviewDestinationDialogTest',
    'SaveToDriveDeprecationWarningsSuppressedCros', function() {
      this.runMochaTest(destination_dialog_test.TestNames
                            .SaveToDriveDeprecationWarningsSuppressedCros);
    });
GEN('#else');
TEST_F(
    'PrintPreviewDestinationDialogTest', 'SaveToDriveDeprecationWarnings',
    function() {
      this.runMochaTest(
          destination_dialog_test.TestNames.SaveToDriveDeprecationWarnings);
    });

TEST_F(
    'PrintPreviewDestinationDialogTest',
    'SaveToDriveDeprecationWarningsSuppressed', function() {
      this.runMochaTest(destination_dialog_test.TestNames
                            .SaveToDriveDeprecationWarningsSuppressed);
    });
GEN('#endif');

// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
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

TEST_F('PrintPreviewPreviewAreaTest', 'ViewportSizeChanges', function() {
  this.runMochaTest(preview_area_test.TestNames.ViewportSizeChanges);
});

// eslint-disable-next-line no-var
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

GEN('#if defined(OS_CHROMEOS)');
// eslint-disable-next-line no-var
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

TEST_F(
    'PrintPreviewDestinationSearchTestChromeOS', 'ReceiveFailedSetup',
    function() {
      this.runMochaTest(
          destination_search_test_chromeos.TestNames.ReceiveFailedSetup);
    });

TEST_F(
    'PrintPreviewDestinationSearchTestChromeOS',
    'ReceiveSuccessfultSetupWithPolicies', function() {
      this.runMochaTest(
          destination_search_test_chromeos.TestNames.ResolutionFails);
    });

TEST_F(
    'PrintPreviewDestinationSearchTestChromeOS', 'CloudKioskPrinter',
    function() {
      this.runMochaTest(
          destination_search_test_chromeos.TestNames.CloudKioskPrinter);
    });

GEN('#else');
// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
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

TEST_F('PrintPreviewDestinationItemTest', 'Online', function() {
  this.runMochaTest(destination_item_test.TestNames.Online);
});

TEST_F('PrintPreviewDestinationItemTest', 'Offline', function() {
  this.runMochaTest(destination_item_test.TestNames.Offline);
});

TEST_F('PrintPreviewDestinationItemTest', 'BadCertificate', function() {
  this.runMochaTest(destination_item_test.TestNames.BadCertificate);
});

TEST_F('PrintPreviewDestinationItemTest', 'QueryName', function() {
  this.runMochaTest(destination_item_test.TestNames.QueryName);
});

TEST_F('PrintPreviewDestinationItemTest', 'QueryDescription', function() {
  this.runMochaTest(destination_item_test.TestNames.QueryDescription);
});

GEN('#if defined(OS_CHROMEOS)');
// eslint-disable-next-line no-var
var PrintPreviewDestinationItemTestCros = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_item_test_cros.js';
  }

  /** @override */
  get suiteName() {
    return destination_item_test_cros.suiteName;
  }

  /** @override */
  get featureList() {
    const kPrinterStatusDialog = ['chromeos::features::kPrinterStatusDialog'];
    const featureList = super.featureList || [];
    featureList.enabled = featureList.enabled ?
        featureList.enabled.concat(kPrinterStatusDialog) :
        kPrinterStatusDialog;
    return featureList;
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
GEN('#endif');

// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
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

GEN('#if defined(OS_CHROMEOS)');
// eslint-disable-next-line no-var
var PrintPreviewPrintButtonTestCros = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/print_button_test.js';
  }

  /** @override */
  get suiteName() {
    return print_button_test.suiteName;
  }

  /** @override */
  get featureList() {
    const kPrintSaveToDrive = ['chromeos::features::kPrintSaveToDrive'];
    const featureList = super.featureList || [];
    featureList.enabled = featureList.enabled ?
        featureList.enabled.concat(kPrintSaveToDrive) :
        kPrintSaveToDrive;
    return featureList;
  }
};

TEST_F(
    'PrintPreviewPrintButtonTestCros', 'SaveToDriveVisiblePreviewCros',
    function() {
      this.runMochaTest(
          print_button_test.TestNames.SaveToDriveVisiblePreviewCros);
    });
GEN('#endif');

// eslint-disable-next-line no-var
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

GEN('#if defined(OS_CHROMEOS)');
// eslint-disable-next-line no-var
var PrintPreviewDestinationSelectTestCrOS = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_select_test_cros.js';
  }

  /** @override */
  get suiteName() {
    return destination_select_test_cros.suiteName;
  }

  /** @override */
  get featureList() {
    const kPrinterStatus = ['chromeos::features::kPrinterStatus'];
    const featureList = super.featureList || [];
    featureList.disabled = featureList.disabled ?
        featureList.disabled.concat(kPrinterStatus) :
        kPrinterStatus;
    return featureList;
  }
};

TEST_F('PrintPreviewDestinationSelectTestCrOS', 'UpdateStatus', function() {
  this.runMochaTest(destination_select_test_cros.TestNames.UpdateStatus);
});

TEST_F(
    'PrintPreviewDestinationSelectTestCrOS', 'UpdateStatusDeprecationWarnings',
    function() {
      this.runMochaTest(destination_select_test_cros.TestNames
                            .UpdateStatusDeprecationWarnings);
    });

TEST_F('PrintPreviewDestinationSelectTestCrOS', 'ChangeIcon', function() {
  this.runMochaTest(destination_select_test_cros.TestNames.ChangeIcon);
});

TEST_F(
    'PrintPreviewDestinationSelectTestCrOS', 'ChangeIconDeprecationWarnings',
    function() {
      this.runMochaTest(
          destination_select_test_cros.TestNames.ChangeIconDeprecationWarnings);
    });

TEST_F('PrintPreviewDestinationSelectTestCrOS', 'EulaIsDisplayed', function() {
  this.runMochaTest(destination_select_test_cros.TestNames.EulaIsDisplayed);
});

// eslint-disable-next-line no-var
var PrintPreviewPrinterStatusTestCros = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_select_test_cros.js';
  }

  /** @override */
  get suiteName() {
    return printer_status_test_cros.suiteName;
  }

  /** @override */
  get featureList() {
    const kPrinterStatus = ['chromeos::features::kPrinterStatus'];
    const featureList = super.featureList || [];
    featureList.enabled = featureList.enabled ?
        featureList.enabled.concat(kPrinterStatus) :
        kPrinterStatus;
    return featureList;
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

// eslint-disable-next-line no-var
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
// eslint-disable-next-line no-var
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

TEST_F('PrintPreviewDestinationSelectTest', 'UpdateStatus', function() {
  this.runMochaTest(destination_select_test.TestNames.UpdateStatus);
});

TEST_F(
    'PrintPreviewDestinationSelectTest', 'UpdateStatusDeprecationWarnings',
    function() {
      this.runMochaTest(
          destination_select_test.TestNames.UpdateStatusDeprecationWarnings);
    });

TEST_F('PrintPreviewDestinationSelectTest', 'ChangeIcon', function() {
  this.runMochaTest(destination_select_test.TestNames.ChangeIcon);
});

TEST_F(
    'PrintPreviewDestinationSelectTest', 'ChangeIconDeprecationWarnings',
    function() {
      this.runMochaTest(
          destination_select_test.TestNames.ChangeIconDeprecationWarnings);
    });
GEN('#endif');

// eslint-disable-next-line no-var
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

TEST_F(
    'PrintPreviewDestinationSettingsTest', 'RecentDestinationsMissing',
    function() {
      this.runMochaTest(
          destination_settings_test.TestNames.RecentDestinationsMissing);
    });

TEST_F('PrintPreviewDestinationSettingsTest', 'SaveAsPdfRecent', function() {
  this.runMochaTest(destination_settings_test.TestNames.SaveAsPdfRecent);
});

TEST_F('PrintPreviewDestinationSettingsTest', 'GoogleDriveRecent', function() {
  this.runMochaTest(destination_settings_test.TestNames.GoogleDriveRecent);
});

TEST_F(
    'PrintPreviewDestinationSettingsTest', 'GoogleDriveAutoselect', function() {
      this.runMochaTest(
          destination_settings_test.TestNames.GoogleDriveAutoselect);
    });

TEST_F('PrintPreviewDestinationSettingsTest', 'SelectSaveAsPdf', function() {
  this.runMochaTest(destination_settings_test.TestNames.SelectSaveAsPdf);
});

TEST_F('PrintPreviewDestinationSettingsTest', 'SelectGoogleDrive', function() {
  this.runMochaTest(destination_settings_test.TestNames.SelectGoogleDrive);
});

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
    'PrintPreviewDestinationSettingsTest', 'TwoAccountsRecentDestinations',
    function() {
      this.runMochaTest(
          destination_settings_test.TestNames.TwoAccountsRecentDestinations);
    });

TEST_F(
    'PrintPreviewDestinationSettingsTest', 'UpdateRecentDestinations',
    function() {
      this.runMochaTest(
          destination_settings_test.TestNames.UpdateRecentDestinations);
    });

TEST_F(
    'PrintPreviewDestinationSettingsTest', 'ResetDestinationOnSignOut',
    function() {
      this.runMochaTest(
          destination_settings_test.TestNames.ResetDestinationOnSignOut);
    });

TEST_F('PrintPreviewDestinationSettingsTest', 'DisabledSaveAsPdf', function() {
  this.runMochaTest(destination_settings_test.TestNames.DisabledSaveAsPdf);
});

TEST_F('PrintPreviewDestinationSettingsTest', 'NoDestinations', function() {
  this.runMochaTest(destination_settings_test.TestNames.NoDestinations);
});

GEN('#if defined(OS_CHROMEOS)');
// eslint-disable-next-line no-var
var PrintPreviewDestinationSettingsTestCros = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_settings_test.js';
  }

  /** @override */
  get suiteName() {
    return destination_settings_test.suiteName;
  }

  /** @override */
  get featureList() {
    const kPrintSaveToDrive = ['chromeos::features::kPrintSaveToDrive'];
    const featureList = super.featureList || [];
    featureList.enabled = featureList.enabled ?
        featureList.enabled.concat(kPrintSaveToDrive) :
        kPrintSaveToDrive;
    return featureList;
  }
};

TEST_F(
    'PrintPreviewDestinationSettingsTestCros', 'EulaIsRetrieved', function() {
      this.runMochaTest(destination_settings_test.TestNames.EulaIsRetrieved);
    });

TEST_F(
    'PrintPreviewDestinationSettingsTestCros', 'DriveIsNotMounted', function() {
      this.runMochaTest(destination_settings_test.TestNames.DriveIsNotMounted);
    });
GEN('#endif');

// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
var PrintPreviewCopiesSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/copies_settings_test.js';
  }
};

TEST_F('PrintPreviewCopiesSettingsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewMediaSizeSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/media_size_settings_test.js';
  }
};

TEST_F('PrintPreviewMediaSizeSettingsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewDpiSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/dpi_settings_test.js';
  }
};

TEST_F('PrintPreviewDpiSettingsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewOtherOptionsSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/other_options_settings_test.js';
  }
};

TEST_F('PrintPreviewOtherOptionsSettingsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewLayoutSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/layout_settings_test.js';
  }
};

TEST_F('PrintPreviewLayoutSettingsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewColorSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/color_settings_test.js';
  }
};

TEST_F('PrintPreviewColorSettingsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewMarginsSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/margins_settings_test.js';
  }
};

TEST_F('PrintPreviewMarginsSettingsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewPagesPerSheetSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/pages_per_sheet_settings_test.js';
  }
};

TEST_F('PrintPreviewPagesPerSheetSettingsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewDuplexSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/duplex_settings_test.js';
  }
};

TEST_F('PrintPreviewDuplexSettingsTest', 'All', function() {
  mocha.run();
});

GEN('#if defined(OS_CHROMEOS)');
// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
var PrintPreviewUserManagerTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/user_manager_test.js';
  }
};

TEST_F('PrintPreviewUserManagerTest', 'All', function() {
  mocha.run();
});

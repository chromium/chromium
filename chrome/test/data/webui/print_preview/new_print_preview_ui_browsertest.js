// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Print Preview tests for the new UI. */

const ROOT_PATH = '../../../../../';

GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/common/chrome_features.h"');

function PrintPreviewSettingsSectionsTest() {}

const NewPrintPreviewTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/';
  }

  /** @override */
  get featureList() {
    return ['features::kNewPrintPreview', ''];
  }

  /** @override */
  get extraLibraries() {
    return PolymerTest.getLibraries(ROOT_PATH).concat([
      ROOT_PATH + 'ui/webui/resources/js/assert.js',
    ]);
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

PrintPreviewSettingsSectionsTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'settings_section_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return settings_sections_tests.suiteName;
  }
};

TEST_F('PrintPreviewSettingsSectionsTest', 'Copies', function() {
  this.runMochaTest(settings_sections_tests.TestNames.Copies);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'Layout', function() {
  this.runMochaTest(settings_sections_tests.TestNames.Layout);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'Color', function() {
  this.runMochaTest(settings_sections_tests.TestNames.Color);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'ColorSaveToDrive', function() {
  this.runMochaTest(settings_sections_tests.TestNames.ColorSaveToDrive);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'MediaSize', function() {
  this.runMochaTest(settings_sections_tests.TestNames.MediaSize);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'MediaSizeCustomNames', function() {
  this.runMochaTest(settings_sections_tests.TestNames.MediaSizeCustomNames);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'Margins', function() {
  this.runMochaTest(settings_sections_tests.TestNames.Margins);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'Dpi', function() {
  this.runMochaTest(settings_sections_tests.TestNames.Dpi);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'Scaling', function() {
  this.runMochaTest(settings_sections_tests.TestNames.Scaling);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'Other', function() {
  this.runMochaTest(settings_sections_tests.TestNames.Other);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'HeaderFooter', function() {
  this.runMochaTest(settings_sections_tests.TestNames.HeaderFooter);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetPages', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetPages);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetCopies', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetCopies);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetLayout', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetLayout);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetColor', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetColor);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetMediaSize', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetMediaSize);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetDpi', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetDpi);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetMargins', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetMargins);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetPagesPerSheet', function() {
  loadTimeData.overrideValues({pagesPerSheetEnabled: true});
  this.runMochaTest(settings_sections_tests.TestNames.SetPagesPerSheet);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetScaling', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetScaling);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'SetOther', function() {
  this.runMochaTest(settings_sections_tests.TestNames.SetOther);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'PresetCopies', function() {
  this.runMochaTest(settings_sections_tests.TestNames.PresetCopies);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'PresetDuplex', function() {
  this.runMochaTest(settings_sections_tests.TestNames.PresetDuplex);
});

TEST_F('PrintPreviewSettingsSectionsTest', 'ColorManaged', function() {
  this.runMochaTest(settings_sections_tests.TestNames.ColorManaged);
});

TEST_F(
    'PrintPreviewSettingsSectionsTest', 'DisableMarginsByPagesPerSheet',
    function() {
      loadTimeData.overrideValues({pagesPerSheetEnabled: true});
      this.runMochaTest(
          settings_sections_tests.TestNames.DisableMarginsByPagesPerSheet);
    });

PrintPreviewPolicyTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'policy_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return policy_tests.suiteName;
  }
};

TEST_F('PrintPreviewPolicyTest', 'EnableHeaderFooterByPref', function() {
  this.runMochaTest(policy_tests.TestNames.EnableHeaderFooterByPref);
});

TEST_F('PrintPreviewPolicyTest', 'DisableHeaderFooterByPref', function() {
  this.runMochaTest(policy_tests.TestNames.DisableHeaderFooterByPref);
});

TEST_F('PrintPreviewPolicyTest', 'EnableHeaderFooterByPolicy', function() {
  this.runMochaTest(policy_tests.TestNames.EnableHeaderFooterByPolicy);
});

TEST_F('PrintPreviewPolicyTest', 'DisableHeaderFooterByPolicy', function() {
  this.runMochaTest(policy_tests.TestNames.DisableHeaderFooterByPolicy);
});

PrintPreviewSettingsSelectTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/settings_select.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'print_preview_test_utils.js',
      'settings_select_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return settings_select_test.suiteName;
  }
};

TEST_F('PrintPreviewSettingsSelectTest', 'CustomMediaNames', function() {
  this.runMochaTest(settings_select_test.TestNames.CustomMediaNames);
});

PrintPreviewSelectBehaviorTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/select_behavior.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'select_behavior_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return select_behavior_test.suiteName;
  }
};

TEST_F('PrintPreviewSelectBehaviorTest', 'CallProcessSelectChange', function() {
  this.runMochaTest(select_behavior_test.TestNames.CallProcessSelectChange);
});

PrintPreviewBaseSettingsSectionTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/settings_section.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'base_settings_section_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return base_settings_section_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewBaseSettingsSectionTest', 'ManagedShowsEnterpriseIcon',
    function() {
      this.runMochaTest(
          base_settings_section_test.TestNames.ManagedShowsEnterpriseIcon);
    });

PrintPreviewNumberSettingsSectionTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/number_settings_section.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'number_settings_section_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return number_settings_section_test.suiteName;
  }
};

TEST_F('PrintPreviewNumberSettingsSectionTest', 'BlocksInvalidKeys',
    function() {
  this.runMochaTest(number_settings_section_test.TestNames.BlocksInvalidKeys);
});

PrintPreviewRestoreStateTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'restore_state_test.js',
    ]);
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

PrintPreviewModelTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/model.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'model_test.js',
    ]);
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

PrintPreviewPreviewGenerationTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'preview_generation_test.js',
    ]);
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

TEST_F('PrintPreviewPreviewGenerationTest', 'FitToPage', function() {
  this.runMochaTest(preview_generation_test.TestNames.FitToPage);
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
  loadTimeData.overrideValues({pagesPerSheetEnabled: true});
  this.runMochaTest(preview_generation_test.TestNames.PagesPerSheet);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'Scaling', function() {
  this.runMochaTest(preview_generation_test.TestNames.Scaling);
});

GEN('#if !defined(OS_WIN) && !defined(OS_MACOSX)');
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
      loadTimeData.overrideValues({pagesPerSheetEnabled: true});
      this.runMochaTest(
          preview_generation_test.TestNames.ChangeMarginsByPagesPerSheet);
    });

GEN('#if !defined(OS_CHROMEOS)');
PrintPreviewLinkContainerTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/link_container.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'link_container_test.js',
    ]);
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

GEN('#if defined(OS_MACOSX)');
TEST_F('PrintPreviewLinkContainerTest', 'OpenInPreviewLinkClick', function() {
  this.runMochaTest(link_container_test.TestNames.OpenInPreviewLinkClick);
});
GEN('#endif');  // defined(OS_MACOSX)

GEN('#if defined(OS_WIN) || defined(OS_MACOSX)');
PrintPreviewSystemDialogBrowserTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'system_dialog_browsertest.js',
    ]);
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
GEN('#endif');  // defined(OS_WIN) || defined(OS_MACOSX)

PrintPreviewInvalidSettingsBrowserTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      ROOT_PATH + 'ui/webui/resources/js/cr/event_target.js',
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'cloud_print_interface_stub.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'invalid_settings_browsertest.js',
    ]);
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
      loadTimeData.overrideValues({isEnterpriseManaged: false});
      this.runMochaTest(
          invalid_settings_browsertest.TestNames.InvalidCertificateError);
    });

TEST_F(
    'PrintPreviewInvalidSettingsBrowserTest',
    'InvalidCertificateErrorReselectDestination', function() {
      loadTimeData.overrideValues({isEnterpriseManaged: false});
      this.runMochaTest(invalid_settings_browsertest.TestNames
                            .InvalidCertificateErrorReselectDestination);
    });

PrintPreviewDestinationSelectTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'print_preview_test_utils.js',
      'destination_select_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return destination_select_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewDestinationSelectTest', 'SingleRecentDestination', function() {
      this.runMochaTest(
          destination_select_test.TestNames.SingleRecentDestination);
    });

TEST_F(
    'PrintPreviewDestinationSelectTest', 'MultipleRecentDestinations',
    function() {
      this.runMochaTest(
          destination_select_test.TestNames.MultipleRecentDestinations);
    });

TEST_F(
    'PrintPreviewDestinationSelectTest', 'MultipleRecentDestinationsOneRequest',
    function() {
      this.runMochaTest(destination_select_test.TestNames
                            .MultipleRecentDestinationsOneRequest);
    });

TEST_F(
    'PrintPreviewDestinationSelectTest', 'DefaultDestinationSelectionRules',
    function() {
      this.runMochaTest(
          destination_select_test.TestNames.DefaultDestinationSelectionRules);
    });

GEN('#if !defined(OS_CHROMEOS)');
TEST_F(
    'PrintPreviewDestinationSelectTest', 'SystemDefaultPrinterPolicy',
    function() {
      loadTimeData.overrideValues({useSystemDefaultPrinter: true});
      this.runMochaTest(
          destination_select_test.TestNames.SystemDefaultPrinterPolicy);
    });
GEN('#endif');

PrintPreviewDestinationDialogTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/destination_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      ROOT_PATH + 'ui/webui/resources/js/webui_listener_tracker.js',
      ROOT_PATH + 'ui/webui/resources/js/cr/event_target.js',
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'cloud_print_interface_stub.js',
      'native_layer_stub.js',
      'print_preview_test_utils.js',
      'destination_dialog_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return destination_dialog_test.suiteName;
  }
};

TEST_F('PrintPreviewDestinationDialogTest', 'PrinterList', function() {
  this.runMochaTest(destination_dialog_test.TestNames.PrinterList);
});

TEST_F(
    'PrintPreviewDestinationDialogTest', 'ShowProvisionalDialog', function() {
      this.runMochaTest(
          destination_dialog_test.TestNames.ShowProvisionalDialog);
    });

TEST_F('PrintPreviewDestinationDialogTest', 'ReloadPrinterList', function() {
  this.runMochaTest(destination_dialog_test.TestNames.ReloadPrinterList);
});

PrintPreviewAdvancedDialogTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/advanced_settings_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'advanced_dialog_test.js',
    ]);
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

TEST_F('PrintPreviewAdvancedDialogTest', 'AdvancedSettingsClose', function() {
  this.runMochaTest(advanced_dialog_test.TestNames.AdvancedSettingsClose);
});

TEST_F('PrintPreviewAdvancedDialogTest', 'AdvancedSettingsFilter', function() {
  this.runMochaTest(advanced_dialog_test.TestNames.AdvancedSettingsFilter);
});

PrintPreviewCustomMarginsTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/margin_control_container.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'custom_margins_test.js',
    ]);
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

PrintPreviewNewDestinationSearchTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/destination_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      ROOT_PATH + 'ui/webui/resources/js/webui_listener_tracker.js',
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'print_preview_test_utils.js',
      'destination_search_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return destination_search_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewNewDestinationSearchTest', 'ReceiveSuccessfulSetup',
    function() {
      this.runMochaTest(
          destination_search_test.TestNames.ReceiveSuccessfulSetup);
    });

GEN('#if defined(OS_CHROMEOS)');
TEST_F('PrintPreviewNewDestinationSearchTest', 'ResolutionFails', function() {
  this.runMochaTest(destination_search_test.TestNames.ResolutionFails);
});

TEST_F(
    'PrintPreviewNewDestinationSearchTest', 'ReceiveFailedSetup', function() {
      this.runMochaTest(destination_search_test.TestNames.ReceiveFailedSetup);
    });

TEST_F(
    'PrintPreviewNewDestinationSearchTest',
    'ReceiveSuccessfultSetupWithPolicies', function() {
      this.runMochaTest(destination_search_test.TestNames.ResolutionFails);
    });

GEN('#else');  // !defined(OS_CHROMEOS)
TEST_F(
    'PrintPreviewNewDestinationSearchTest', 'GetCapabilitiesFails', function() {
      this.runMochaTest(destination_search_test.TestNames.GetCapabilitiesFails);
    });
GEN('#endif');  // defined(OS_CHROMEOS)

TEST_F('PrintPreviewNewDestinationSearchTest', 'CloudKioskPrinter', function() {
  this.runMochaTest(destination_search_test.TestNames.CloudKioskPrinter);
});

PrintPreviewHeaderTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/header.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'header_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return header_test.suiteName;
  }
};

TEST_F('PrintPreviewHeaderTest', 'HeaderPrinterTypes', function() {
  this.runMochaTest(header_test.TestNames.HeaderPrinterTypes);
});

TEST_F('PrintPreviewHeaderTest', 'HeaderWithDuplex', function() {
  this.runMochaTest(header_test.TestNames.HeaderWithDuplex);
});

TEST_F('PrintPreviewHeaderTest', 'HeaderWithCopies', function() {
  this.runMochaTest(header_test.TestNames.HeaderWithCopies);
});

TEST_F('PrintPreviewHeaderTest', 'HeaderChangesForState', function() {
  this.runMochaTest(header_test.TestNames.HeaderChangesForState);
});

TEST_F('PrintPreviewHeaderTest', 'ButtonOrder', function() {
  this.runMochaTest(header_test.TestNames.ButtonOrder);
});

PrintPreviewDestinationItemTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/destination_list_item.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'print_preview_test_utils.js',
      'destination_item_test.js',
    ]);
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
  loadTimeData.overrideValues({isEnterpriseManaged: false});
  this.runMochaTest(destination_item_test.TestNames.BadCertificate);
});

TEST_F('PrintPreviewDestinationItemTest', 'QueryName', function() {
  this.runMochaTest(destination_item_test.TestNames.QueryName);
});

TEST_F('PrintPreviewDestinationItemTest', 'QueryDescription', function() {
  this.runMochaTest(destination_item_test.TestNames.QueryDescription);
});

PrintPreviewAdvancedItemTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/advanced_settings_item.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'print_preview_test_utils.js',
      'advanced_item_test.js',
    ]);
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

PrintPreviewDestinationListTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/destination_list.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'destination_list_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return destination_list_test.suiteName;
  }
};

TEST_F('PrintPreviewDestinationListTest', 'FilterDestinations', function() {
  this.runMochaTest(destination_list_test.TestNames.FilterDestinations);
});

TEST_F('PrintPreviewDestinationListTest', 'FireDestinationSelected',
    function() {
  this.runMochaTest(destination_list_test.TestNames.FireDestinationSelected);
});

PrintPreviewPrintButtonTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'print_button_test.js',
    ]);
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

PrintPreviewKeyEventTest = class extends NewPrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'key_event_test.js',
    ]);
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

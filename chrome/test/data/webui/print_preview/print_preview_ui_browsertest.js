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
    'PrintPreviewPrinterStatusTestCros', 'PrinterStatusUpdatesColor_FlagOff',
    function() {
      this.runMochaTest(
          printer_status_test_cros.TestNames.PrinterStatusUpdatesColor_FlagOff);
    });

TEST_F(
    'PrintPreviewPrinterStatusTestCros', 'PrinterStatusUpdatesColor_FlagOn',
    function() {
      this.runMochaTest(
          printer_status_test_cros.TestNames.PrinterStatusUpdatesColor_FlagOn);
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

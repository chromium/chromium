// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Print Preview interactive UI tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "build/build_config.h"');
GEN('#include "content/public/test/browser_test.h"');

const PrintPreviewInteractiveUITest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
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

var PrintPreviewButtonStripInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/button_strip_interactive_test.js';
  }

  /** @override */
  get suiteName() {
    return button_strip_interactive_test.suiteName;
  }
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if BUILDFLAG(IS_WIN)');
GEN('#define MAYBE_FocusPrintOnReady DISABLED_FocusPrintOnReady');
GEN('#else');
GEN('#define MAYBE_FocusPrintOnReady FocusPrintOnReady');
GEN('#endif');
TEST_F(
    'PrintPreviewButtonStripInteractiveTest', 'MAYBE_FocusPrintOnReady',
    function() {
      this.runMochaTest(
          button_strip_interactive_test.TestNames.FocusPrintOnReady);
    });

GEN('#if !BUILDFLAG(IS_CHROMEOS)');
var PrintPreviewDestinationDialogInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_dialog_interactive_test.js';
  }

  /** @override */
  get suiteName() {
    return destination_dialog_interactive_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewDestinationDialogInteractiveTest', 'FocusSearchBox',
    function() {
      this.runMochaTest(
          destination_dialog_interactive_test.TestNames.FocusSearchBox);
    });

TEST_F(
    'PrintPreviewDestinationDialogInteractiveTest', 'EscapeSearchBox',
    function() {
      this.runMochaTest(
          destination_dialog_interactive_test.TestNames.EscapeSearchBox);
    });
GEN('#else');

var PrintPreviewDestinationDialogCrosInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_dialog_cros_interactive_test.js';
  }

  /** @override */
  get suiteName() {
    return destination_dialog_cros_interactive_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewDestinationDialogCrosInteractiveTest', 'FocusSearchBox',
    function() {
      this.runMochaTest(
          destination_dialog_cros_interactive_test.TestNames.FocusSearchBox);
    });

TEST_F(
    'PrintPreviewDestinationDialogCrosInteractiveTest', 'EscapeSearchBox',
    function() {
      this.runMochaTest(
          destination_dialog_cros_interactive_test.TestNames.EscapeSearchBox);
    });
GEN('#endif');


var PrintPreviewPagesSettingsTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/pages_settings_test.js';
  }

  /** @override */
  get suiteName() {
    return pages_settings_test.suiteName;
  }
};

TEST_F('PrintPreviewPagesSettingsTest', 'ClearInput', function() {
  this.runMochaTest(pages_settings_test.TestNames.ClearInput);
});

TEST_F(
    'PrintPreviewPagesSettingsTest', 'InputNotDisabledOnValidityChange',
    function() {
      this.runMochaTest(
          pages_settings_test.TestNames.InputNotDisabledOnValidityChange);
    });

TEST_F(
    'PrintPreviewPagesSettingsTest', 'EnterOnInputTriggersPrint', function() {
      this.runMochaTest(
          pages_settings_test.TestNames.EnterOnInputTriggersPrint);
    });

var PrintPreviewNumberSettingsSectionInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/number_settings_section_interactive_test.js';
  }

  /** @override */
  get suiteName() {
    return number_settings_section_interactive_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewNumberSettingsSectionInteractiveTest', 'BlurResetsEmptyInput',
    function() {
      this.runMochaTest(number_settings_section_interactive_test.TestNames
                            .BlurResetsEmptyInput);
    });

var PrintPreviewScalingSettingsInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/scaling_settings_interactive_test.js';
  }

  /** @override */
  get suiteName() {
    return scaling_settings_interactive_test.suiteName;
  }
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if BUILDFLAG(IS_WIN)');
GEN('#define MAYBE_AutoFocusInput DISABLED_InputAutoFocus');
GEN('#else');
GEN('#define MAYBE_AutoFocusInput InputAutoFocus');
GEN('#endif');
TEST_F(
    'PrintPreviewScalingSettingsInteractiveTest', 'MAYBE_AutoFocusInput',
    function() {
      this.runMochaTest(
          scaling_settings_interactive_test.TestNames.AutoFocusInput);
    });

GEN('#if BUILDFLAG(IS_CHROMEOS)');
var PrintPreviewDestinationDropdownCrosTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/test_loader.html?module=print_preview/destination_dropdown_cros_test.js';
  }

  /** @override */
  get suiteName() {
    return destination_dropdown_cros_test.suiteName;
  }
};

TEST_F('PrintPreviewDestinationDropdownCrosTest', 'ClickCloses', function() {
  this.runMochaTest(destination_dropdown_cros_test.TestNames.ClickCloses);
});
GEN('#endif');

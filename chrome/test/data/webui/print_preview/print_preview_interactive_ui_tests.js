// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Print Preview interactive UI tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);
GEN('#include "services/network/public/cpp/features.h"');

const PrintPreviewInteractiveUITest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  /** @override */
  get featureList() {
    return {enabled: ['network::features::kOutOfBlinkCors']};
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
GEN('#if defined(OS_WIN)');
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

// eslint-disable-next-line no-var
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
    'PrintPreviewDestinationDialogInteractiveTest', 'FocusSearchBoxOnSignIn',
    function() {
      this.runMochaTest(
          destination_dialog_interactive_test.TestNames.FocusSearchBoxOnSignIn);
    });

TEST_F(
    'PrintPreviewDestinationDialogInteractiveTest', 'EscapeSearchBox',
    function() {
      this.runMochaTest(
          destination_dialog_interactive_test.TestNames.EscapeSearchBox);
    });

// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
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
GEN('#if defined(OS_WIN)');
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

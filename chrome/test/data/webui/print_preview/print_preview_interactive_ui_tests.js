// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Print Preview interactive UI tests. */

/** @const {string} Path to source root. */
const ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_interactive_ui_test.js']);
GEN('#include "chrome/common/chrome_features.h"');

const PrintPreviewInteractiveUITest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
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

PrintPreviewPrintHeaderInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/header.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      ROOT_PATH + 'chrome/test/data/webui/settings/test_util.js',
      'print_header_interactive_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return print_header_interactive_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewPrintHeaderInteractiveTest', 'FocusPrintOnReady', function() {
      this.runMochaTest(
          print_header_interactive_test.TestNames.FocusPrintOnReady);
    });

PrintPreviewDestinationDialogInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/destination_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      ROOT_PATH + 'chrome/test/data/webui/settings/test_util.js',
      ROOT_PATH + 'ui/webui/resources/js/webui_listener_tracker.js',
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'print_preview_test_utils.js',
      'destination_dialog_interactive_test.js',
    ]);
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

PrintPreviewPagesSettingsTest = class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/pages_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'pages_settings_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return pages_settings_test.suiteName;
  }
};

TEST_F('PrintPreviewPagesSettingsTest', 'ValidPageRanges', function() {
  this.runMochaTest(pages_settings_test.TestNames.ValidPageRanges);
});

TEST_F('PrintPreviewPagesSettingsTest', 'InvalidPageRanges', function() {
  this.runMochaTest(pages_settings_test.TestNames.InvalidPageRanges);
});

TEST_F('PrintPreviewPagesSettingsTest', 'NupChangesPages', function() {
  this.runMochaTest(pages_settings_test.TestNames.NupChangesPages);
});

TEST_F('PrintPreviewPagesSettingsTest', 'ClearInput', function() {
  this.runMochaTest(pages_settings_test.TestNames.ClearInput);
});

TEST_F('PrintPreviewPagesSettingsTest', 'TabOrder', function() {
  this.runMochaTest(pages_settings_test.TestNames.TabOrder);
});

TEST_F(
    'PrintPreviewPagesSettingsTest', 'InputNotDisabledOnValidityChange',
    function() {
      this.runMochaTest(
          pages_settings_test.TestNames.InputNotDisabledOnValidityChange);
    });

TEST_F('PrintPreviewPagesSettingsTest', 'IgnoreInputKeyEvents', function() {
  this.runMochaTest(pages_settings_test.TestNames.IgnoreInputKeyEvents);
});

TEST_F(
    'PrintPreviewPagesSettingsTest', 'EnterOnInputTriggersPrint', function() {
      this.runMochaTest(
          pages_settings_test.TestNames.EnterOnInputTriggersPrint);
    });

PrintPreviewNumberSettingsSectionInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/new/number_settings_section.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'number_settings_section_interactive_test.js',
    ]);
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

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for the Gaia action buttons.
 */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

var GaiaActionButtonsTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://chrome-signin/test_loader.html?module=chromeos/gaia_action_buttons/gaia_action_buttons_test.js';
  }

  get suiteName() {
    return gaia_action_buttons_test.suiteName;
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(this.suiteName, testName);
  }
};

TEST_F('GaiaActionButtonsTest', 'ButtonLabels', function() {
  this.runMochaTest(gaia_action_buttons_test.TestNames.ButtonLabels);
});

TEST_F('GaiaActionButtonsTest', 'EnabledEvents', function() {
  this.runMochaTest(gaia_action_buttons_test.TestNames.EnabledEvents);
});

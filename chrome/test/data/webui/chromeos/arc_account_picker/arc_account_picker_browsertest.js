// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for ARC account picker screen.
 */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

var ArcAccountPickerTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://chrome-signin/test_loader.html?module=chromeos/arc_account_picker/arc_account_picker_test.js';
  }

  get suiteName() {
    return arc_account_picker_test.suiteName;
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(this.suiteName, testName);
  }
};

TEST_F('ArcAccountPickerTest', 'EmptyAccountList', function() {
  this.runMochaTest(arc_account_picker_test.TestNames.EmptyAccountList);
});

TEST_F('ArcAccountPickerTest', 'AccountList', function() {
  this.runMochaTest(arc_account_picker_test.TestNames.AccountList);
});

TEST_F('ArcAccountPickerTest', 'AddAccount', function() {
  this.runMochaTest(arc_account_picker_test.TestNames.AddAccount);
});

TEST_F('ArcAccountPickerTest', 'MakeAvailableInArc', function() {
  this.runMochaTest(arc_account_picker_test.TestNames.MakeAvailableInArc);
});

TEST_F('ArcAccountPickerTest', 'LinkClick', function() {
  this.runMochaTest(arc_account_picker_test.TestNames.LinkClick);
});

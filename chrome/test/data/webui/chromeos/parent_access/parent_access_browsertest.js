// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Parent Access flow tests. */

GEN('#include "content/public/test/browser_test.h"');

var ParentAccessControllerTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return 'chrome://parent-access/test_loader.html?module=' +
        'chromeos/parent_access/parent_access_controller_test.js';
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(parent_access_controller_tests.suiteName, testName);
  }
};

TEST_F('ParentAccessControllerTest', 'ParentAccessResultFnCalled', function() {
  this.runMochaTest(
      parent_access_controller_tests.TestNames.ParentAccessResultFnCalled);
});

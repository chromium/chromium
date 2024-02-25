// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the EDU Coexistence flow tests. */

GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');
GEN('#include "ash/constants/ash_features.h"');

const EduCoexistenceTest = class extends PolymerTest {};

const tests = [
  ['App', 'edu_coexistence_app_test.js'],
  ['Controller', 'edu_coexistence_controller_test.js'],
  ['Ui', 'edu_coexistence_ui_test.js'],
];

tests.forEach(test => registerTest(...test));

/*
 * Add a `caseName` to a specific test to disable it i.e. 'DISABLED_All'
 * @param {string} testName
 * @param {string} module
 * @param {string} caseName
 */
function registerTest(testName, module, caseName) {
  const className = `EduCoexistence${testName}`;
  this[className] = class extends EduCoexistenceTest {
    /** @override */
    get browsePreload() {
      return `chrome://chrome-signin/test_loader.html` +
          `?module=chromeos/edu_coexistence/${module}`;
    }
  };
  TEST_F(className, caseName || 'All', () => mocha.run());
}

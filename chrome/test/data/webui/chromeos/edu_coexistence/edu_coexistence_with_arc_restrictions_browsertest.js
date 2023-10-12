// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');
GEN('#include "ash/constants/ash_features.h"');

// TODO(crbug.com/1347746): Merge this test suite with the EduCoexistenceTest
// after the feature is launched.
const EduCoexistenceTestWithArcRestrictions = class extends PolymerTest {
  get featureList() {
    return {
      enabled: [
        'ash::features::kLacrosOnly',
        'ash::features::kLacrosProfileMigrationForceOff',
      ],
    };
  }
};

const tests = [
  ['AppWithArcPicker', 'edu_coexistence_app_with_arc_picker_test.js'],
];

tests.forEach(test => registerTest(...test));

/*
 * Add a `caseName` to a specific test to disable it i.e. 'DISABLED_All'
 * @param {string} testName
 * @param {string} module
 * @param {string} caseName
 */
function registerTest(testName, module, caseName) {
  const className = `EduCoexistence${testName}_WithArcRestrictions`;
  this[className] = class extends EduCoexistenceTestWithArcRestrictions {
    /** @override */
    get browsePreload() {
      return `chrome://chrome-signin/test_loader.html` +
          `?module=chromeos/edu_coexistence/${module}&host=test`;
    }
  };
  TEST_F(className, caseName || 'All', () => mocha.run());
}

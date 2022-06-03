// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the EDU Coexistence flow tests. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

const EduCoexistenceTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overridden by subclasses';
  }

  get suiteName() {
    throw 'this is abstract and should be overridden by subclasses';
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(this.suiteName, testName);
  }
};

var EduCoexistenceAppTest = class extends EduCoexistenceTest {
  /** @override */
  get browsePreload() {
    return 'chrome://chrome-signin/test_loader.html?module=' +
        'chromeos/edu_coexistence/edu_coexistence_app_test.js';
  }

  /** @override */
  get suiteName() {
    return edu_coexistence_app_tests.suiteName;
  }
};

TEST_F('EduCoexistenceAppTest', 'InitOnline', function() {
  this.runMochaTest(edu_coexistence_app_tests.TestNames.InitOnline);
});

TEST_F('EduCoexistenceAppTest', 'InitOffline', function() {
  this.runMochaTest(edu_coexistence_app_tests.TestNames.InitOffline);
});

TEST_F('EduCoexistenceAppTest', 'ShowOffline', function() {
  this.runMochaTest(edu_coexistence_app_tests.TestNames.ShowOffline);
});

TEST_F('EduCoexistenceAppTest', 'ShowOnline', function() {
  this.runMochaTest(edu_coexistence_app_tests.TestNames.ShowOnline);
});

TEST_F('EduCoexistenceAppTest', 'ShowError', function() {
  this.runMochaTest(edu_coexistence_app_tests.TestNames.ShowError);
});

TEST_F('EduCoexistenceAppTest', 'DontSwitchViewIfDisplayingError', function() {
  this.runMochaTest(
      edu_coexistence_app_tests.TestNames.DontSwitchViewIfDisplayingError);
});

TEST_F(
    'EduCoexistenceAppTest', 'ShowErrorScreenImmediatelyOnLoadAbort',
    function() {
      this.runMochaTest(edu_coexistence_app_tests.TestNames
                            .ShowErrorScreenImmediatelyOnLoadAbort);
    });


var EduCoexistenceControllerTest = class extends EduCoexistenceTest {
  /** @override */
  get browsePreload() {
    return 'chrome://chrome-signin/test_loader.html?module=' +
        'chromeos/edu_coexistence/edu_coexistence_controller_test.js';
  }

  /** @override */
  get suiteName() {
    return edu_coexistence_controller_tests.suiteName;
  }
};

TEST_F('EduCoexistenceControllerTest', 'GetSigninTimeDelta', function() {
  this.runMochaTest(
      edu_coexistence_controller_tests.TestNames.GetSigninTimeDelta);
});

var EduCoexistenceUiTest = class extends EduCoexistenceTest {
  /** @override */
  get browsePreload() {
    return 'chrome://chrome-signin/test_loader.html?module=' +
        'chromeos/edu_coexistence/edu_coexistence_ui_test.js';
  }

  /** @override */
  get suiteName() {
    return edu_coexistence_ui_tests.suiteName;
  }
};

TEST_F('EduCoexistenceUiTest', 'DisableGaiaBackButtonAfterClick', function() {
  this.runMochaTest(
      edu_coexistence_ui_tests.TestNames.DisableGaiaBackButtonAfterClick);
});

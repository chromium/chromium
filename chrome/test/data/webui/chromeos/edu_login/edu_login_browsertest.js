// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the EDU login flow tests. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

const EduLoginTest = class extends PolymerTest {
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

// eslint-disable-next-line no-var
var EduLoginButtonTest = class extends EduLoginTest {
  /** @override */
  get browsePreload() {
    return 'chrome://chrome-signin/test_loader.html?module=chromeos/edu_login/edu_login_button_test.js';
  }

  /** @override */
  get suiteName() {
    return edu_login_button_tests.suiteName;
  }
};

TEST_F('EduLoginButtonTest', 'OkButtonProperties', function() {
  this.runMochaTest(edu_login_button_tests.TestNames.OkButtonProperties);
});

TEST_F('EduLoginButtonTest', 'NextButtonProperties', function() {
  this.runMochaTest(edu_login_button_tests.TestNames.NextButtonProperties);
});

TEST_F('EduLoginButtonTest', 'BackButtonProperties', function() {
  this.runMochaTest(edu_login_button_tests.TestNames.BackButtonProperties);
});

TEST_F('EduLoginButtonTest', 'OkButtonRtlIcon', function() {
  this.runMochaTest(edu_login_button_tests.TestNames.OkButtonRtlIcon);
});

TEST_F('EduLoginButtonTest', 'NextButtonRtlIcon', function() {
  this.runMochaTest(edu_login_button_tests.TestNames.NextButtonRtlIcon);
});

TEST_F('EduLoginButtonTest', 'BackButtonRtlIcon', function() {
  this.runMochaTest(edu_login_button_tests.TestNames.BackButtonRtlIcon);
});

// eslint-disable-next-line no-var
var EduLoginParentsTest = class extends EduLoginTest {
  /** @override */
  get browsePreload() {
    return 'chrome://chrome-signin/test_loader.html?module=chromeos/edu_login/edu_login_parents_test.js';
  }

  /** @override */
  get suiteName() {
    return edu_login_parents_tests.suiteName;
  }
};

TEST_F('EduLoginParentsTest', 'Initialize', function() {
  this.runMochaTest(edu_login_parents_tests.TestNames.Initialize);
});

TEST_F('EduLoginParentsTest', 'NextButton', function() {
  this.runMochaTest(edu_login_parents_tests.TestNames.NextButton);
});

TEST_F('EduLoginParentsTest', 'GoNext', function() {
  this.runMochaTest(edu_login_parents_tests.TestNames.GoNext);
});

TEST_F('EduLoginParentsTest', 'SelectedParent', function() {
  this.runMochaTest(edu_login_parents_tests.TestNames.SelectedParent);
});

TEST_F('EduLoginParentsTest', 'NoInternetError', function() {
  this.runMochaTest(edu_login_parents_tests.TestNames.NoInternetError);
});

TEST_F('EduLoginParentsTest', 'CannotAddAccountError', function() {
  this.runMochaTest(edu_login_parents_tests.TestNames.CannotAddAccountError);
});

var EduLoginParentSigninTest = class extends EduLoginTest {
  /** @override */
  get browsePreload() {
    return 'chrome://chrome-signin/test_loader.html?module=chromeos/edu_login/edu_login_parent_signin_test.js';
  }

  /** @override */
  get suiteName() {
    return edu_login_parent_signin_tests.suiteName;
  }
};

TEST_F('EduLoginParentSigninTest', 'Initialize', function() {
  this.runMochaTest(edu_login_parent_signin_tests.TestNames.Initialize);
});

TEST_F('EduLoginParentSigninTest', 'WrongPassword', function() {
  this.runMochaTest(edu_login_parent_signin_tests.TestNames.WrongPassword);
});

TEST_F('EduLoginParentSigninTest', 'ParentSigninSuccess', function() {
  this.runMochaTest(
      edu_login_parent_signin_tests.TestNames.ParentSigninSuccess);
});

TEST_F('EduLoginParentSigninTest', 'ShowHidePassword', function() {
  this.runMochaTest(edu_login_parent_signin_tests.TestNames.ShowHidePassword);
});

TEST_F('EduLoginParentSigninTest', 'ClearState', function() {
  this.runMochaTest(edu_login_parent_signin_tests.TestNames.ClearState);
});

var EduLoginSigninTest = class extends EduLoginTest {
  /** @override */
  get browsePreload() {
    return 'chrome://chrome-signin/test_loader.html?module=chromeos/edu_login/edu_login_signin_test.js';
  }

  /** @override */
  get suiteName() {
    return edu_login_signin_tests.suiteName;
  }
};

TEST_F('EduLoginSigninTest', 'Init', function() {
  this.runMochaTest(edu_login_signin_tests.TestNames.Init);
});

TEST_F('EduLoginSigninTest', 'WebUICallbacks', function() {
  this.runMochaTest(edu_login_signin_tests.TestNames.WebUICallbacks);
});

TEST_F('EduLoginSigninTest', 'AuthExtHostCallbacks', function() {
  this.runMochaTest(edu_login_signin_tests.TestNames.AuthExtHostCallbacks);
});

TEST_F('EduLoginSigninTest', 'GoBackInWebview', function() {
  this.runMochaTest(edu_login_signin_tests.TestNames.GoBackInWebview);
});

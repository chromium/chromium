// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Parent Access flow tests. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/ui/webui/ash/parent_access/parent_access_browsertest_base.h"');
GEN('#include "content/public/test/browser_test.h"');

var ParentAccessAfterTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://parent-access/test_loader.html?module=' +
        'chromeos/parent_access/parent_access_after_test.js&host=test';
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(parent_access_after_tests.suiteName, testName);
  }
}

TEST_F('ParentAccessAfterTest', 'TestApproveButton', function() {
  this.runMochaTest(parent_access_after_tests.TestNames.TestApproveButton);
});

TEST_F('ParentAccessAfterTest', 'TestDenyButton', function() {
  this.runMochaTest(parent_access_after_tests.TestNames.TestDenyButton);
});

var ParentAccessAppTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://parent-access/test_loader.html?module=' +
        'chromeos/parent_access/parent_access_app_test.js&host=test';
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(parent_access_app_tests.suiteName, testName);
  }
};

TEST_F('ParentAccessAppTest', 'TestShowWebApprovalsAfterFlow', function() {
  this.runMochaTest(
      parent_access_app_tests.TestNames.TestShowWebApprovalsAfterFlow);
});

TEST_F('ParentAccessAppTest', 'TestShowExtensionApprovalsFlow', function() {
  this.runMochaTest(
      parent_access_app_tests.TestNames.TestShowExtensionApprovalsFlow);
});

TEST_F(
    'ParentAccessAppTest', 'TestShowExtensionApprovalsDisabledScreen',
    function() {
      this.runMochaTest(parent_access_app_tests.TestNames
                            .TestShowExtensionApprovalsDisabledScreen);
    });

TEST_F('ParentAccessAppTest', 'TestShowErrorScreenOnOAuthFailure', function() {
  this.runMochaTest(
      parent_access_app_tests.TestNames.TestShowErrorScreenOnOAuthFailure);
});

TEST_F('ParentAccessAppTest', 'TestWebApprovalsOffline', function() {
  this.runMochaTest(parent_access_app_tests.TestNames.TestWebApprovalsOffline);
});

TEST_F('ParentAccessAppTest', 'TestErrorStateIsTerminal', function() {
  this.runMochaTest(parent_access_app_tests.TestNames.TestErrorStateIsTerminal);
});

var ParentAccessControllerTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return 'chrome://parent-access/test_loader.html?module=' +
        'chromeos/parent_access/parent_access_controller_test.js&host=test';
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

TEST_F(
    'ParentAccessControllerTest', 'ParentAccessCallbackReceivedFnCalled',
    function() {
      this.runMochaTest(parent_access_controller_tests.TestNames
                            .ParentAccessCallbackReceivedFnCalled);
    });

var ParentAccessUITest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://parent-access/test_loader.html?module=' +
        'chromeos/parent_access/parent_access_ui_test.js&host=test';
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(parent_access_ui_tests.suiteName, testName);
  }
};

TEST_F('ParentAccessUITest', 'TestIsAllowedRequest', function() {
  this.runMochaTest(parent_access_ui_tests.TestNames.TestIsAllowedRequest);
});

TEST_F('ParentAccessUITest', 'TestShouldReceiveAuthHeader', function() {
  this.runMochaTest(
      parent_access_ui_tests.TestNames.TestShouldReceiveAuthHeader);
});


var ParentAccessUIHandlerTest = class extends testing.Test {
  /** @override */
  get typedefCppFixture() {
    return 'MojoWebUIBrowserTest';
  }

  /** @override */
  get browsePreload() {
    return 'chrome://parent-access/test_loader.html?module=' +
        'chromeos/parent_access/parent_access_ui_handler_test.js&host=test';
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(parent_access_ui_handler_tests.suiteName, testName);
  }
};

TEST_F(
    'ParentAccessUIHandlerTest', 'TestOnParentAccessCallbackReceived',
    function() {
      this.runMochaTest(parent_access_ui_handler_tests.TestNames
                            .TestOnParentAccessCallbackReceived);
    });

var ParentAccessWebviewManagerTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://parent-access/test_loader.html?module=' +
        'chromeos/parent_access/webview_manager_test.js&host=test';
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(webview_manager_tests.suiteName, testName);
  }
};

TEST_F('ParentAccessWebviewManagerTest', 'AccessTokenTest', function() {
  this.runMochaTest(webview_manager_tests.TestNames.AccessTokenTest);
});

TEST_F('ParentAccessWebviewManagerTest', 'BlockAccessTokenTest', function() {
  this.runMochaTest(webview_manager_tests.TestNames.BlockAccessTokenTest);
});

TEST_F('ParentAccessWebviewManagerTest', 'AllowRequestFnTest', function() {
  this.runMochaTest(webview_manager_tests.TestNames.AllowRequestFnTest);
});

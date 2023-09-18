// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Parent Access flow tests. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/ui/webui/ash/parent_access/parent_access_browsertest_base.h"');
GEN('#include "content/public/test/browser_test.h"');

this.ParentAccessBrowserTest = class extends PolymerTest {};

const tests = [
  ['ExtensionApprovals', 'extension_approvals_test.js'],
  ['ParentAccessAfter', 'parent_access_after_test.js'],
  ['ParentAccessApp', 'parent_access_app_test.js'],
  ['ParentAccessController', 'parent_access_controller_test.js'],
  ['ParentAccessDisabled', 'parent_access_disabled_test.js'],
  ['ParentAccessUi', 'parent_access_ui_test.js'],
  ['ParentAccessUiHandler', 'parent_access_ui_handler_test.js'],
  ['WebviewManager', 'webview_manager_test.js'],
];

tests.forEach(test => registerTest(...test));

/*
 * Add a `caseName` to a specific test to disable it i.e. 'DISABLED_All'
 * @param {string} testName
 * @param {string} module
 * @param {string} caseName
 */
function registerTest(testName, module, caseName) {
  const className = `ParentAccess${testName}`;
  this[className] = class extends ParentAccessBrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://parent-access/test_loader.html` +
          `?module=chromeos/parent_access/${module}`;
    }
  };
  TEST_F(className, caseName || 'All', () => mocha.run());
}

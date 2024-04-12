// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Borealis Installer page.
 */
GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

this.BorealisInstallerBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://borealis-installer/';
  }
};

const tests = [
  ['App', 'borealis_installer_app_test.js'],
  ['ErrorDialog', 'borealis_installer_error_dialog_test.js'],
];

tests.forEach(test => registerTest(...test));

function registerTest(testName, module, caseName) {
  const className = `BorealisInstaller${testName}Test`;
  this[className] = class extends BorealisInstallerBrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://borealis-installer/test_loader.html?` +
          `module=chromeos/borealis_installer/${module}`;
    }
  };

  TEST_F(className, caseName || 'All', () => mocha.run());
}

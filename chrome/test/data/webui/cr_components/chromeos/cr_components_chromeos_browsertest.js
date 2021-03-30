// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer2 tests for components shared with OOBE. All Chrome OS
 * WebUI elements have been converted to Polymer3 except for OOBE, which uses a
 * polyfill for HTML imports.
 *
 * All remaining elements are tested on "chrome://oobe/login".
 */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

// Polymer 2 test list format:
//
// ['ModuleNameTest', 'module.js',
//   [<module.js dependency source list>]
// ]
// clang-format off

[
  ['NetworkSelect', 'network/network_select_test.js', []],
].forEach(test => registerTest('NetworkComponents', ...test));

[
  ['Integration', 'multidevice_setup/integration_test.js', [
    '../../test_browser_proxy.js',
    '../../fake_chrome_event.js',  // Necessary for
                                // fake_quick_unlock_private.js
    '../../settings/chromeos/fake_quick_unlock_private.js',
    '../../test_util.js',
    'multidevice_setup/setup_succeeded_page_test.js',
  ]],
  ['SetupSucceededPage', 'multidevice_setup/setup_succeeded_page_test.js', [
    '../../test_browser_proxy.js',
  ]],
  ['StartSetupPage', 'multidevice_setup/start_setup_page_test.js', [
    '../../test_browser_proxy.js',
    '../../test_util.js',
  ]],
].forEach(test => registerTest('MultiDeviceSetup', ...test));

// clang-format on

function registerTest(componentName, testName, module, deps) {
  const className = `${componentName}${testName}Test`;
  this[className] = class extends Polymer2DeprecatedTest {
    /** @override */
    get browsePreload() {
      return 'chrome://oobe/login';
    }

    /** @override */
    get extraLibraries() {
      return super.extraLibraries.concat(deps).concat(module);
    }

    /** @override */
    get featureList() {
      return {
        enabled: [
          'chromeos::features::kUpdatedCellularActivationUi',
        ],
      };
    }
  };

  TEST_F(className, 'All', () => mocha.run());
}

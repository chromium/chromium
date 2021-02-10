// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for MultiDevice unified setup WebUI. Chrome OS only. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

// clang-format off
[
  ['Integration', 'integration_test.js', []],
  ['SetupSucceededPage', 'setup_succeeded_page_test.js', []],
  ['StartSetupPage', 'start_setup_page_test.js', []],
]
    .forEach(
        test => registerTest('MultiDeviceSetup', ...test));
// clang-format on

function registerTest(componentName, testName, module, deps) {
  const className = `${componentName}${testName}Test`;
  this[className] = class extends Polymer2DeprecatedTest {
    /** @override */
    get browsePreload() {
      return `chrome://multidevice-setup/`;
    }

    /** @override */
    get extraLibraries() {
      return [
        ...Polymer2DeprecatedTest.prototype.extraLibraries,
        '../test_browser_proxy.js',
        '../fake_chrome_event.js',  // Necessary for
                                    // fake_quick_unlock_private.js
        '../settings/chromeos/fake_quick_unlock_private.js',
        '../test_util.js',
        'setup_succeeded_page_test.js',
      ];
    }
  };

  TEST_F(className, 'All', () => mocha.run());
}

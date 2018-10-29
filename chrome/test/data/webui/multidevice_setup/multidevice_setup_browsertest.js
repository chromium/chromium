// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for MultiDevice unified setup WebUI. */

GEN('#if defined(OS_CHROMEOS)');

/** @const {string} Path to source root. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * Test fixture for MultiDeviceSetup elements.
 * @constructor
 * @extends {PolymerTest}
 */
function MultiDeviceSetupBrowserTest() {}

MultiDeviceSetupBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload: 'chrome://multidevice-setup/',

  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    '../test_browser_proxy.js',
    '../fake_chrome_event.js',  // Necessary for fake_quick_unlock_private.js
    '../settings/fake_quick_unlock_private.js',
    '../settings/test_util.js',
    'integration_test.js',
    'setup_succeeded_page_test.js',
    'start_setup_page_test.js',
  ]),
};

TEST_F('MultiDeviceSetupBrowserTest', 'Integration', function() {
  multidevice_setup.registerIntegrationTests();
  mocha.run();
});

TEST_F('MultiDeviceSetupBrowserTest', 'SetupSucceededPage', function() {
  multidevice_setup.registerSetupSucceededPageTests();
  mocha.run();
});

TEST_F('MultiDeviceSetupBrowserTest', 'StartSetupPage', function() {
  multidevice_setup.registerStartSetupPageTests();
  mocha.run();
});

GEN('#endif');

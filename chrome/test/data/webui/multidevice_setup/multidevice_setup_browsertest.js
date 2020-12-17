// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for MultiDevice unified setup WebUI. Chrome OS only. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

/**
 * Test fixture for MultiDeviceSetup elements.
 * @constructor
 * @extends {PolymerTest}
 */
function MultiDeviceSetupBrowserTest() {}

MultiDeviceSetupBrowserTest.prototype = {
  __proto__: Polymer2DeprecatedTest.prototype,

  browsePreload: 'chrome://multidevice-setup/',

  extraLibraries: [
    ...Polymer2DeprecatedTest.prototype.extraLibraries,
    '../test_browser_proxy.js',
    '../fake_chrome_event.js',  // Necessary for fake_quick_unlock_private.js
    '../settings/chromeos/fake_quick_unlock_private.js',
    '../test_util.js',
    'integration_test.js',
    'setup_succeeded_page_test.js',
    'start_setup_page_test.js',
  ],
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

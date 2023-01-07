// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "content/public/test/browser_test.h"');

/**
 * TestFixture for Invalidations WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function InvalidationsWebUITest() {}

InvalidationsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the Invalidations page.
   */
  browsePreload:
      'chrome://invalidations/invalidations.html?module=invalidations/invalidations_test.js',

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],

  /** @override */
  isAsync: true,

  suiteName: 'invalidations_test',

  /** @param {string} testName The name of the test to run. */
  runMochaTest: function(testName) {
    runMochaTest(this.suiteName, testName);
  },
};

// Test that registering an invalidations appears properly on the textarea.
TEST_F('InvalidationsWebUITest', 'testRegisteringNewInvalidation', function() {
  this.runMochaTest(invalidations_test.TestNames.RegisterNewInvalidation);
});

// Test that changing the Invalidations Service state appears both in the
// span and in the textarea.
TEST_F('InvalidationsWebUITest', 'testChangingInvalidationsState', function() {
  this.runMochaTest(invalidations_test.TestNames.ChangeInvalidationsState);
});

// Test that objects ids appear on the table.
TEST_F('InvalidationsWebUITest', 'testRegisteringNewIds', function() {
  this.runMochaTest(invalidations_test.TestNames.RegisterNewIds);
});

// Test that registering new handlers appear on the website.
TEST_F('InvalidationsWebUITest', 'testUpdatingRegisteredHandlers', function() {
  this.runMochaTest(invalidations_test.TestNames.UpdateRegisteredHandlers);
});

// Test that an object showing internal state is correctly displayed.
TEST_F('InvalidationsWebUITest', 'testUpdatingInternalDisplay', function() {
  this.runMochaTest(invalidations_test.TestNames.UpdateInternalDisplay);
});

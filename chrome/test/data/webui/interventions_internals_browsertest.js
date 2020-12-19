// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for interventions_internals.js
 */

GEN('#include "content/public/test/browser_test.h"');

/**
 * Test fixture for InterventionsInternals WebUI testing.
 * @constructor
 * @extends testing.Test
 */
function InterventionsInternalsUITest() {
  window.setupFnResolver = new PromiseResolver();
}

InterventionsInternalsUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the options page and call preLoad().
   * @override
   */
  browsePreload: 'chrome://interventions-internals',

  /** @override */
  isAsync: true,

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
    '//ui/webui/resources/js/assert.js',
    '//ui/webui/resources/js/promise_resolver.js',
  ],

  preLoad: function() {
    window.setupFn = function() {
      return window.setupFnResolver.promise;
    };
  },
};

// Helper for loading the Mocha test file as a JS module. Not using
// test_loader.html, as the test code needs to be loaded in the context of the
// entire UI.
function loadTestModule() {
  const scriptPolicy =
      window.trustedTypes.createPolicy('interventions-internals-test-script', {
        createScriptURL: () => 'chrome://test/interventions_internals_test.js',
      });
  const s = document.createElement('script');
  s.type = 'module';
  s.src = scriptPolicy.createScriptURL('');
  document.body.appendChild(s);
  return new Promise(function(resolve, reject) {
    s.addEventListener('load', () => resolve());
  });
}

['GetPreviewsEnabled',
 'GetPreviewsFlagsDetails',
 'LogNewMessage',
 'LogNewMessageWithLongUrl',
 'LogNewMessageWithNoUrl',
 'LogNewMessagePageIdZero',
 'LogNewMessageNewPageId',
 'LogNewMessageExistedPageId',
 'LogNewMessageExistedPageIdGroupToTopOfTable',
 'AddNewBlocklistedHost',
 'HostAlreadyBlocklisted',
 'UpdateUserBlocklisted',
 'OnBlocklistCleared',
 'ClearLogMessageOnBlocklistCleared',
 'OnECTChanged',
 'OnBlocklistIgnoreChange',
].forEach(name => {
  TEST_F('InterventionsInternalsUITest', name, function() {
    loadTestModule().then(() => {
      runMochaTest('InterventionsInternalsUITest', name);
    });
  });
});

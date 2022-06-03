// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Framework for running JavaScript tests of Polymer elements.
 */

/**
 * Test fixture for Polymer element testing.
 * @constructor
 * @extends testing.Test
 */
function PolymerTest() {}

PolymerTest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Navigate to a WebUI to satisfy BrowserTest conditions. Override to load a
   * more useful WebUI.
   * @override
   */
  browsePreload: 'chrome://chrome-urls/',

  /**
   * The mocha adapter assumes all tests are async.
   * @override
   * @final
   */
  isAsync: true,
};

/**
 * Test fixture for Polymer2 elements testing (deprecated).
 * TODO(crbug.com/965770): Delete once all remaining Polymer2 UIs have been
 * migrated.
 * @constructor
 * @extends PolymerTest
 */
function Polymer2DeprecatedTest() {}

Polymer2DeprecatedTest.prototype = {
  __proto__: PolymerTest.prototype,

  /**
   * Files that need not be compiled.
   * @override
   */
  extraLibraries: [
    '//ui/webui/resources/js/cr.js',
    '//ui/webui/resources/js/assert.js',
    '//ui/webui/resources/js/promise_resolver.js',
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
    '//third_party/polymer/v1_0/components-chromium/iron-test-helpers/' +
        'mock-interactions.js',
  ],
};

/**
 * Imports the HTML file.
 * @param {string} src The URL to load.
 * @return {!Promise} A promise that is resolved/rejected on success/failure.
 */
PolymerTest.importHtml = function(src) {
  var link = document.createElement('link');
  link.rel = 'import';
  var promise = new Promise(function(resolve, reject) {
    link.onload = resolve;
    link.onerror = reject;
  });
  link.href = src;
  document.head.appendChild(link);
  return promise;
};

/**
 * Removes all content from the body. In a vulcanized build, this retains the
 * inlined tags so stylesheets and dom-modules are not discarded.
 */
PolymerTest.clearBody = function() {
  // Save the div where vulcanize inlines content before clearing the page.
  var vulcanizeDiv =
      document.querySelector('body > div[hidden][by-polymer-bundler]');
  document.body.innerHTML = '';
  if (vulcanizeDiv) {
    document.body.appendChild(vulcanizeDiv);
  }
};

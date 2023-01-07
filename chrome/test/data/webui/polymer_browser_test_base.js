// Copyright 2015 The Chromium Authors
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

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

  /**
   * Old style a11y checks are obsolete. See ../a11y/accessibility_test.js for
   * the new suggested way.
   * @override
   */
  runAccessibilityChecks: false,

  /**
   * Files that need not be compiled.
   * @override
   */
  extraLibraries: [
    '//ui/webui/resources/js/cr.js',
    '//ui/webui/resources/js/promise_resolver.js',
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
    '//third_party/polymer/v1_0/components-chromium/iron-test-helpers/' +
        'mock-interactions.js',
  ],

  /** @override */
  setUp: function() {
    testing.Test.prototype.setUp.call(this);

    // List of imported URLs for debugging purposes.
    PolymerTest.importUrls_ = [];
    PolymerTest.scriptUrls_ = [];

    // Importing a URL like "chrome://settings/foo" redirects to the base
    // ("chrome://settings") page, which due to how browsePreload works can
    // result in duplicate imports. Wrap document.registerElement so failures
    // caused by re-registering Polymer elements are caught; otherwise Chrome
    // simply throws "Script error" which is unhelpful.
    var originalRegisterElement = document.registerElement;
    document.registerElement = function() {
      try {
        return originalRegisterElement.apply(document, arguments);
      } catch (e) {
        var msg =
            'If the call to document.registerElement failed because a type ' +
            'is already registered, perhaps you have loaded a script twice. ' +
            'Incorrect resource URLs can redirect to base WebUI pages; make ' +
            'sure the following URLs are correct and unique:\n';
        for (var i = 0; i < PolymerTest.importUrls_.length; i++) {
          msg += '  ' + PolymerTest.importUrls_[i] + '\n';
        }
        for (var i = 0; i < PolymerTest.scriptUrls_.length; i++) {
          msg += '  ' + PolymerTest.scriptUrls_[i] + '\n';
        }
        console.error(msg);

        // Mocha will handle the error.
        throw e;
      }
    };
  },
};

/**
 * Imports the HTML file.
 * @param {string} src The URL to load.
 * @return {!Promise} A promise that is resolved/rejected on success/failure.
 */
PolymerTest.importHtml = function(src) {
  PolymerTest.importUrls_.push(src);
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
 * Loads the script file.
 * @param {string} src The URL to load.
 * @return {!Promise} A promise that is resolved/rejected on success/failure.
 */
PolymerTest.loadScript = function(src) {
  PolymerTest.scriptUrls_.push(src);
  var script = document.createElement('script');
  var promise = new Promise(function(resolve, reject) {
    script.onload = resolve;
    script.onerror = reject;
  });
  script.src = src;
  document.head.appendChild(script);
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

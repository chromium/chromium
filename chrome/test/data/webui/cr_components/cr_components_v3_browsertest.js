// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 components. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

/** Test fixture for shared Polymer 3 components. */
// eslint-disable-next-line no-var
var CrComponentsV3BrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://dummyurl';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }

  /** @override */
  setUp() {
    PolymerTest.prototype.setUp.call(this);
    // We aren't loading the main document.
    this.accessibilityAuditConfig.ignoreSelectors('humanLangMissing', 'html');
  }
};

// eslint-disable-next-line no-var
var CrComponentsManagedFootnoteV3Test =
    class extends CrComponentsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_components/managed_footnote_test.m.js';
  }
};

TEST_F('CrComponentsManagedFootnoteV3Test', 'All', function() {
  mocha.run();
});

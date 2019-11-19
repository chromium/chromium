// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer components. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * Test fixture for shared Polymer components.
 * @constructor
 * @extends {PolymerTest}
 */
function CrComponentsBrowserTest() {}

CrComponentsBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  get browsePreload() {
    throw 'subclasses should override to load a WebUI page that includes it.';
  },

  /** @override */
  runAccessibilityChecks: true,

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    // We aren't loading the main document.
    this.accessibilityAuditConfig.ignoreSelectors('humanLangMissing', 'html');
  },
};

/**
 * @constructor
 * @extends {CrComponentsBrowserTest}
 */
function CrComponentsManagedFootnoteTest() {}

CrComponentsManagedFootnoteTest.prototype = {
  __proto__: CrComponentsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_components/managed_footnote/managed_footnote.html',

  /** @override */
  extraLibraries: CrComponentsBrowserTest.prototype.extraLibraries.concat([
    'managed_footnote_test.js',
  ]),

  /** @override */
  get suiteName() {
    return managed_footnote_test.suiteName;
  }
};

TEST_F('CrComponentsManagedFootnoteTest', 'Hidden', function() {
  runMochaTest(this.suiteName, managed_footnote_test.TestNames.Hidden);
});

TEST_F('CrComponentsManagedFootnoteTest', 'LoadTimeDataBrowser', function() {
  runMochaTest(
      this.suiteName, managed_footnote_test.TestNames.LoadTimeDataBrowser);
});

TEST_F('CrComponentsManagedFootnoteTest', 'Events', function() {
  runMochaTest(this.suiteName, managed_footnote_test.TestNames.Events);
});

GEN('#if defined(OS_CHROMEOS)');

TEST_F('CrComponentsManagedFootnoteTest', 'LoadTimeDataDevice', function() {
  runMochaTest(
      this.suiteName, managed_footnote_test.TestNames.LoadTimeDataDevice);
});

/**
 * @constructor
 * @extends {CrComponentsBrowserTest}
 */
function CrComponentsNetworkConfigTest() {}

CrComponentsNetworkConfigTest.prototype = {
  __proto__: CrComponentsBrowserTest.prototype,

  /** @override */

  browsePreload: 'chrome://internet-config-dialog',

  /** @override */
  extraLibraries: CrComponentsBrowserTest.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/assert.js',
    '//ui/webui/resources/js/promise_resolver.js',
    '../fake_chrome_event.js',
    '../chromeos/networking_private_constants.js',
    '../chromeos/fake_network_config_mojom.js',
    '../chromeos/cr_onc_strings.js',
    'network_config_test.js',
  ]),
};

TEST_F('CrComponentsNetworkConfigTest', 'All', function() {
  mocha.run();
});

GEN('#endif');

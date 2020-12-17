// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer components. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

/**
 * Test fixture for shared Polymer components.
 * @constructor
 * @extends {PolymerTest}
 */
function CrComponentsBrowserTest() {}

CrComponentsBrowserTest.prototype = {
  __proto__: Polymer2DeprecatedTest.prototype,

  /** @override */
  get browsePreload() {
    throw 'subclasses should override to load a WebUI page that includes it.';
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

GEN('#endif');

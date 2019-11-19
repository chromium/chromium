// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * Test fixture for FindShortcutBehavior.
 * @constructor
 * @extends {PolymerTest}
 */
function FindShortcutBehaviorTest() {}

FindShortcutBehaviorTest.prototype = {
  __proto__: PolymerTest.prototype,

  /**
   * Preload a module that depends on both cr-dialog and FindShortcutBehavior.
   * cr-dialog is used in the tests.
   * @override
   */
  browsePreload: 'chrome://resources/html/find_shortcut_behavior.html',

  /** @override */
  extraLibraries: [
    ...PolymerTest.prototype.extraLibraries,
    '//ui/webui/resources/js/util.js',
    'test_util.js',
    'find_shortcut_behavior_test.js',
  ],
};

TEST_F('FindShortcutBehaviorTest', 'All', function() {
  mocha.run();
});

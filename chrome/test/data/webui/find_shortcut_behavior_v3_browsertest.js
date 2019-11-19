// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * Test fixture for FindShortcutBehavior.
 * @constructor
 * @extends {PolymerTest}
 */
var FindShortcutBehaviorV3Test = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=find_shortcut_behavior_test.m.js';
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }
};

TEST_F('FindShortcutBehaviorV3Test', 'All', function() {
  mocha.run();
});

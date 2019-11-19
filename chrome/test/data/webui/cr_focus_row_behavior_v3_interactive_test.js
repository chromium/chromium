// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

/**
 * Test fixture for FocusRowBehavior.
 * @constructor
 * @extends {PolymerInteractiveUITest}
 */
var CrFocusRowBehaviorV3Test = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_focus_row_behavior_test.m.js';
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

TEST_F('CrFocusRowBehaviorV3Test', 'FocusTest', function() {
  mocha.run();
});

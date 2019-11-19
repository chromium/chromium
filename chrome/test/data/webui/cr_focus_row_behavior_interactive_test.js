// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

/**
 * Test fixture for FocusRowBehavior.
 * @constructor
 * @extends {PolymerInteractiveUITest}
 */
function CrFocusRowBehaviorTest() {}

CrFocusRowBehaviorTest.prototype = {
  __proto__: PolymerInteractiveUITest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/html/cr/ui/focus_row_behavior.html',

  /** @override */
  extraLibraries: [
    ...PolymerTest.prototype.extraLibraries,
    '//ui/webui/resources/js/util.js',
    'cr_focus_row_behavior_test.js',
    'test_util.js',
  ],

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    // We aren't loading the main document.
    this.accessibilityAuditConfig.ignoreSelectors('humanLangMissing', 'html');
  },
};

TEST_F('CrFocusRowBehaviorTest', 'FocusTest', function() {
  mocha.run();
});

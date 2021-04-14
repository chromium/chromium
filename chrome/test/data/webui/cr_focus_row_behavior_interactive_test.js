// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');

/**
 * Test fixture for FocusRowBehavior.
 * @constructor
 * @extends {PolymerInteractiveUITest}
 */
var CrFocusRowBehaviorV3Test = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=cr_focus_row_behavior_test.js';
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }
};

TEST_F('CrFocusRowBehaviorV3Test', 'FocusTest', function() {
  mocha.run();
});

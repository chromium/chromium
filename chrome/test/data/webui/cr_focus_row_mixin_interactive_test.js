// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');

/**
 * Test fixture for FocusRowMixin.
 * @constructor
 * @extends {PolymerInteractiveUITest}
 */
var CrFocusRowMixinTest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=cr_focus_row_mixin_test.js';
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }
};

TEST_F('CrFocusRowMixinTest', 'FocusTest', function() {
  mocha.run();
});

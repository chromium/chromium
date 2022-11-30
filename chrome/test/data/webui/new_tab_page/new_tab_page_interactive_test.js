// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI new tab page page. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');

class NewTabPageInteractiveTest extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
  }
}

var NewTabPageCustomizeDialogFocusTest =
    class extends NewTabPageInteractiveTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/customize_dialog_focus_test.js';
  }
};

TEST_F('NewTabPageCustomizeDialogFocusTest', 'All', function() {
  mocha.run();
});

var NewTabPageDoodleShareDialogFocusTest =
    class extends NewTabPageInteractiveTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/doodle_share_dialog_focus_test.js';
  }
};

TEST_F('NewTabPageDoodleShareDialogFocusTest', 'All', function() {
  mocha.run();
});

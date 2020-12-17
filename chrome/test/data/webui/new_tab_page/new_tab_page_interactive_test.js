// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI new tab page page. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');

class NewTabPageInteractiveTest extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  }
}

// eslint-disable-next-line no-var
var NewTabPageMostVisitedFocusTest = class extends NewTabPageInteractiveTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/most_visited_focus_test.js';
  }
};

TEST_F('NewTabPageMostVisitedFocusTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
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

// eslint-disable-next-line no-var
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

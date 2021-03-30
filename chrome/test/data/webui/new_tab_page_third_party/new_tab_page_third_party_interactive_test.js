// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI new tab page page. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');

class NewTabPageThirdPartyInteractiveTest extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  }
}

// eslint-disable-next-line no-var
var NewTabPageThirdPartyMostVisitedFocusTest =
    class extends NewTabPageThirdPartyInteractiveTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page-third-party/test_loader.html?module=new_tab_page_third_party/most_visited_focus_test.js';
  }
};

TEST_F('NewTabPageThirdPartyMostVisitedFocusTest', 'All', function() {
  mocha.run();
});

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI tab search. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "content/public/test/browser_test.h"');

var SidePanelBookmarksListInteractiveUITest =
    class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=side_panel/bookmarks/bookmarks_list_interactive_ui_test.js';
  }
};

TEST_F('SidePanelBookmarksListInteractiveUITest', 'All', function() {
  mocha.run();
});

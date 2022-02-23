// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI tab search. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "components/reading_list/features/reading_list_switches.h"');
GEN('#include "content/public/test/browser_test.h"');

// eslint-disable-next-line no-var
var SidePanelBookmarksListInteractiveUITest =
    class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=read_later/side_panel/bookmarks_list_interactive_ui_test.js&host=webui-test';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'features::kSidePanel',
        'reading_list::switches::kReadLater',
      ]
    };
  }
};

TEST_F('SidePanelBookmarksListInteractiveUITest', 'All', function() {
  mocha.run();
});

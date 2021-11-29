// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI read later. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "components/reading_list/features/reading_list_switches.h"');
GEN('#include "content/public/test/browser_test.h"');

/* eslint-disable no-var */

class SidePanelBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
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
}

var SidePanelAppTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=read_later/side_panel/side_panel_app_test.js&host=webui-test';
  }
};

TEST_F('SidePanelAppTest', 'All', function() {
  mocha.run();
});

var SidePanelBookmarksListTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=read_later/side_panel/bookmarks_list_test.js&host=webui-test';
  }
};

TEST_F('SidePanelBookmarksListTest', 'All', function() {
  mocha.run();
});


var SidePanelBookmarkFolderTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=read_later/side_panel/bookmark_folder_test.js&host=webui-test';
  }
};

TEST_F('SidePanelBookmarkFolderTest', 'All', function() {
  mocha.run();
});


var SidePanelBookmarksDragManagerTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=read_later/side_panel/bookmarks_drag_manager_test.js&host=webui-test';
  }
};

TEST_F('SidePanelBookmarksDragManagerTest', 'All', function() {
  mocha.run();
});

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI read later. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "ui/accessibility/accessibility_features.h"');

class SidePanelBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
  }
}

var SidePanelAppTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=side_panel/side_panel_app_test.js';
  }

  /** @override */
  get featureList() {
    return {disabled: ['features::kUnifiedSidePanel']};
  }
};

TEST_F('SidePanelAppTest', 'All', function() {
  mocha.run();
});

var SidePanelBookmarksListTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=side_panel/bookmarks/bookmarks_list_test.js';
  }
};

TEST_F('SidePanelBookmarksListTest', 'All', function() {
  mocha.run();
});

var SidePanelPowerBookmarksListTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=side_panel/bookmarks/power_bookmarks_list_test.js';
  }
};

TEST_F('SidePanelPowerBookmarksListTest', 'All', function() {
  mocha.run();
});

// TODO(crbug.com/1396268): Flaky on Mac. Re-enable this test.
GEN('#if BUILDFLAG(IS_MAC)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');

var ShoppingListTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=side_panel/bookmarks/commerce/shopping_list_test.js';
  }
};

TEST_F('ShoppingListTest', 'MAYBE_All', function() {
  mocha.run();
});

var SidePanelBookmarkFolderTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=side_panel/bookmarks/bookmark_folder_test.js';
  }
};

TEST_F('SidePanelBookmarkFolderTest', 'All', function() {
  mocha.run();
});


var SidePanelBookmarksDragManagerTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=side_panel/bookmarks/bookmarks_drag_manager_test.js';
  }
};

TEST_F('SidePanelBookmarksDragManagerTest', 'All', function() {
  mocha.run();
});

var ReadingListAppTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=side_panel/reading_list/reading_list_app_test.js';
  }
};

TEST_F('ReadingListAppTest', 'All', function() {
  mocha.run();
});

var ReadAnythingAppTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=side_panel/read_anything/read_anything_app_test.js';
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kReadAnything']};
  }
};

TEST_F('ReadAnythingAppTest', 'All', function() {
  mocha.run();
});

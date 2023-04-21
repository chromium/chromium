// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI read later. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');
GEN('#include "components/user_notes/user_notes_features.h"');
GEN('#include "components/power_bookmarks/core/power_bookmark_features.h"');

class SidePanelBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
  }
}

var SidePanelBookmarksListTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks-side-panel.top-chrome/test_loader.html?module=side_panel/bookmarks/bookmarks_list_test.js';
  }
};

TEST_F('SidePanelBookmarksListTest', 'All', function() {
  mocha.run();
});

var SidePanelPowerBookmarksContextMenuTest =
    class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks-side-panel.top-chrome/test_loader.html?module=side_panel/bookmarks/power_bookmarks_context_menu_test.js';
  }
};

TEST_F('SidePanelPowerBookmarksContextMenuTest', 'All', function() {
  mocha.run();
});

var SidePanelPowerBookmarksEditDialogTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks-side-panel.top-chrome/test_loader.html?module=side_panel/bookmarks/power_bookmarks_edit_dialog_test.js';
  }
};

TEST_F('SidePanelPowerBookmarksEditDialogTest', 'All', function() {
  mocha.run();
});

var SidePanelPowerBookmarksListTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks-side-panel.top-chrome/test_loader.html?module=side_panel/bookmarks/power_bookmarks_list_test.js';
  }
};

TEST_F('SidePanelPowerBookmarksListTest', 'All', function() {
  mocha.run();
});

var SidePanelPowerBookmarksServiceTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks-side-panel.top-chrome/test_loader.html?module=side_panel/bookmarks/power_bookmarks_service_test.js';
  }
};

TEST_F('SidePanelPowerBookmarksServiceTest', 'All', function() {
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
    return 'chrome://bookmarks-side-panel.top-chrome/test_loader.html?module=side_panel/bookmarks/commerce/shopping_list_test.js';
  }
};

TEST_F('ShoppingListTest', 'MAYBE_All', function() {
  mocha.run();
});

var SidePanelBookmarkFolderTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks-side-panel.top-chrome/test_loader.html?module=side_panel/bookmarks/bookmark_folder_test.js';
  }
};

TEST_F('SidePanelBookmarkFolderTest', 'All', function() {
  mocha.run();
});


var SidePanelBookmarksDragManagerTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks-side-panel.top-chrome/test_loader.html?module=side_panel/bookmarks/bookmarks_drag_manager_test.js';
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

var UserNotesAppTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://user-notes-side-panel.top-chrome/test_loader.html?module=side_panel/user_notes/app_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled:
          ['user_notes::kUserNotes', 'power_bookmarks::kPowerBookmarkBackend']
    };
  }
};

TEST_F('UserNotesAppTest', 'All', function() {
  mocha.run();
});

var UserNoteOverviewsListTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://user-notes-side-panel.top-chrome/test_loader.html?module=side_panel/user_notes/user_note_overviews_list_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled:
          ['user_notes::kUserNotes', 'power_bookmarks::kPowerBookmarkBackend']
    };
  }
};

TEST_F('UserNoteOverviewsListTest', 'All', function() {
  mocha.run();
});

var UserNotesListTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://user-notes-side-panel.top-chrome/test_loader.html?module=side_panel/user_notes/user_notes_list_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled:
          ['user_notes::kUserNotes', 'power_bookmarks::kPowerBookmarkBackend']
    };
  }
};

TEST_F('UserNotesListTest', 'All', function() {
  mocha.run();
});

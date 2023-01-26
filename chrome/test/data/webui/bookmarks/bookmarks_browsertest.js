// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the bookmarks page.
 */
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/build_config.h"');
GEN('#include "chrome/browser/ui/webui/bookmarks/bookmarks_browsertest.h"');
GEN('#include "content/public/test/browser_test.h"');

const BookmarksBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/';
  }

  /** @override */
  get typedefCppFixture() {
    return 'BookmarksBrowserTest';
  }
};

var BookmarksActionsTest = class extends BookmarksBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/actions_test.js';
  }
};

TEST_F('BookmarksActionsTest', 'All', function() {
  mocha.run();
});

var BookmarksAppTest = class extends BookmarksBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/app_test.js';
  }
};

TEST_F('BookmarksAppTest', 'All', function() {
  mocha.run();
});

var BookmarksCommandManagerTest = class extends BookmarksBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/command_manager_test.js';
  }
};

// https://crbug.com/1010381: Flaky.
TEST_F('BookmarksCommandManagerTest', 'DISABLED_All', function() {
  mocha.run();
});

var BookmarksEditDialogTest = class extends BookmarksBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/edit_dialog_test.js';
  }
};

TEST_F('BookmarksEditDialogTest', 'All', function() {
  mocha.run();
});

var BookmarksItemTest = class extends BookmarksBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/item_test.js';
  }
};

TEST_F('BookmarksItemTest', 'All', function() {
  mocha.run();
});

var BookmarksListTest = class extends BookmarksBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/list_test.js';
  }
};

TEST_F('BookmarksListTest', 'All', function() {
  mocha.run();
});

var BookmarksReducersTest = class extends BookmarksBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/reducers_test.js';
  }
};

TEST_F('BookmarksReducersTest', 'All', function() {
  mocha.run();
});

var BookmarksRouterTest = class extends BookmarksBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/router_test.js';
  }
};

TEST_F('BookmarksRouterTest', 'All', function() {
  mocha.run();
});

var BookmarksFolderNodeTest = class extends BookmarksBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/folder_node_test.js';
  }
};

TEST_F('BookmarksFolderNodeTest', 'All', function() {
  mocha.run();
});

var BookmarksPolicyTest = class extends BookmarksBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/policy_test.js';
  }
};

TEST_F('BookmarksPolicyTest', 'All', function() {
  mocha.run();
});

var BookmarksStoreTest = class extends BookmarksBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/store_test.js';
  }
};

TEST_F('BookmarksStoreTest', 'All', function() {
  mocha.run();
});

var BookmarksToolbarTest = class extends BookmarksBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/toolbar_test.js';
  }
};

TEST_F('BookmarksToolbarTest', 'All', function() {
  mocha.run();
});

var BookmarksUtilTest = class extends BookmarksBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/util_test.js';
  }
};

TEST_F('BookmarksUtilTest', 'All', function() {
  mocha.run();
});

var BookmarksExtensionAPITest = class extends BookmarksBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/extension_api_test.js';
  }

  /** @override */
  testGenPreamble() {
    GEN('SetupExtensionAPITest();');
  }
};

TEST_F('BookmarksExtensionAPITest', 'All', function() {
  mocha.run();
});

var BookmarksExtensionAPIEditDisabledTest = class extends BookmarksBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks/test_loader.html?module=bookmarks/extension_api_test_edit_disabled.js';
  }

  /** @override */
  testGenPreamble() {
    GEN('SetupExtensionAPIEditDisabledTest();');
  }
};

TEST_F('BookmarksExtensionAPIEditDisabledTest', 'All', function() {
  mocha.run();
});
